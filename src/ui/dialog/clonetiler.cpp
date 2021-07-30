// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * Clone tiling dialog
 */
/* Authors:
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Romain de Bossoreille
 *
 * Copyright (C) 2004-2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "clonetiler.h"

#include <glibmm/i18n.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/combobox.h>
#include <gtkmm/grid.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/sizegroup.h>

#include <2geom/transforms.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "filter-chemistry.h"
#include "inkscape.h"
#include "message-stack.h"
#include "unclump.h"
#include "verbs.h"

#include "display/cairo-utils.h"
#include "display/drawing-context.h"
#include "display/drawing.h"

#include "ui/icon-loader.h"

#include "object/sp-item.h"
#include "object/sp-namedview.h"
#include "object/sp-root.h"
#include "object/sp-use.h"

#include "ui/icon-names.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-menu.h"

#include "svg/svg-color.h"
#include "svg/svg.h"

using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;

namespace Inkscape {
namespace UI {

namespace Widget {
/**
 * Simple extension of Gtk::CheckButton, which adds a flag
 * to indicate whether the box should be unticked when reset
 */
class CheckButtonInternal : public Gtk::CheckButton {
  private:
    bool _uncheckable = false;
  public:
    CheckButtonInternal() = default;

    CheckButtonInternal(const Glib::ustring &label)
        : Gtk::CheckButton(label)
    {}

    void set_uncheckable(const bool val = true) { _uncheckable = val; }
    bool get_uncheckable() const { return _uncheckable; }
};
}

namespace Dialog {

#define SB_MARGIN 1
#define VB_MARGIN 4

static Glib::ustring const prefs_path = "/dialogs/clonetiler/";

static Inkscape::Drawing *trace_drawing = nullptr;
static unsigned trace_visionkey;
static gdouble trace_zoom;
static SPDocument *trace_doc = nullptr;

CloneTiler::CloneTiler()
    : DialogBase("/dialogs/clonetiler/", "CloneTiler")
    , table_row_labels(nullptr)
{
    set_spacing(0);

    {
        auto prefs = Inkscape::Preferences::get();

        auto mainbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 4));
        mainbox->set_homogeneous(false);
        mainbox->set_border_width(6);

        pack_start(*mainbox, true, true, 0);

        nb = Gtk::manage(new Gtk::Notebook());
        mainbox->pack_start(*nb, false, false, 0);


        // Symmetry
        {
            auto vb = new_tab(nb, _("_Symmetry"));

        /* TRANSLATORS: For the following 17 symmetry groups, see
             * http://www.bib.ulb.ac.be/coursmath/doc/17.htm (visual examples);
             * http://www.clarku.edu/~djoyce/wallpaper/seventeen.html (English vocabulary); or
             * http://membres.lycos.fr/villemingerard/Geometri/Sym1D.htm (French vocabulary).
             */
            struct SymGroups {
                gint group;
                Glib::ustring label;
            } const sym_groups[] = {
                // TRANSLATORS: "translation" means "shift" / "displacement" here.
                {TILE_P1, _("<b>P1</b>: simple translation")},
                {TILE_P2, _("<b>P2</b>: 180&#176; rotation")},
                {TILE_PM, _("<b>PM</b>: reflection")},
                // TRANSLATORS: "glide reflection" is a reflection and a translation combined.
                //  For more info, see http://mathforum.org/sum95/suzanne/symsusan.html
                {TILE_PG, _("<b>PG</b>: glide reflection")},
                {TILE_CM, _("<b>CM</b>: reflection + glide reflection")},
                {TILE_PMM, _("<b>PMM</b>: reflection + reflection")},
                {TILE_PMG, _("<b>PMG</b>: reflection + 180&#176; rotation")},
                {TILE_PGG, _("<b>PGG</b>: glide reflection + 180&#176; rotation")},
                {TILE_CMM, _("<b>CMM</b>: reflection + reflection + 180&#176; rotation")},
                {TILE_P4, _("<b>P4</b>: 90&#176; rotation")},
                {TILE_P4M, _("<b>P4M</b>: 90&#176; rotation + 45&#176; reflection")},
                {TILE_P4G, _("<b>P4G</b>: 90&#176; rotation + 90&#176; reflection")},
                {TILE_P3, _("<b>P3</b>: 120&#176; rotation")},
                {TILE_P31M, _("<b>P31M</b>: reflection + 120&#176; rotation, dense")},
                {TILE_P3M1, _("<b>P3M1</b>: reflection + 120&#176; rotation, sparse")},
                {TILE_P6, _("<b>P6</b>: 60&#176; rotation")},
                {TILE_P6M, _("<b>P6M</b>: reflection + 60&#176; rotation")},
            };

            gint current = prefs->getInt(prefs_path + "symmetrygroup", 0);

            // Add a new combo box widget with the list of symmetry groups to the vbox
            auto combo = Gtk::manage(new Gtk::ComboBoxText());
            combo->set_tooltip_text(_("Select one of the 17 symmetry groups for the tiling"));

            // Hack to add markup support
            auto cell_list = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo->gobj()));
            gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo->gobj()),
                                           GTK_CELL_RENDERER(cell_list->data),
                                           "markup", 0, nullptr);

            for (const auto & sg : sym_groups) {
                // Add the description of the symgroup to a new row
                combo->append(sg.label);
            }

            vb->pack_start(*combo, false, false, SB_MARGIN);

            combo->set_active(current);
            combo->signal_changed().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::symgroup_changed), combo));
        }

        table_row_labels = Gtk::SizeGroup::create(Gtk::SIZE_GROUP_HORIZONTAL);

        // Shift
        {
            auto vb = new_tab(nb, _("S_hift"));

            auto table = table_x_y_rand (3);
            vb->pack_start(*table, false, false, 0);

            // X
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                    // TRANSLATORS: "shift" means: the tiles will be shifted (offset) horizontally by this amount
                    // xgettext:no-c-format
                l->set_markup(_("<b>Shift X:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 2, 1);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                   _("Horizontal shift per row (in % of tile width)"), "shiftx_per_j",
                   -10000, 10000, "%");
                table_attach (table, l, 0, 2, 2);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                   _("Horizontal shift per column (in % of tile width)"), "shiftx_per_i",
                   -10000, 10000, "%");
                table_attach (table, l, 0, 2, 3);
            }

            {
                auto l = spinbox (_("Randomize the horizontal shift by this percentage"), "shiftx_rand",
                                                   0, 1000, "%");
                table_attach (table, l, 0, 2, 4);
            }

            // Y
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                    // TRANSLATORS: "shift" means: the tiles will be shifted (offset) vertically by this amount
                    // xgettext:no-c-format
                l->set_markup(_("<b>Shift Y:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 3, 1);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Vertical shift per row (in % of tile height)"), "shifty_per_j",
                                                   -10000, 10000, "%");
                table_attach (table, l, 0, 3, 2);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Vertical shift per column (in % of tile height)"), "shifty_per_i",
                                                   -10000, 10000, "%");
                table_attach (table, l, 0, 3, 3);
            }

            {
                auto l = spinbox (
                                                   _("Randomize the vertical shift by this percentage"), "shifty_rand",
                                                   0, 1000, "%");
                table_attach (table, l, 0, 3, 4);
            }

            // Exponent
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Exponent:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 4, 1);
            }

            {
                auto l = spinbox (
                                                   _("Whether rows are spaced evenly (1), converge (<1) or diverge (>1)"), "shifty_exp",
                                                   0, 10, "", true);
                table_attach (table, l, 0, 4, 2);
            }

            {
                auto l = spinbox (
                                                   _("Whether columns are spaced evenly (1), converge (<1) or diverge (>1)"), "shiftx_exp",
                                                   0, 10, "", true);
                table_attach (table, l, 0, 4, 3);
            }

            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Alternate" is a verb here
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 5, 1);
            }

            {
                auto l = checkbox (_("Alternate the sign of shifts for each row"), "shifty_alternate");
                table_attach (table, l, 0, 5, 2);
            }

            {
                auto l = checkbox (_("Alternate the sign of shifts for each column"), "shiftx_alternate");
                table_attach (table, l, 0, 5, 3);
            }

            { // Cumulate
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Cumulate" is a verb here
                l->set_markup(_("<small>Cumulate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 6, 1);
            }

            {
                auto l = checkbox (_("Cumulate the shifts for each row"), "shifty_cumulate");
                table_attach (table, l, 0, 6, 2);
            }

            {
                auto l = checkbox (_("Cumulate the shifts for each column"), "shiftx_cumulate");
                table_attach (table, l, 0, 6, 3);
            }

            { // Exclude tile width and height in shift
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Cumulate" is a verb here
                l->set_markup(_("<small>Exclude tile:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 7, 1);
            }

            {
                auto l = checkbox (_("Exclude tile height in shift"), "shifty_excludeh");
                table_attach (table, l, 0, 7, 2);
            }

            {
                auto l = checkbox (_("Exclude tile width in shift"), "shiftx_excludew");
                table_attach (table, l, 0, 7, 3);
            }

        }


        // Scale
        {
            auto vb = new_tab(nb, _("Sc_ale"));

            auto table = table_x_y_rand(2);
            vb->pack_start(*table, false, false, 0);

            // X
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Scale X:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 2, 1);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Horizontal scale per row (in % of tile width)"), "scalex_per_j",
                                                   -100, 1000, "%");
                table_attach (table, l, 0, 2, 2);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Horizontal scale per column (in % of tile width)"), "scalex_per_i",
                                                   -100, 1000, "%");
                table_attach (table, l, 0, 2, 3);
            }

            {
                auto l = spinbox (_("Randomize the horizontal scale by this percentage"), "scalex_rand",
                                                   0, 1000, "%");
                table_attach (table, l, 0, 2, 4);
            }

            // Y
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Scale Y:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 3, 1);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Vertical scale per row (in % of tile height)"), "scaley_per_j",
                                                   -100, 1000, "%");
                table_attach (table, l, 0, 3, 2);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Vertical scale per column (in % of tile height)"), "scaley_per_i",
                                                   -100, 1000, "%");
                table_attach (table, l, 0, 3, 3);
            }

            {
                auto l = spinbox (_("Randomize the vertical scale by this percentage"), "scaley_rand",
                                                   0, 1000, "%");
                table_attach (table, l, 0, 3, 4);
            }

            // Exponent
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Exponent:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 4, 1);
            }

            {
                auto l = spinbox (_("Whether row scaling is uniform (1), converge (<1) or diverge (>1)"), "scaley_exp",
                                                   0, 10, "", true);
                table_attach (table, l, 0, 4, 2);
            }

            {
                auto l = spinbox (_("Whether column scaling is uniform (1), converge (<1) or diverge (>1)"), "scalex_exp",
                                                   0, 10, "", true);
                table_attach (table, l, 0, 4, 3);
            }

            // Logarithmic (as in logarithmic spiral)
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Base:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 5, 1);
            }

            {
                auto l = spinbox (_("Base for a logarithmic spiral: not used (0), converge (<1), or diverge (>1)"), "scaley_log",
                                                   0, 10, "", false);
                table_attach (table, l, 0, 5, 2);
            }

            {
                auto l = spinbox (_("Base for a logarithmic spiral: not used (0), converge (<1), or diverge (>1)"), "scalex_log",
                                                   0, 10, "", false);
                table_attach (table, l, 0, 5, 3);
            }

            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Alternate" is a verb here
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 6, 1);
            }

            {
                auto l = checkbox (_("Alternate the sign of scales for each row"), "scaley_alternate");
                table_attach (table, l, 0, 6, 2);
            }

            {
                auto l = checkbox (_("Alternate the sign of scales for each column"), "scalex_alternate");
                table_attach (table, l, 0, 6, 3);
            }

            { // Cumulate
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Cumulate" is a verb here
                l->set_markup(_("<small>Cumulate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 7, 1);
            }

            {
                auto l = checkbox (_("Cumulate the scales for each row"), "scaley_cumulate");
                table_attach (table, l, 0, 7, 2);
            }

            {
                auto l = checkbox (_("Cumulate the scales for each column"), "scalex_cumulate");
                table_attach (table, l, 0, 7, 3);
            }

        }


        // Rotation
        {
            auto vb = new_tab(nb, _("_Rotation"));

            auto table = table_x_y_rand (1);
            vb->pack_start(*table, false, false, 0);

            // Angle
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Angle:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 2, 1);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Rotate tiles by this angle for each row"), "rotate_per_j",
                                                   -180, 180, "&#176;");
                table_attach (table, l, 0, 2, 2);
            }

            {
                auto l = spinbox (
                    // xgettext:no-c-format
                                                   _("Rotate tiles by this angle for each column"), "rotate_per_i",
                                                   -180, 180, "&#176;");
                table_attach (table, l, 0, 2, 3);
            }

            {
                auto l = spinbox (_("Randomize the rotation angle by this percentage"), "rotate_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 2, 4);
            }

            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Alternate" is a verb here
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 3, 1);
            }

            {
                auto l = checkbox (_("Alternate the rotation direction for each row"), "rotate_alternatej");
                table_attach (table, l, 0, 3, 2);
            }

            {
                auto l = checkbox (_("Alternate the rotation direction for each column"), "rotate_alternatei");
                table_attach (table, l, 0, 3, 3);
            }

            { // Cumulate
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Cumulate" is a verb here
                l->set_markup(_("<small>Cumulate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 4, 1);
            }

            {
                auto l = checkbox (_("Cumulate the rotation for each row"), "rotate_cumulatej");
                table_attach (table, l, 0, 4, 2);
            }

            {
                auto l = checkbox (_("Cumulate the rotation for each column"), "rotate_cumulatei");
                table_attach (table, l, 0, 4, 3);
            }

        }


        // Blur and opacity
        {
            auto vb = new_tab(nb, _("_Blur & opacity"));

            auto table = table_x_y_rand(1);
            vb->pack_start(*table, false, false, 0);


            // Blur
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Blur:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 2, 1);
            }

            {
                auto l = spinbox (_("Blur tiles by this percentage for each row"), "blur_per_j",
                                                   0, 100, "%");
                table_attach (table, l, 0, 2, 2);
            }

            {
                auto l = spinbox (_("Blur tiles by this percentage for each column"), "blur_per_i",
                                                   0, 100, "%");
                table_attach (table, l, 0, 2, 3);
            }

            {
                auto l = spinbox (_("Randomize the tile blur by this percentage"), "blur_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 2, 4);
            }

            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Alternate" is a verb here
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 3, 1);
            }

            {
                auto l = checkbox (_("Alternate the sign of blur change for each row"), "blur_alternatej");
                table_attach (table, l, 0, 3, 2);
            }

            {
                auto l = checkbox (_("Alternate the sign of blur change for each column"), "blur_alternatei");
                table_attach (table, l, 0, 3, 3);
            }



            // Dissolve
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>Opacity:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 4, 1);
            }

            {
                auto l = spinbox (_("Decrease tile opacity by this percentage for each row"), "opacity_per_j",
                                                   0, 100, "%");
                table_attach (table, l, 0, 4, 2);
            }

            {
                auto l = spinbox (_("Decrease tile opacity by this percentage for each column"), "opacity_per_i",
                                                   0, 100, "%");
                table_attach (table, l, 0, 4, 3);
            }

            {
                auto l = spinbox (_("Randomize the tile opacity by this percentage"), "opacity_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 4, 4);
            }

            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                // TRANSLATORS: "Alternate" is a verb here
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 5, 1);
            }

            {
                auto l = checkbox (_("Alternate the sign of opacity change for each row"), "opacity_alternatej");
                table_attach (table, l, 0, 5, 2);
            }

            {
                auto l = checkbox (_("Alternate the sign of opacity change for each column"), "opacity_alternatei");
                table_attach (table, l, 0, 5, 3);
            }
        }


        // Color
        {
            auto vb = new_tab(nb, _("Co_lor"));

            {
                auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
                hb->set_homogeneous(false);

                auto l = Gtk::manage(new Gtk::Label(_("Initial color: ")));
                hb->pack_start(*l, false, false, 0);

                guint32 rgba = 0x000000ff | sp_svg_read_color (prefs->getString(prefs_path + "initial_color").data(), 0x000000ff);
                color_picker = new Inkscape::UI::Widget::ColorPicker (*new Glib::ustring(_("Initial color of tiled clones")), *new Glib::ustring(_("Initial color for clones (works only if the original has unset fill or stroke or on spray tool in copy mode)")), rgba, false);
                color_changed_connection = color_picker->connectChanged(sigc::mem_fun(*this, &CloneTiler::on_picker_color_changed));

                hb->pack_start(*color_picker, false, false, 0);

                vb->pack_start(*hb, false, false, 0);
            }


            auto table = table_x_y_rand(3);
            vb->pack_start(*table, false, false, 0);

            // Hue
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>H:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 2, 1);
            }

            {
                auto l = spinbox (_("Change the tile hue by this percentage for each row"), "hue_per_j",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 2, 2);
            }

            {
                auto l = spinbox (_("Change the tile hue by this percentage for each column"), "hue_per_i",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 2, 3);
            }

            {
                auto l = spinbox (_("Randomize the tile hue by this percentage"), "hue_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 2, 4);
            }


            // Saturation
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>S:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 3, 1);
            }

            {
                auto l = spinbox (_("Change the color saturation by this percentage for each row"), "saturation_per_j",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 3, 2);
            }

            {
                auto l = spinbox (_("Change the color saturation by this percentage for each column"), "saturation_per_i",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 3, 3);
            }

            {
                auto l = spinbox (_("Randomize the color saturation by this percentage"), "saturation_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 3, 4);
            }

            // Lightness
            {
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<b>L:</b>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 4, 1);
            }

            {
                auto l = spinbox (_("Change the color lightness by this percentage for each row"), "lightness_per_j",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 4, 2);
            }

            {
                auto l = spinbox (_("Change the color lightness by this percentage for each column"), "lightness_per_i",
                                                   -100, 100, "%");
                table_attach (table, l, 0, 4, 3);
            }

            {
                auto l = spinbox (_("Randomize the color lightness by this percentage"), "lightness_rand",
                                                   0, 100, "%");
                table_attach (table, l, 0, 4, 4);
            }


            { // alternates
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup(_("<small>Alternate:</small>"));
                l->set_xalign(0.0);
                table_row_labels->add_widget(*l);
                table_attach (table, l, 1, 5, 1);
            }

            {
                auto l = checkbox (_("Alternate the sign of color changes for each row"), "color_alternatej");
                table_attach (table, l, 0, 5, 2);
            }

            {
                auto l = checkbox (_("Alternate the sign of color changes for each column"), "color_alternatei");
                table_attach (table, l, 0, 5, 3);
            }

        }

        // Trace
        {
            auto vb = new_tab(nb, _("_Trace"));
        {
            auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
            hb->set_border_width(4);
            hb->set_homogeneous(false);
            vb->pack_start(*hb, false, false, 0);

            _b = Gtk::manage(new UI::Widget::CheckButtonInternal(_("Trace the drawing under the clones/sprayed items")));
            _b->set_uncheckable();
            bool old = prefs->getBool(prefs_path + "dotrace");
            _b->set_active(old);
            _b->set_tooltip_text(_("For each clone/sprayed item, pick a value from the drawing in its location and apply it"));
            hb->pack_start(*_b, false, false, 0);
            _b->signal_toggled().connect(sigc::mem_fun(*this, &CloneTiler::do_pick_toggled));
        }

        {
            auto vvb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0));
            vvb->set_homogeneous(false);
            vb->pack_start(*vvb, false, false, 0);
            _dotrace = vvb;

            {
                auto frame = Gtk::manage(new Gtk::Frame(_("1. Pick from the drawing:")));
                vvb->pack_start(*frame, false, false, 0);

                auto table = Gtk::manage(new Gtk::Grid());
                table->set_row_spacing(4);
                table->set_column_spacing(6);
                table->set_border_width(4);
                frame->add(*table);

                Gtk::RadioButtonGroup rb_group;
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("Color")));
                    radio->set_tooltip_text(_("Pick the visible color and opacity"));
                    table_attach(table, radio, 0.0, 1, 1);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_COLOR));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_COLOR);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("Opacity")));
                    radio->set_tooltip_text(_("Pick the total accumulated opacity"));
                    table_attach (table, radio, 0.0, 2, 1);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_OPACITY));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_OPACITY);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("R")));
                    radio->set_tooltip_text(_("Pick the Red component of the color"));
                    table_attach (table, radio, 0.0, 1, 2);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_R));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_R);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("G")));
                    radio->set_tooltip_text(_("Pick the Green component of the color"));
                    table_attach (table, radio, 0.0, 2, 2);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_G));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_G);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("B")));
                    radio->set_tooltip_text(_("Pick the Blue component of the color"));
                    table_attach (table, radio, 0.0, 3, 2);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_B));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_B);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, C_("Clonetiler color hue", "H")));
                    radio->set_tooltip_text(_("Pick the hue of the color"));
                    table_attach (table, radio, 0.0, 1, 3);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_H));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_H);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, C_("Clonetiler color saturation", "S")));
                    radio->set_tooltip_text(_("Pick the saturation of the color"));
                    table_attach (table, radio, 0.0, 2, 3);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_S));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_S);
                }
                {
                    auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, C_("Clonetiler color lightness", "L")));
                    radio->set_tooltip_text(_("Pick the lightness of the color"));
                    table_attach (table, radio, 0.0, 3, 3);
                    radio->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_switched), PICK_L));
                    radio->set_active(prefs->getInt(prefs_path + "pick", 0) == PICK_L);
                }

            }

            {
                auto frame = Gtk::manage(new Gtk::Frame(_("2. Tweak the picked value:")));
                vvb->pack_start(*frame, false, false, VB_MARGIN);

                auto table = Gtk::manage(new Gtk::Grid());
                table->set_row_spacing(4);
                table->set_column_spacing(6);
                table->set_border_width(4);
                frame->add(*table);

                {
                    auto l = Gtk::manage(new Gtk::Label(""));
                    l->set_markup(_("Gamma-correct:"));
                    table_attach (table, l, 1.0, 1, 1);
                }
                {
                    auto l = spinbox (_("Shift the mid-range of the picked value upwards (>0) or downwards (<0)"), "gamma_picked",
                                                       -10, 10, "");
                    table_attach (table, l, 0.0, 1, 2);
                }

                {
                    auto l = Gtk::manage(new Gtk::Label(""));
                    l->set_markup(_("Randomize:"));
                    table_attach (table, l, 1.0, 1, 3);
                }
                {
                    auto l = spinbox (_("Randomize the picked value by this percentage"), "rand_picked",
                                                       0, 100, "%");
                    table_attach (table, l, 0.0, 1, 4);
                }

                {
                    auto l = Gtk::manage(new Gtk::Label(""));
                    l->set_markup(_("Invert:"));
                    table_attach (table, l, 1.0, 2, 1);
                }
                {
                    auto l = checkbox (_("Invert the picked value"), "invert_picked");
                    table_attach (table, l, 0.0, 2, 2);
                }
            }

            {
                auto frame = Gtk::manage(new Gtk::Frame(_("3. Apply the value to the clones':")));
                vvb->pack_start(*frame, false, false, 0);

                auto table = Gtk::manage(new Gtk::Grid());
                table->set_row_spacing(4);
                table->set_column_spacing(6);
                table->set_border_width(4);
                frame->add(*table);

                {
                    auto b = Gtk::manage(new Gtk::CheckButton(_("Presence")));
                    bool old = prefs->getBool(prefs_path + "pick_to_presence", true);
                    b->set_active(old);
                    b->set_tooltip_text(_("Each clone is created with the probability determined by the picked value in that point"));
                    table_attach (table, b, 0.0, 1, 1);
                    b->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_to), b, "pick_to_presence"));
                }

                {
                    auto b = Gtk::manage(new Gtk::CheckButton(_("Size")));
                    bool old = prefs->getBool(prefs_path + "pick_to_size");
                    b->set_active(old);
                    b->set_tooltip_text(_("Each clone's size is determined by the picked value in that point"));
                    table_attach (table, b, 0.0, 2, 1);
                    b->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_to), b, "pick_to_size"));
                }

                {
                    auto b = Gtk::manage(new Gtk::CheckButton(_("Color")));
                    bool old = prefs->getBool(prefs_path + "pick_to_color", false);
                    b->set_active(old);
                    b->set_tooltip_text(_("Each clone is painted by the picked color (the original must have unset fill or stroke)"));
                    table_attach (table, b, 0.0, 1, 2);
                    b->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_to), b, "pick_to_color"));
                }

                {
                    auto b = Gtk::manage(new Gtk::CheckButton(_("Opacity")));
                    bool old = prefs->getBool(prefs_path + "pick_to_opacity", false);
                    b->set_active(old);
                    b->set_tooltip_text(_("Each clone's opacity is determined by the picked value in that point"));
                    table_attach (table, b, 0.0, 2, 2);
                    b->signal_toggled().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::pick_to), b, "pick_to_opacity"));
                }
            }
            vvb->set_sensitive(prefs->getBool(prefs_path + "dotrace"));
        }
        }

        {
            auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
            hb->set_homogeneous(false);
            mainbox->pack_start(*hb, false, false, 0);
            auto l = Gtk::manage(new Gtk::Label(""));
            l->set_markup(_("Apply to tiled clones:"));
            hb->pack_start(*l, false, false, 0);
        }
        // Rows/columns, width/height
        {
            auto table = Gtk::manage(new Gtk::Grid());
            table->set_row_spacing(4);
            table->set_column_spacing(6);

            table->set_border_width(VB_MARGIN);
            mainbox->pack_start(*table, false, false, 0);

            {
                _rowscols = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));

                {
                    auto a = Gtk::Adjustment::create(0.0, 1, 500, 1, 10, 0);
                    int value = prefs->getInt(prefs_path + "jmax", 2);
                    a->set_value (value);

                    auto sb = new Inkscape::UI::Widget::SpinButton(a, 1.0, 0);
                    sb->set_tooltip_text (_("How many rows in the tiling"));
                    sb->set_width_chars (7);
                    _rowscols->pack_start(*sb, true, true, 0);

                    a->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::xy_changed), a, "jmax"));
                }

                {
                    auto l = Gtk::manage(new Gtk::Label(""));
                    l->set_markup("&#215;");
                    _rowscols->pack_start(*l, true, true, 0);
                }

                {
                    auto a = Gtk::Adjustment::create(0.0, 1, 500, 1, 10, 0);
                    int value = prefs->getInt(prefs_path + "imax", 2);
                    a->set_value (value);

                    auto sb = new Inkscape::UI::Widget::SpinButton(a, 1.0, 0);
                    sb->set_tooltip_text (_("How many columns in the tiling"));
                    sb->set_width_chars (7);
                    _rowscols->pack_start(*sb, true, true, 0);

                    a->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::xy_changed), a, "imax"));
                }

                table_attach(table, _rowscols, 0.0, 1, 2);
            }

            {
                _widthheight = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));

                // unitmenu
                unit_menu = new Inkscape::UI::Widget::UnitMenu();
                unit_menu->setUnitType(Inkscape::Util::UNIT_TYPE_LINEAR);
                unit_menu->setUnit(SP_ACTIVE_DESKTOP->getNamedView()->display_units->abbr);
                unitChangedConn = unit_menu->signal_changed().connect(sigc::mem_fun(*this, &CloneTiler::unit_changed));

                {
                    // Width spinbutton
                    fill_width = Gtk::Adjustment::create(0.0, -1e6, 1e6, 1.0, 10.0, 0);

                    double value = prefs->getDouble(prefs_path + "fillwidth", 50.0);
                    Inkscape::Util::Unit const *unit = unit_menu->getUnit();
                    gdouble const units = Inkscape::Util::Quantity::convert(value, "px", unit);
                    fill_width->set_value (units);

                    auto e = new Inkscape::UI::Widget::SpinButton(fill_width, 1.0, 2);
                    e->set_tooltip_text (_("Width of the rectangle to be filled"));
                    e->set_width_chars (7);
                    e->set_digits (4);
                    _widthheight->pack_start(*e, true, true, 0);
                    fill_width->signal_value_changed().connect(sigc::mem_fun(*this, &CloneTiler::fill_width_changed));
                }
                {
                    auto l = Gtk::manage(new Gtk::Label(""));
                    l->set_markup("&#215;");
                    _widthheight->pack_start(*l, true, true, 0);
                }

                {
                    // Height spinbutton
                    fill_height = Gtk::Adjustment::create(0.0, -1e6, 1e6, 1.0, 10.0, 0);

                    double value = prefs->getDouble(prefs_path + "fillheight", 50.0);
                    Inkscape::Util::Unit const *unit = unit_menu->getUnit();
                    gdouble const units = Inkscape::Util::Quantity::convert(value, "px", unit);
                    fill_height->set_value (units);

                    auto e = new Inkscape::UI::Widget::SpinButton(fill_height, 1.0, 2);
                    e->set_tooltip_text (_("Height of the rectangle to be filled"));
                    e->set_width_chars (7);
                    e->set_digits (4);
                    _widthheight->pack_start(*e, true, true, 0);
                    fill_height->signal_value_changed().connect(sigc::mem_fun(*this, &CloneTiler::fill_height_changed));
                }

                _widthheight->pack_start(*unit_menu, true, true, 0);
                table_attach(table, _widthheight, 0.0, 2, 2);

            }

            // Switch
            Gtk::RadioButtonGroup rb_group;
            {
                auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("Rows, columns: ")));
                radio->set_tooltip_text(_("Create the specified number of rows and columns"));
                table_attach(table, radio, 0.0, 1, 1);
                radio->signal_toggled().connect(sigc::mem_fun(*this, &CloneTiler::switch_to_create));

                if (!prefs->getBool(prefs_path + "fillrect")) {
                    radio->set_active(true);
                }
            }
            {
                auto radio = Gtk::manage(new Gtk::RadioButton(rb_group, _("Width, height: ")));
                radio->set_tooltip_text(_("Fill the specified width and height with the tiling"));
                table_attach(table, radio, 0.0, 2, 1);
                radio->signal_toggled().connect(sigc::mem_fun(*this, &CloneTiler::switch_to_fill));

                if (prefs->getBool(prefs_path + "fillrect")) {
                    radio->set_active(true);
                }
            }
        }


        // Use saved pos
        {
            auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
            mainbox->pack_start(*hb, false, false, 0);

            _cb_keep_bbox = Gtk::manage(new UI::Widget::CheckButtonInternal(_("Use saved size and position of the tile")));
            auto keepbbox = prefs->getBool(prefs_path + "keepbbox", true);
            _cb_keep_bbox->set_active(keepbbox);
            _cb_keep_bbox->set_tooltip_text(_("Pretend that the size and position of the tile are the same "
                                              "as the last time you tiled it (if any), instead of using the "
                                              "current size"));
            hb->pack_start(*_cb_keep_bbox, false, false, 0);
            _cb_keep_bbox->signal_toggled().connect(sigc::mem_fun(*this, &CloneTiler::keep_bbox_toggled));
        }

        // Statusbar
        {
            auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
            hb->set_homogeneous(false);
            mainbox->pack_end(*hb, false, false, 0);
            auto l = Gtk::manage(new Gtk::Label(""));
            _status = l;
            hb->pack_start(*l, false, false, 0);
        }

        // Buttons
        {
            auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
            hb->set_homogeneous(false);
            mainbox->pack_start(*hb, false, false, 0);

            {
                auto b = Gtk::manage(new Gtk::Button());
                auto l = Gtk::manage(new Gtk::Label(""));
                l->set_markup_with_mnemonic(_(" <b>_Create</b> "));
                b->add(*l);
                b->set_tooltip_text(_("Create and tile the clones of the selection"));
                b->signal_clicked().connect(sigc::mem_fun(*this, &CloneTiler::apply));
                hb->pack_end(*b, false, false, 0);
            }

            { // buttons which are enabled only when there are tiled clones
                auto sb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 4));
                sb->set_homogeneous(false);
                hb->pack_end(*sb, false, false, 0);
                _buttons_on_tiles = sb;
                {
                    // TRANSLATORS: if a group of objects are "clumped" together, then they
                    //  are unevenly spread in the given amount of space - as shown in the
                    //  diagrams on the left in the following screenshot:
                    //  http://www.inkscape.org/screenshots/gallery/inkscape-0.42-CVS-tiles-unclump.png
                    //  So unclumping is the process of spreading a number of objects out more evenly.
                    auto b = Gtk::manage(new Gtk::Button(_(" _Unclump "), true));
                    b->set_tooltip_text(_("Spread out clones to reduce clumping; can be applied repeatedly"));
                    b->signal_clicked().connect(sigc::mem_fun(*this, &CloneTiler::unclump));
                    sb->pack_end(*b, false, false, 0);
                }

                {
                    auto b = Gtk::manage(new Gtk::Button(_(" Re_move "), true));
                    b->set_tooltip_text(_("Remove existing tiled clones of the selected object (siblings only)"));
                    b->signal_clicked().connect(sigc::mem_fun(*this, &CloneTiler::on_remove_button_clicked));
                    sb->pack_end(*b, false, false, 0);
                }

                // connect to global selection changed signal (so we can change desktops) and
                // external_change (so we're not fooled by undo)
                selectChangedConn = INKSCAPE.signal_selection_changed.connect(sigc::mem_fun(*this, &CloneTiler::change_selection));
                externChangedConn = INKSCAPE.signal_external_change.connect(sigc::mem_fun(*this, &CloneTiler::external_change));

                // update now
                change_selection(SP_ACTIVE_DESKTOP->getSelection());
            }

            {
                auto b = Gtk::manage(new Gtk::Button(_(" R_eset "), true));
                // TRANSLATORS: "change" is a noun here
                b->set_tooltip_text(_("Reset all shifts, scales, rotates, opacity and color changes in the dialog to zero"));
                b->signal_clicked().connect(sigc::mem_fun(*this, &CloneTiler::reset));
                hb->pack_start(*b, false, false, 0);
            }
        }

        mainbox->show_all();
    }

    show_all();
}

CloneTiler::~CloneTiler ()
{
    selectChangedConn.disconnect();
    externChangedConn.disconnect();
    color_changed_connection.disconnect();
}

void CloneTiler::on_picker_color_changed(guint rgba)
{
    static bool is_updating = false;
    if (is_updating || !SP_ACTIVE_DESKTOP)
        return;

    is_updating = true;

    gchar c[32];
    sp_svg_write_color(c, sizeof(c), rgba);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString(prefs_path + "initial_color", c);

    is_updating = false;
}

void CloneTiler::change_selection(Inkscape::Selection *selection)
{
    if (selection->isEmpty()) {
        _buttons_on_tiles->set_sensitive(false);
        _status->set_markup(_("<small>Nothing selected.</small>"));
        return;
    }

    if (boost::distance(selection->items()) > 1) {
        _buttons_on_tiles->set_sensitive(false);
        _status->set_markup(_("<small>More than one object selected.</small>"));
        return;
    }

    guint n = number_of_clones(selection->singleItem());
    if (n > 0) {
        _buttons_on_tiles->set_sensitive(true);
        gchar *sta = g_strdup_printf (_("<small>Object has <b>%d</b> tiled clones.</small>"), n);
        _status->set_markup(sta);
        g_free (sta);
    } else {
        _buttons_on_tiles->set_sensitive(false);
        _status->set_markup(_("<small>Object has no tiled clones.</small>"));
    }
}

void CloneTiler::external_change()
{
    change_selection(SP_ACTIVE_DESKTOP->getSelection());
}

Geom::Affine CloneTiler::get_transform(
    // symmetry group
    int type,

    // row, column
    int i, int j,

    // center, width, height of the tile
    double cx, double cy,
    double w,  double h,

    // values from the dialog:
    // Shift
    double shiftx_per_i,      double shifty_per_i,
    double shiftx_per_j,      double shifty_per_j,
    double shiftx_rand,       double shifty_rand,
    double shiftx_exp,        double shifty_exp,
    int    shiftx_alternate,  int    shifty_alternate,
    int    shiftx_cumulate,   int    shifty_cumulate,
    int    shiftx_excludew,   int    shifty_excludeh,

    // Scale
    double scalex_per_i,      double scaley_per_i,
    double scalex_per_j,      double scaley_per_j,
    double scalex_rand,       double scaley_rand,
    double scalex_exp,        double scaley_exp,
    double scalex_log,        double scaley_log,
    int    scalex_alternate,  int    scaley_alternate,
    int    scalex_cumulate,   int    scaley_cumulate,

    // Rotation
    double rotate_per_i,      double rotate_per_j,
    double rotate_rand,
    int    rotate_alternatei, int    rotate_alternatej,
    int    rotate_cumulatei,  int    rotate_cumulatej
    )
{

    // Shift (in units of tile width or height) -------------
    double delta_shifti = 0.0;
    double delta_shiftj = 0.0;

    if( shiftx_alternate ) {
        delta_shifti = (double)(i%2);
    } else {
        if( shiftx_cumulate ) {  // Should the delta shifts be cumulative (i.e. 1, 1+2, 1+2+3, ...)
            delta_shifti = (double)(i*i);
        } else {
            delta_shifti = (double)i;
        }
    }

    if( shifty_alternate ) {
        delta_shiftj = (double)(j%2);
    } else {
        if( shifty_cumulate ) {
            delta_shiftj = (double)(j*j);
        } else {
            delta_shiftj = (double)j;
        }
    }

    // Random shift, only calculate if non-zero.
    double delta_shiftx_rand = 0.0;
    double delta_shifty_rand = 0.0;
    if( shiftx_rand != 0.0 ) delta_shiftx_rand = shiftx_rand * g_random_double_range (-1, 1);
    if( shifty_rand != 0.0 ) delta_shifty_rand = shifty_rand * g_random_double_range (-1, 1);


    // Delta shift (units of tile width/height)
    double di = shiftx_per_i * delta_shifti  + shiftx_per_j * delta_shiftj + delta_shiftx_rand;
    double dj = shifty_per_i * delta_shifti  + shifty_per_j * delta_shiftj + delta_shifty_rand;

    // Shift in actual x and y, used below
    double dx = w * di;
    double dy = h * dj;

    double shifti = di;
    double shiftj = dj;

    // Include tile width and height in shift if required
    if( !shiftx_excludew ) shifti += i;
    if( !shifty_excludeh ) shiftj += j;

    // Add exponential shift if necessary
    double shifti_sign = (shifti > 0.0) ? 1.0 : -1.0;
    shifti = shifti_sign * pow(fabs(shifti), shiftx_exp);
    double shiftj_sign = (shiftj > 0.0) ? 1.0 : -1.0;
    shiftj = shiftj_sign * pow(fabs(shiftj), shifty_exp);

    // Final shift
    Geom::Affine rect_translate (Geom::Translate (w * shifti, h * shiftj));

    // Rotation (in degrees) ------------
    double delta_rotationi = 0.0;
    double delta_rotationj = 0.0;

    if( rotate_alternatei ) {
        delta_rotationi = (double)(i%2);
    } else {
        if( rotate_cumulatei ) {
            delta_rotationi = (double)(i*i + i)/2.0;
        } else {
            delta_rotationi = (double)i;
        }
    }

    if( rotate_alternatej ) {
        delta_rotationj = (double)(j%2);
    } else {
        if( rotate_cumulatej ) {
            delta_rotationj = (double)(j*j + j)/2.0;
        } else {
            delta_rotationj = (double)j;
        }
    }

    double delta_rotate_rand = 0.0;
    if( rotate_rand != 0.0 ) delta_rotate_rand = rotate_rand * 180.0 * g_random_double_range (-1, 1);

    double dr = rotate_per_i * delta_rotationi + rotate_per_j * delta_rotationj + delta_rotate_rand;

    // Scale (times the original) -----------
    double delta_scalei = 0.0;
    double delta_scalej = 0.0;

    if( scalex_alternate ) {
        delta_scalei = (double)(i%2);
    } else {
        if( scalex_cumulate ) {  // Should the delta scales be cumulative (i.e. 1, 1+2, 1+2+3, ...)
            delta_scalei = (double)(i*i + i)/2.0;
        } else {
            delta_scalei = (double)i;
        }
    }

    if( scaley_alternate ) {
        delta_scalej = (double)(j%2);
    } else {
        if( scaley_cumulate ) {
            delta_scalej = (double)(j*j + j)/2.0;
        } else {
            delta_scalej = (double)j;
        }
    }

    // Random scale, only calculate if non-zero.
    double delta_scalex_rand = 0.0;
    double delta_scaley_rand = 0.0;
    if( scalex_rand != 0.0 ) delta_scalex_rand = scalex_rand * g_random_double_range (-1, 1);
    if( scaley_rand != 0.0 ) delta_scaley_rand = scaley_rand * g_random_double_range (-1, 1);
    // But if random factors are same, scale x and y proportionally
    if( scalex_rand == scaley_rand ) delta_scalex_rand = delta_scaley_rand;

    // Total delta scale
    double scalex = 1.0 + scalex_per_i * delta_scalei  + scalex_per_j * delta_scalej + delta_scalex_rand;
    double scaley = 1.0 + scaley_per_i * delta_scalei  + scaley_per_j * delta_scalej + delta_scaley_rand;

    if( scalex < 0.0 ) scalex = 0.0;
    if( scaley < 0.0 ) scaley = 0.0;

    // Add exponential scale if necessary
    if ( scalex_exp != 1.0 ) scalex = pow( scalex, scalex_exp );
    if ( scaley_exp != 1.0 ) scaley = pow( scaley, scaley_exp );

    // Add logarithmic factor if necessary
    if ( scalex_log  > 0.0 ) scalex = pow( scalex_log, scalex - 1.0 );
    if ( scaley_log  > 0.0 ) scaley = pow( scaley_log, scaley - 1.0 );
    // Alternative using rotation angle
    //if ( scalex_log  != 1.0 ) scalex *= pow( scalex_log, M_PI*dr/180 );
    //if ( scaley_log  != 1.0 ) scaley *= pow( scaley_log, M_PI*dr/180 );


    // Calculate transformation matrices, translating back to "center of tile" (rotation center) before transforming
    Geom::Affine drot_c   = Geom::Translate(-cx, -cy) * Geom::Rotate (M_PI*dr/180)    * Geom::Translate(cx, cy);

    Geom::Affine dscale_c = Geom::Translate(-cx, -cy) * Geom::Scale (scalex, scaley)  * Geom::Translate(cx, cy);

    Geom::Affine d_s_r = dscale_c * drot_c;

    Geom::Affine rotate_180_c  = Geom::Translate(-cx, -cy) * Geom::Rotate (M_PI)      * Geom::Translate(cx, cy);

    Geom::Affine rotate_90_c   = Geom::Translate(-cx, -cy) * Geom::Rotate (-M_PI/2)   * Geom::Translate(cx, cy);
    Geom::Affine rotate_m90_c  = Geom::Translate(-cx, -cy) * Geom::Rotate ( M_PI/2)   * Geom::Translate(cx, cy);

    Geom::Affine rotate_120_c  = Geom::Translate(-cx, -cy) * Geom::Rotate (-2*M_PI/3) * Geom::Translate(cx, cy);
    Geom::Affine rotate_m120_c = Geom::Translate(-cx, -cy) * Geom::Rotate ( 2*M_PI/3) * Geom::Translate(cx, cy);

    Geom::Affine rotate_60_c   = Geom::Translate(-cx, -cy) * Geom::Rotate (-M_PI/3)   * Geom::Translate(cx, cy);
    Geom::Affine rotate_m60_c  = Geom::Translate(-cx, -cy) * Geom::Rotate ( M_PI/3)   * Geom::Translate(cx, cy);

    Geom::Affine flip_x        = Geom::Translate(-cx, -cy) * Geom::Scale (-1, 1)      * Geom::Translate(cx, cy);
    Geom::Affine flip_y        = Geom::Translate(-cx, -cy) * Geom::Scale (1, -1)      * Geom::Translate(cx, cy);


    // Create tile with required symmetry
    const double cos60 = cos(M_PI/3);
    const double sin60 = sin(M_PI/3);
    const double cos30 = cos(M_PI/6);
    const double sin30 = sin(M_PI/6);

    switch (type) {

    case TILE_P1:
        return d_s_r * rect_translate;
        break;

    case TILE_P2:
        if (i % 2 == 0) {
            return d_s_r * rect_translate;
        } else {
            return d_s_r * rotate_180_c * rect_translate;
        }
        break;

    case TILE_PM:
        if (i % 2 == 0) {
            return d_s_r * rect_translate;
        } else {
            return d_s_r * flip_x * rect_translate;
        }
        break;

    case TILE_PG:
        if (j % 2 == 0) {
            return d_s_r * rect_translate;
        } else {
            return d_s_r * flip_x * rect_translate;
        }
        break;

    case TILE_CM:
        if ((i + j) % 2 == 0) {
            return d_s_r * rect_translate;
        } else {
            return d_s_r * flip_x * rect_translate;
        }
        break;

    case TILE_PMM:
        if (j % 2 == 0) {
            if (i % 2 == 0) {
                return d_s_r * rect_translate;
            } else {
                return d_s_r * flip_x * rect_translate;
            }
        } else {
            if (i % 2 == 0) {
                return d_s_r * flip_y * rect_translate;
            } else {
                return d_s_r * flip_x * flip_y * rect_translate;
            }
        }
        break;

    case TILE_PMG:
        if (j % 2 == 0) {
            if (i % 2 == 0) {
                return d_s_r * rect_translate;
            } else {
                return d_s_r * rotate_180_c * rect_translate;
            }
        } else {
            if (i % 2 == 0) {
                return d_s_r * flip_y * rect_translate;
            } else {
                return d_s_r * rotate_180_c * flip_y * rect_translate;
            }
        }
        break;

    case TILE_PGG:
        if (j % 2 == 0) {
            if (i % 2 == 0) {
                return d_s_r * rect_translate;
            } else {
                return d_s_r * flip_y * rect_translate;
            }
        } else {
            if (i % 2 == 0) {
                return d_s_r * rotate_180_c * rect_translate;
            } else {
                return d_s_r * rotate_180_c * flip_y * rect_translate;
            }
        }
        break;

    case TILE_CMM:
        if (j % 4 == 0) {
            if (i % 2 == 0) {
                return d_s_r * rect_translate;
            } else {
                return d_s_r * flip_x * rect_translate;
            }
        } else if (j % 4 == 1) {
            if (i % 2 == 0) {
                return d_s_r * flip_y * rect_translate;
            } else {
                return d_s_r * flip_x * flip_y * rect_translate;
            }
        } else if (j % 4 == 2) {
            if (i % 2 == 1) {
                return d_s_r * rect_translate;
            } else {
                return d_s_r * flip_x * rect_translate;
            }
        } else {
            if (i % 2 == 1) {
                return d_s_r * flip_y * rect_translate;
            } else {
                return d_s_r * flip_x * flip_y * rect_translate;
            }
        }
        break;

    case TILE_P4:
    {
        Geom::Affine ori  (Geom::Translate ((w + h) * pow((i/2), shiftx_exp) + dx,  (h + w) * pow((j/2), shifty_exp) + dy));
        Geom::Affine dia1 (Geom::Translate (w/2 + h/2, -h/2 + w/2));
        Geom::Affine dia2 (Geom::Translate (-w/2 + h/2, h/2 + w/2));
        if (j % 2 == 0) {
            if (i % 2 == 0) {
                return d_s_r * ori;
            } else {
                return d_s_r * rotate_m90_c * dia1 * ori;
            }
        } else {
            if (i % 2 == 0) {
                return d_s_r * rotate_90_c * dia2 * ori;
            } else {
                return d_s_r * rotate_180_c * dia1 * dia2 * ori;
            }
        }
    }
    break;

    case TILE_P4M:
    {
        double max = MAX(w, h);
        Geom::Affine ori (Geom::Translate ((max + max) * pow((i/4), shiftx_exp) + dx,  (max + max) * pow((j/2), shifty_exp) + dy));
        Geom::Affine dia1 (Geom::Translate ( w/2 - h/2, h/2 - w/2));
        Geom::Affine dia2 (Geom::Translate (-h/2 + w/2, w/2 - h/2));
        if (j % 2 == 0) {
            if (i % 4 == 0) {
                return d_s_r * ori;
            } else if (i % 4 == 1) {
                return d_s_r * flip_y * rotate_m90_c * dia1 * ori;
            } else if (i % 4 == 2) {
                return d_s_r * rotate_m90_c * dia1 * Geom::Translate (h, 0) * ori;
            } else if (i % 4 == 3) {
                return d_s_r * flip_x * Geom::Translate (w, 0) * ori;
            }
        } else {
            if (i % 4 == 0) {
                return d_s_r * flip_y * Geom::Translate(0, h) * ori;
            } else if (i % 4 == 1) {
                return d_s_r * rotate_90_c * dia2 * Geom::Translate(0, h) * ori;
            } else if (i % 4 == 2) {
                return d_s_r * flip_y * rotate_90_c * dia2 * Geom::Translate(h, 0) * Geom::Translate(0, h) * ori;
            } else if (i % 4 == 3) {
                return d_s_r * flip_y * flip_x * Geom::Translate(w, 0) * Geom::Translate(0, h) * ori;
            }
        }
    }
    break;

    case TILE_P4G:
    {
        double max = MAX(w, h);
        Geom::Affine ori (Geom::Translate ((max + max) * pow((i/4), shiftx_exp) + dx,  (max + max) * pow(j, shifty_exp) + dy));
        Geom::Affine dia1 (Geom::Translate ( w/2 + h/2, h/2 - w/2));
        Geom::Affine dia2 (Geom::Translate (-h/2 + w/2, w/2 + h/2));
        if (((i/4) + j) % 2 == 0) {
            if (i % 4 == 0) {
                return d_s_r * ori;
            } else if (i % 4 == 1) {
                return d_s_r * rotate_m90_c * dia1 * ori;
            } else if (i % 4 == 2) {
                return d_s_r * rotate_90_c * dia2 * ori;
            } else if (i % 4 == 3) {
                return d_s_r * rotate_180_c * dia1 * dia2 * ori;
            }
        } else {
            if (i % 4 == 0) {
                return d_s_r * flip_y * Geom::Translate (0, h) * ori;
            } else if (i % 4 == 1) {
                return d_s_r * flip_y * rotate_m90_c * dia1 * Geom::Translate (-h, 0) * ori;
            } else if (i % 4 == 2) {
                return d_s_r * flip_y * rotate_90_c * dia2 * Geom::Translate (h, 0) * ori;
            } else if (i % 4 == 3) {
                return d_s_r * flip_x * Geom::Translate (w, 0) * ori;
            }
        }
    }
    break;

    case TILE_P3:
    {
        double width;
        double height;
        Geom::Affine dia1;
        Geom::Affine dia2;
        if (w > h) {
            width  = w + w * cos60;
            height = 2 * w * sin60;
            dia1 = Geom::Affine (Geom::Translate (w/2 + w/2 * cos60, -(w/2 * sin60)));
            dia2 = dia1 * Geom::Affine (Geom::Translate (0, 2 * (w/2 * sin60)));
        } else {
            width = h * cos (M_PI/6);
            height = h;
            dia1 = Geom::Affine (Geom::Translate (h/2 * cos30, -(h/2 * sin30)));
            dia2 = dia1 * Geom::Affine (Geom::Translate (0, h/2));
        }
        Geom::Affine ori (Geom::Translate (width * pow((2*(i/3) + j%2), shiftx_exp) + dx,  (height/2) * pow(j, shifty_exp) + dy));
        if (i % 3 == 0) {
            return d_s_r * ori;
        } else if (i % 3 == 1) {
            return d_s_r * rotate_m120_c * dia1 * ori;
        } else if (i % 3 == 2) {
            return d_s_r * rotate_120_c * dia2 * ori;
        }
    }
    break;

    case TILE_P31M:
    {
        Geom::Affine ori;
        Geom::Affine dia1;
        Geom::Affine dia2;
        Geom::Affine dia3;
        Geom::Affine dia4;
        if (w > h) {
            ori = Geom::Affine(Geom::Translate (w * pow((i/6) + 0.5*(j%2), shiftx_exp) + dx,  (w * cos30) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (0, h/2) * Geom::Translate (w/2, 0) * Geom::Translate (w/2 * cos60, -w/2 * sin60) * Geom::Translate (-h/2 * cos30, -h/2 * sin30) );
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (0, 2 * (w/2 * sin60 - h/2 * sin30)));
            dia4 = dia3 * Geom::Affine (Geom::Translate (-h * cos30, h * sin30));
        } else {
            ori  = Geom::Affine (Geom::Translate (2*h * cos30  * pow((i/6 + 0.5*(j%2)), shiftx_exp) + dx,  (2*h - h * sin30) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (0, -h/2) * Geom::Translate (h/2 * cos30, h/2 * sin30));
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (0, h/2));
            dia4 = dia3 * Geom::Affine (Geom::Translate (-h * cos30, h * sin30));
        }
        if (i % 6 == 0) {
            return d_s_r * ori;
        } else if (i % 6 == 1) {
            return d_s_r * flip_y * rotate_m120_c * dia1 * ori;
        } else if (i % 6 == 2) {
            return d_s_r * rotate_m120_c * dia2 * ori;
        } else if (i % 6 == 3) {
            return d_s_r * flip_y * rotate_120_c * dia3 * ori;
        } else if (i % 6 == 4) {
            return d_s_r * rotate_120_c * dia4 * ori;
        } else if (i % 6 == 5) {
            return d_s_r * flip_y * Geom::Translate(0, h) * ori;
        }
    }
    break;

    case TILE_P3M1:
    {
        double width;
        double height;
        Geom::Affine dia1;
        Geom::Affine dia2;
        Geom::Affine dia3;
        Geom::Affine dia4;
        if (w > h) {
            width = w + w * cos60;
            height = 2 * w * sin60;
            dia1 = Geom::Affine (Geom::Translate (0, h/2) * Geom::Translate (w/2, 0) * Geom::Translate (w/2 * cos60, -w/2 * sin60) * Geom::Translate (-h/2 * cos30, -h/2 * sin30) );
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (0, 2 * (w/2 * sin60 - h/2 * sin30)));
            dia4 = dia3 * Geom::Affine (Geom::Translate (-h * cos30, h * sin30));
        } else {
            width = 2 * h * cos (M_PI/6);
            height = 2 * h;
            dia1 = Geom::Affine (Geom::Translate (0, -h/2) * Geom::Translate (h/2 * cos30, h/2 * sin30));
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (0, h/2));
            dia4 = dia3 * Geom::Affine (Geom::Translate (-h * cos30, h * sin30));
        }
        Geom::Affine ori (Geom::Translate (width * pow((2*(i/6) + j%2), shiftx_exp) + dx,  (height/2) * pow(j, shifty_exp) + dy));
        if (i % 6 == 0) {
            return d_s_r * ori;
        } else if (i % 6 == 1) {
            return d_s_r * flip_y * rotate_m120_c * dia1 * ori;
        } else if (i % 6 == 2) {
            return d_s_r * rotate_m120_c * dia2 * ori;
        } else if (i % 6 == 3) {
            return d_s_r * flip_y * rotate_120_c * dia3 * ori;
        } else if (i % 6 == 4) {
            return d_s_r * rotate_120_c * dia4 * ori;
        } else if (i % 6 == 5) {
            return d_s_r * flip_y * Geom::Translate(0, h) * ori;
        }
    }
    break;

    case TILE_P6:
    {
        Geom::Affine ori;
        Geom::Affine dia1;
        Geom::Affine dia2;
        Geom::Affine dia3;
        Geom::Affine dia4;
        Geom::Affine dia5;
        if (w > h) {
            ori = Geom::Affine(Geom::Translate (w * pow((2*(i/6) + (j%2)), shiftx_exp) + dx,  (2*w * sin60) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (w/2 * cos60, -w/2 * sin60));
            dia2 = dia1 * Geom::Affine (Geom::Translate (w/2, 0));
            dia3 = dia2 * Geom::Affine (Geom::Translate (w/2 * cos60, w/2 * sin60));
            dia4 = dia3 * Geom::Affine (Geom::Translate (-w/2 * cos60, w/2 * sin60));
            dia5 = dia4 * Geom::Affine (Geom::Translate (-w/2, 0));
        } else {
            ori = Geom::Affine(Geom::Translate (2*h * cos30 * pow((i/6 + 0.5*(j%2)), shiftx_exp) + dx,  (h + h * sin30) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (-w/2, -h/2) * Geom::Translate (h/2 * cos30, -h/2 * sin30) * Geom::Translate (w/2 * cos60, w/2 * sin60));
            dia2 = dia1 * Geom::Affine (Geom::Translate (-w/2 * cos60, -w/2 * sin60) * Geom::Translate (h/2 * cos30, -h/2 * sin30) * Geom::Translate (h/2 * cos30, h/2 * sin30) * Geom::Translate (-w/2 * cos60, w/2 * sin60));
            dia3 = dia2 * Geom::Affine (Geom::Translate (w/2 * cos60, -w/2 * sin60) * Geom::Translate (h/2 * cos30, h/2 * sin30) * Geom::Translate (-w/2, h/2));
            dia4 = dia3 * dia1.inverse();
            dia5 = dia3 * dia2.inverse();
        }
        if (i % 6 == 0) {
            return d_s_r * ori;
        } else if (i % 6 == 1) {
            return d_s_r * rotate_m60_c * dia1 * ori;
        } else if (i % 6 == 2) {
            return d_s_r * rotate_m120_c * dia2 * ori;
        } else if (i % 6 == 3) {
            return d_s_r * rotate_180_c * dia3 * ori;
        } else if (i % 6 == 4) {
            return d_s_r * rotate_120_c * dia4 * ori;
        } else if (i % 6 == 5) {
            return d_s_r * rotate_60_c * dia5 * ori;
        }
    }
    break;

    case TILE_P6M:
    {

        Geom::Affine ori;
        Geom::Affine dia1, dia2, dia3, dia4, dia5, dia6, dia7, dia8, dia9, dia10;
        if (w > h) {
            ori = Geom::Affine(Geom::Translate (w * pow((2*(i/12) + (j%2)), shiftx_exp) + dx,  (2*w * sin60) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (w/2, h/2) * Geom::Translate (-w/2 * cos60, -w/2 * sin60) * Geom::Translate (-h/2 * cos30, h/2 * sin30));
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, -h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (-h/2 * cos30, h/2 * sin30) * Geom::Translate (w * cos60, 0) * Geom::Translate (-h/2 * cos30, -h/2 * sin30));
            dia4 = dia3 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia5 = dia4 * Geom::Affine (Geom::Translate (-h/2 * cos30, -h/2 * sin30) * Geom::Translate (-w/2 * cos60, w/2 * sin60) * Geom::Translate (w/2, -h/2));
            dia6 = dia5 * Geom::Affine (Geom::Translate (0, h));
            dia7 = dia6 * dia1.inverse();
            dia8 = dia6 * dia2.inverse();
            dia9 = dia6 * dia3.inverse();
            dia10 = dia6 * dia4.inverse();
        } else {
            ori = Geom::Affine(Geom::Translate (4*h * cos30 * pow((i/12 + 0.5*(j%2)), shiftx_exp) + dx,  (2*h  + 2*h * sin30) * pow(j, shifty_exp) + dy));
            dia1 = Geom::Affine (Geom::Translate (-w/2, -h/2) * Geom::Translate (h/2 * cos30, -h/2 * sin30) * Geom::Translate (w/2 * cos60, w/2 * sin60));
            dia2 = dia1 * Geom::Affine (Geom::Translate (h * cos30, -h * sin30));
            dia3 = dia2 * Geom::Affine (Geom::Translate (-w/2 * cos60, -w/2 * sin60) * Geom::Translate (h * cos30, 0) * Geom::Translate (-w/2 * cos60, w/2 * sin60));
            dia4 = dia3 * Geom::Affine (Geom::Translate (h * cos30, h * sin30));
            dia5 = dia4 * Geom::Affine (Geom::Translate (w/2 * cos60, -w/2 * sin60) * Geom::Translate (h/2 * cos30, h/2 * sin30) * Geom::Translate (-w/2, h/2));
            dia6 = dia5 * Geom::Affine (Geom::Translate (0, h));
            dia7 = dia6 * dia1.inverse();
            dia8 = dia6 * dia2.inverse();
            dia9 = dia6 * dia3.inverse();
            dia10 = dia6 * dia4.inverse();
        }
        if (i % 12 == 0) {
            return d_s_r * ori;
        } else if (i % 12 == 1) {
            return d_s_r * flip_y * rotate_m60_c * dia1 * ori;
        } else if (i % 12 == 2) {
            return d_s_r * rotate_m60_c * dia2 * ori;
        } else if (i % 12 == 3) {
            return d_s_r * flip_y * rotate_m120_c * dia3 * ori;
        } else if (i % 12 == 4) {
            return d_s_r * rotate_m120_c * dia4 * ori;
        } else if (i % 12 == 5) {
            return d_s_r * flip_x * dia5 * ori;
        } else if (i % 12 == 6) {
            return d_s_r * flip_x * flip_y * dia6 * ori;
        } else if (i % 12 == 7) {
            return d_s_r * flip_y * rotate_120_c * dia7 * ori;
        } else if (i % 12 == 8) {
            return d_s_r * rotate_120_c * dia8 * ori;
        } else if (i % 12 == 9) {
            return d_s_r * flip_y * rotate_60_c * dia9 * ori;
        } else if (i % 12 == 10) {
            return d_s_r * rotate_60_c * dia10 * ori;
        } else if (i % 12 == 11) {
            return d_s_r * flip_y * Geom::Translate (0, h) * ori;
        }
    }
    break;

    default:
        break;
    }

    return Geom::identity();
}

bool CloneTiler::is_a_clone_of(SPObject *tile, SPObject *obj)
{
    bool result = false;
    char *id_href = nullptr;

    if (obj) {
        Inkscape::XML::Node *obj_repr = obj->getRepr();
        id_href = g_strdup_printf("#%s", obj_repr->attribute("id"));
    }

    if (dynamic_cast<SPUse *>(tile) &&
        tile->getRepr()->attribute("xlink:href") &&
        (!id_href || !strcmp(id_href, tile->getRepr()->attribute("xlink:href"))) &&
        tile->getRepr()->attribute("inkscape:tiled-clone-of") &&
        (!id_href || !strcmp(id_href, tile->getRepr()->attribute("inkscape:tiled-clone-of"))))
    {
        result = true;
    } else {
        result = false;
    }
    if (id_href) {
        g_free(id_href);
        id_href = nullptr;
    }
    return result;
}

void CloneTiler::trace_hide_tiled_clones_recursively(SPObject *from)
{
    if (!trace_drawing)
        return;

    for (auto& o: from->children) {
        SPItem *item = dynamic_cast<SPItem *>(&o);
        if (item && is_a_clone_of(&o, nullptr)) {
            item->invoke_hide(trace_visionkey); // FIXME: hide each tiled clone's original too!
        }
        trace_hide_tiled_clones_recursively (&o);
    }
}

void CloneTiler::trace_setup(SPDocument *doc, gdouble zoom, SPItem *original)
{
    trace_drawing = new Inkscape::Drawing();
    /* Create ArenaItem and set transform */
    trace_visionkey = SPItem::display_key_new(1);
    trace_doc = doc;
    trace_drawing->setRoot(trace_doc->getRoot()->invoke_show(*trace_drawing, trace_visionkey, SP_ITEM_SHOW_DISPLAY));

    // hide the (current) original and any tiled clones, we only want to pick the background
    original->invoke_hide(trace_visionkey);
    trace_hide_tiled_clones_recursively(trace_doc->getRoot());

    trace_doc->getRoot()->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    trace_doc->ensureUpToDate();

    trace_zoom = zoom;
}

guint32 CloneTiler::trace_pick(Geom::Rect box)
{
    if (!trace_drawing) {
        return 0;
    }

    trace_drawing->root()->setTransform(Geom::Scale(trace_zoom));
    trace_drawing->update();

    /* Item integer bbox in points */
    Geom::IntRect ibox = (box * Geom::Scale(trace_zoom)).roundOutwards();

    /* Find visible area */
    cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ibox.width(), ibox.height());
    Inkscape::DrawingContext dc(s, ibox.min());
    /* Render */
    trace_drawing->render(dc, ibox);
    double R = 0, G = 0, B = 0, A = 0;
    ink_cairo_surface_average_color(s, R, G, B, A);
    cairo_surface_destroy(s);

    return SP_RGBA32_F_COMPOSE (R, G, B, A);
}

void CloneTiler::trace_finish()
{
    if (trace_doc) {
        trace_doc->getRoot()->invoke_hide(trace_visionkey);
        delete trace_drawing;
        trace_doc = nullptr;
        trace_drawing = nullptr;
    }
}

void CloneTiler::unclump()
{
    auto selection = getSelection();
    if (!selection)
        return;

    // check if something is selected
    if (selection->isEmpty() || boost::distance(selection->items()) > 1) {
        getDesktop()->getMessageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select <b>one object</b> whose tiled clones to unclump."));
        return;
    }

    auto obj = selection->singleItem();
    auto parent = obj->parent;

    std::vector<SPItem*> to_unclump; // not including the original

    for (auto& child: parent->children) {
        if (is_a_clone_of (&child, obj)) {
            to_unclump.push_back((SPItem*)&child);
        }
    }

    getDocument()->ensureUpToDate();
    reverse(to_unclump.begin(),to_unclump.end());
    ::unclump (to_unclump);

    DocumentUndo::done(getDocument(), SP_VERB_DIALOG_CLONETILER, _("Unclump tiled clones"));
}

guint CloneTiler::number_of_clones(SPObject *obj)
{
    SPObject *parent = obj->parent;

    guint n = 0;

    for (auto& child: parent->children) {
        if (is_a_clone_of (&child, obj)) {
            n ++;
        }
    }

    return n;
}

void CloneTiler::remove(bool do_undo/* = true*/)
{
    auto selection = getSelection();
    if (!selection)
        return;

    // check if something is selected
    if (selection->isEmpty() || boost::distance(selection->items()) > 1) {
        getDesktop()->getMessageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select <b>one object</b> whose tiled clones to remove."));
        return;
    }

    SPObject *obj = selection->singleItem();
    SPObject *parent = obj->parent;

// remove old tiling
    std::vector<SPObject *> to_delete;
    for (auto& child: parent->children) {
        if (is_a_clone_of (&child, obj)) {
            to_delete.push_back(&child);
        }
    }
    for (auto obj:to_delete) {
        g_assert(obj != nullptr);
        obj->deleteObject();
    }

    change_selection (selection);

    if (do_undo) {
        DocumentUndo::done(getDocument(), SP_VERB_DIALOG_CLONETILER,
                           _("Delete tiled clones"));
    }
}

Geom::Rect CloneTiler::transform_rect(Geom::Rect const &r, Geom::Affine const &m)
{
    using Geom::X;
    using Geom::Y;
    Geom::Point const p1 = r.corner(1) * m;
    Geom::Point const p2 = r.corner(2) * m;
    Geom::Point const p3 = r.corner(3) * m;
    Geom::Point const p4 = r.corner(4) * m;
    return Geom::Rect(
        Geom::Point(
            std::min(std::min(p1[X], p2[X]), std::min(p3[X], p4[X])),
            std::min(std::min(p1[Y], p2[Y]), std::min(p3[Y], p4[Y]))),
        Geom::Point(
            std::max(std::max(p1[X], p2[X]), std::max(p3[X], p4[X])),
            std::max(std::max(p1[Y], p2[Y]), std::max(p3[Y], p4[Y]))));
}

/**
Randomizes \a val by \a rand, with 0 < val < 1 and all values (including 0, 1) having the same
probability of being displaced.
 */
double CloneTiler::randomize01(double val, double rand)
{
    double base = MIN (val - rand, 1 - 2*rand);
    if (base < 0) {
        base = 0;
    }
    val = base + g_random_double_range (0, MIN (2 * rand, 1 - base));
    return CLAMP(val, 0, 1); // this should be unnecessary with the above provisions, but just in case...
}


void CloneTiler::apply()
{
    auto desktop = getDesktop();
    auto selection = getSelection();
    if (!selection)
        return;

    // check if something is selected
    if (selection->isEmpty()) {
        desktop->getMessageStack()->flash(Inkscape::WARNING_MESSAGE, _("Select an <b>object</b> to clone."));
        return;
    }

    // Check if more than one object is selected.
    if (boost::distance(selection->items()) > 1) {
        desktop->getMessageStack()->flash(Inkscape::ERROR_MESSAGE, _("If you want to clone several objects, <b>group</b> them and <b>clone the group</b>."));
        return;
    }

    // set "busy" cursor
    desktop->setWaitingCursor();

    // set statusbar text
    _status->set_markup(_("<small>Creating tiled clones...</small>"));
    _status->queue_draw();

    SPObject *obj = selection->singleItem();
    if (!obj) {
        // Should never happen (empty selection checked above).
        std::cerr << "CloneTiler::clonetile_apply(): No object in single item selection!!!" << std::endl;
        return;
    }
    Inkscape::XML::Node *obj_repr = obj->getRepr();
    const char *id_href = g_strdup_printf("#%s", obj_repr->attribute("id"));
    SPObject *parent = obj->parent;

    remove(false);

    Geom::Scale scale = getDocument()->getDocumentScale().inverse();
    double scale_units = scale[Geom::X]; // Use just x direction....

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    double shiftx_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "shiftx_per_i", 0, -10000, 10000);
    double shifty_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "shifty_per_i", 0, -10000, 10000);
    double shiftx_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "shiftx_per_j", 0, -10000, 10000);
    double shifty_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "shifty_per_j", 0, -10000, 10000);
    double shiftx_rand  = 0.01 * prefs->getDoubleLimited(prefs_path + "shiftx_rand", 0, 0, 1000);
    double shifty_rand  = 0.01 * prefs->getDoubleLimited(prefs_path + "shifty_rand", 0, 0, 1000);
    double shiftx_exp   =        prefs->getDoubleLimited(prefs_path + "shiftx_exp",   1, 0, 10);
    double shifty_exp   =        prefs->getDoubleLimited(prefs_path + "shifty_exp", 1, 0, 10);
    bool   shiftx_alternate =    prefs->getBool(prefs_path + "shiftx_alternate");
    bool   shifty_alternate =    prefs->getBool(prefs_path + "shifty_alternate");
    bool   shiftx_cumulate  =    prefs->getBool(prefs_path + "shiftx_cumulate");
    bool   shifty_cumulate  =    prefs->getBool(prefs_path + "shifty_cumulate");
    bool   shiftx_excludew  =    prefs->getBool(prefs_path + "shiftx_excludew");
    bool   shifty_excludeh  =    prefs->getBool(prefs_path + "shifty_excludeh");

    double scalex_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "scalex_per_i", 0, -100, 1000);
    double scaley_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "scaley_per_i", 0, -100, 1000);
    double scalex_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "scalex_per_j", 0, -100, 1000);
    double scaley_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "scaley_per_j", 0, -100, 1000);
    double scalex_rand  = 0.01 * prefs->getDoubleLimited(prefs_path + "scalex_rand",  0, 0, 1000);
    double scaley_rand  = 0.01 * prefs->getDoubleLimited(prefs_path + "scaley_rand",  0, 0, 1000);
    double scalex_exp   =        prefs->getDoubleLimited(prefs_path + "scalex_exp",   1, 0, 10);
    double scaley_exp   =        prefs->getDoubleLimited(prefs_path + "scaley_exp",   1, 0, 10);
    double scalex_log       =    prefs->getDoubleLimited(prefs_path + "scalex_log",   0, 0, 10);
    double scaley_log       =    prefs->getDoubleLimited(prefs_path + "scaley_log",   0, 0, 10);
    bool   scalex_alternate =    prefs->getBool(prefs_path + "scalex_alternate");
    bool   scaley_alternate =    prefs->getBool(prefs_path + "scaley_alternate");
    bool   scalex_cumulate  =    prefs->getBool(prefs_path + "scalex_cumulate");
    bool   scaley_cumulate  =    prefs->getBool(prefs_path + "scaley_cumulate");

    double rotate_per_i =        prefs->getDoubleLimited(prefs_path + "rotate_per_i", 0, -180, 180);
    double rotate_per_j =        prefs->getDoubleLimited(prefs_path + "rotate_per_j", 0, -180, 180);
    double rotate_rand =  0.01 * prefs->getDoubleLimited(prefs_path + "rotate_rand", 0, 0, 100);
    bool   rotate_alternatei   = prefs->getBool(prefs_path + "rotate_alternatei");
    bool   rotate_alternatej   = prefs->getBool(prefs_path + "rotate_alternatej");
    bool   rotate_cumulatei    = prefs->getBool(prefs_path + "rotate_cumulatei");
    bool   rotate_cumulatej    = prefs->getBool(prefs_path + "rotate_cumulatej");

    double blur_per_i =   0.01 * prefs->getDoubleLimited(prefs_path + "blur_per_i", 0, 0, 100);
    double blur_per_j =   0.01 * prefs->getDoubleLimited(prefs_path + "blur_per_j", 0, 0, 100);
    bool   blur_alternatei =     prefs->getBool(prefs_path + "blur_alternatei");
    bool   blur_alternatej =     prefs->getBool(prefs_path + "blur_alternatej");
    double blur_rand =    0.01 * prefs->getDoubleLimited(prefs_path + "blur_rand", 0, 0, 100);

    double opacity_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "opacity_per_i", 0, 0, 100);
    double opacity_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "opacity_per_j", 0, 0, 100);
    bool   opacity_alternatei =   prefs->getBool(prefs_path + "opacity_alternatei");
    bool   opacity_alternatej =   prefs->getBool(prefs_path + "opacity_alternatej");
    double opacity_rand =  0.01 * prefs->getDoubleLimited(prefs_path + "opacity_rand", 0, 0, 100);

    Glib::ustring initial_color =    prefs->getString(prefs_path + "initial_color");
    double hue_per_j =        0.01 * prefs->getDoubleLimited(prefs_path + "hue_per_j", 0, -100, 100);
    double hue_per_i =        0.01 * prefs->getDoubleLimited(prefs_path + "hue_per_i", 0, -100, 100);
    double hue_rand  =        0.01 * prefs->getDoubleLimited(prefs_path + "hue_rand", 0, 0, 100);
    double saturation_per_j = 0.01 * prefs->getDoubleLimited(prefs_path + "saturation_per_j", 0, -100, 100);
    double saturation_per_i = 0.01 * prefs->getDoubleLimited(prefs_path + "saturation_per_i", 0, -100, 100);
    double saturation_rand =  0.01 * prefs->getDoubleLimited(prefs_path + "saturation_rand", 0, 0, 100);
    double lightness_per_j =  0.01 * prefs->getDoubleLimited(prefs_path + "lightness_per_j", 0, -100, 100);
    double lightness_per_i =  0.01 * prefs->getDoubleLimited(prefs_path + "lightness_per_i", 0, -100, 100);
    double lightness_rand =   0.01 * prefs->getDoubleLimited(prefs_path + "lightness_rand", 0, 0, 100);
    bool   color_alternatej = prefs->getBool(prefs_path + "color_alternatej");
    bool   color_alternatei = prefs->getBool(prefs_path + "color_alternatei");

    int    type = prefs->getInt(prefs_path + "symmetrygroup", 0);
    bool   keepbbox = prefs->getBool(prefs_path + "keepbbox", true);
    int    imax = prefs->getInt(prefs_path + "imax", 2);
    int    jmax = prefs->getInt(prefs_path + "jmax", 2);

    bool   fillrect = prefs->getBool(prefs_path + "fillrect");
    double fillwidth = scale_units*prefs->getDoubleLimited(prefs_path + "fillwidth", 50, 0, 1e6);
    double fillheight = scale_units*prefs->getDoubleLimited(prefs_path + "fillheight", 50, 0, 1e6);

    bool   dotrace = prefs->getBool(prefs_path + "dotrace");
    int    pick = prefs->getInt(prefs_path + "pick");
    bool   pick_to_presence = prefs->getBool(prefs_path + "pick_to_presence");
    bool   pick_to_size = prefs->getBool(prefs_path + "pick_to_size");
    bool   pick_to_color = prefs->getBool(prefs_path + "pick_to_color");
    bool   pick_to_opacity = prefs->getBool(prefs_path + "pick_to_opacity");
    double rand_picked = 0.01 * prefs->getDoubleLimited(prefs_path + "rand_picked", 0, 0, 100);
    bool   invert_picked = prefs->getBool(prefs_path + "invert_picked");
    double gamma_picked = prefs->getDoubleLimited(prefs_path + "gamma_picked", 0, -10, 10);

    SPItem *item = dynamic_cast<SPItem *>(obj);
    if (dotrace) {
        trace_setup(getDocument(), 1.0, item);
    }

    Geom::Point center;
    double w = 0;
    double h = 0;
    double x0 = 0;
    double y0 = 0;

    if (keepbbox &&
        obj_repr->attribute("inkscape:tile-w") &&
        obj_repr->attribute("inkscape:tile-h") &&
        obj_repr->attribute("inkscape:tile-x0") &&
        obj_repr->attribute("inkscape:tile-y0") &&
        obj_repr->attribute("inkscape:tile-cx") &&
        obj_repr->attribute("inkscape:tile-cy")) {

        double cx = obj_repr->getAttributeDouble("inkscape:tile-cx", 0);
        double cy = obj_repr->getAttributeDouble("inkscape:tile-cy", 0);
        center = Geom::Point (cx, cy);

        w = obj_repr->getAttributeDouble("inkscape:tile-w", w);
        h = obj_repr->getAttributeDouble("inkscape:tile-h", h);
        x0 = obj_repr->getAttributeDouble("inkscape:tile-x0", x0);
        y0 = obj_repr->getAttributeDouble("inkscape:tile-y0", y0);
    } else {
        bool prefs_bbox = prefs->getBool("/tools/bounding_box", false);
        SPItem::BBoxType bbox_type = ( !prefs_bbox ?
            SPItem::VISUAL_BBOX : SPItem::GEOMETRIC_BBOX );
        Geom::OptRect r = item->documentBounds(bbox_type);
        if (r) {
            w = scale_units*r->dimensions()[Geom::X];
            h = scale_units*r->dimensions()[Geom::Y];
            x0 = scale_units*r->min()[Geom::X];
            y0 = scale_units*r->min()[Geom::Y];
            center = scale_units*desktop->dt2doc(item->getCenter());

            obj_repr->setAttributeSvgDouble("inkscape:tile-cx", center[Geom::X]);
            obj_repr->setAttributeSvgDouble("inkscape:tile-cy", center[Geom::Y]);
            obj_repr->setAttributeSvgDouble("inkscape:tile-w", w);
            obj_repr->setAttributeSvgDouble("inkscape:tile-h", h);
            obj_repr->setAttributeSvgDouble("inkscape:tile-x0", x0);
            obj_repr->setAttributeSvgDouble("inkscape:tile-y0", y0);
        } else {
            center = Geom::Point(0, 0);
            w = h = 0;
            x0 = y0 = 0;
        }
    }

    Geom::Point cur(0, 0);
    Geom::Rect bbox_original (Geom::Point (x0, y0), Geom::Point (x0 + w, y0 + h));
    double perimeter_original = (w + h)/4;

    // The integers i and j are reserved for tile column and row.
    // The doubles x and y are used for coordinates
    for (int i = 0;
         fillrect?
             (fabs(cur[Geom::X]) < fillwidth && i < 200) // prevent "freezing" with too large fillrect, arbitrarily limit rows
             : (i < imax);
         i ++) {
        for (int j = 0;
             fillrect?
                 (fabs(cur[Geom::Y]) < fillheight && j < 200) // prevent "freezing" with too large fillrect, arbitrarily limit cols
                 : (j < jmax);
             j ++) {

            // Note: We create a clone at 0,0 too, right over the original, in case our clones are colored

            // Get transform from symmetry, shift, scale, rotation
            Geom::Affine orig_t = get_transform (type, i, j, center[Geom::X], center[Geom::Y], w, h,
                                                       shiftx_per_i,     shifty_per_i,
                                                       shiftx_per_j,     shifty_per_j,
                                                       shiftx_rand,      shifty_rand,
                                                       shiftx_exp,       shifty_exp,
                                                       shiftx_alternate, shifty_alternate,
                                                       shiftx_cumulate,  shifty_cumulate,
                                                       shiftx_excludew,  shifty_excludeh,
                                                       scalex_per_i,     scaley_per_i,
                                                       scalex_per_j,     scaley_per_j,
                                                       scalex_rand,      scaley_rand,
                                                       scalex_exp,       scaley_exp,
                                                       scalex_log,       scaley_log,
                                                       scalex_alternate, scaley_alternate,
                                                       scalex_cumulate,  scaley_cumulate,
                                                       rotate_per_i,     rotate_per_j,
                                                       rotate_rand,
                                                       rotate_alternatei, rotate_alternatej,
                                                       rotate_cumulatei,  rotate_cumulatej      );
            Geom::Affine parent_transform = (((SPItem*)item->parent)->i2doc_affine())*(item->document->getRoot()->c2p.inverse());
            Geom::Affine t = parent_transform*orig_t*parent_transform.inverse();
            cur = center * t - center;
            if (fillrect) {
                if ((cur[Geom::X] > fillwidth) || (cur[Geom::Y] > fillheight)) { // off limits
                    continue;
                }
            }

            gchar color_string[32]; *color_string = 0;

            // Color tab
            if (!initial_color.empty()) {
                guint32 rgba = sp_svg_read_color (initial_color.data(), 0x000000ff);
                float hsl[3];
                SPColor::rgb_to_hsl_floatv (hsl, SP_RGBA32_R_F(rgba), SP_RGBA32_G_F(rgba), SP_RGBA32_B_F(rgba));

                double eff_i = (color_alternatei? (i%2) : (i));
                double eff_j = (color_alternatej? (j%2) : (j));

                hsl[0] += hue_per_i * eff_i + hue_per_j * eff_j + hue_rand * g_random_double_range (-1, 1);
                double notused;
                hsl[0] = modf( hsl[0], &notused ); // Restrict to 0-1
                hsl[1] += saturation_per_i * eff_i + saturation_per_j * eff_j + saturation_rand * g_random_double_range (-1, 1);
                hsl[1] = CLAMP (hsl[1], 0, 1);
                hsl[2] += lightness_per_i * eff_i + lightness_per_j * eff_j + lightness_rand * g_random_double_range (-1, 1);
                hsl[2] = CLAMP (hsl[2], 0, 1);

                float rgb[3];
                SPColor::hsl_to_rgb_floatv (rgb, hsl[0], hsl[1], hsl[2]);
                sp_svg_write_color(color_string, sizeof(color_string), SP_RGBA32_F_COMPOSE(rgb[0], rgb[1], rgb[2], 1.0));
            }

            // Blur
            double blur = 0.0;
            {
            int eff_i = (blur_alternatei? (i%2) : (i));
            int eff_j = (blur_alternatej? (j%2) : (j));
            blur =  (blur_per_i * eff_i + blur_per_j * eff_j + blur_rand * g_random_double_range (-1, 1));
            blur = CLAMP (blur, 0, 1);
            }

            // Opacity
            double opacity = 1.0;
            {
            int eff_i = (opacity_alternatei? (i%2) : (i));
            int eff_j = (opacity_alternatej? (j%2) : (j));
            opacity = 1 - (opacity_per_i * eff_i + opacity_per_j * eff_j + opacity_rand * g_random_double_range (-1, 1));
            opacity = CLAMP (opacity, 0, 1);
            }

            // Trace tab
            if (dotrace) {
                Geom::Rect bbox_t = transform_rect (bbox_original, t*Geom::Scale(1.0/scale_units));

                guint32 rgba = trace_pick (bbox_t);
                float r = SP_RGBA32_R_F(rgba);
                float g = SP_RGBA32_G_F(rgba);
                float b = SP_RGBA32_B_F(rgba);
                float a = SP_RGBA32_A_F(rgba);

                float hsl[3];
                SPColor::rgb_to_hsl_floatv (hsl, r, g, b);

                gdouble val = 0;
                switch (pick) {
                case PICK_COLOR:
                    val = 1 - hsl[2]; // inverse lightness; to match other picks where black = max
                    break;
                case PICK_OPACITY:
                    val = a;
                    break;
                case PICK_R:
                    val = r;
                    break;
                case PICK_G:
                    val = g;
                    break;
                case PICK_B:
                    val = b;
                    break;
                case PICK_H:
                    val = hsl[0];
                    break;
                case PICK_S:
                    val = hsl[1];
                    break;
                case PICK_L:
                    val = 1 - hsl[2];
                    break;
                default:
                    break;
                }

                if (rand_picked > 0) {
                    val = randomize01 (val, rand_picked);
                    r = randomize01 (r, rand_picked);
                    g = randomize01 (g, rand_picked);
                    b = randomize01 (b, rand_picked);
                }

                if (gamma_picked != 0) {
                    double power;
                    if (gamma_picked > 0)
                        power = 1/(1 + fabs(gamma_picked));
                    else
                        power = 1 + fabs(gamma_picked);

                    val = pow (val, power);
                    r = pow (r, power);
                    g = pow (g, power);
                    b = pow (b, power);
                }

                if (invert_picked) {
                    val = 1 - val;
                    r = 1 - r;
                    g = 1 - g;
                    b = 1 - b;
                }

                val = CLAMP (val, 0, 1);
                r = CLAMP (r, 0, 1);
                g = CLAMP (g, 0, 1);
                b = CLAMP (b, 0, 1);

                // recompose tweaked color
                rgba = SP_RGBA32_F_COMPOSE(r, g, b, a);

                if (pick_to_presence) {
                    if (g_random_double_range (0, 1) > val) {
                        continue; // skip!
                    }
                }
                if (pick_to_size) {
                    t = parent_transform * Geom::Translate(-center[Geom::X], -center[Geom::Y])
                    * Geom::Scale (val, val) * Geom::Translate(center[Geom::X], center[Geom::Y])
                    * parent_transform.inverse() * t;
                }
                if (pick_to_opacity) {
                    opacity *= val;
                }
                if (pick_to_color) {
                    sp_svg_write_color(color_string, sizeof(color_string), rgba);
                }
            }

            if (opacity < 1e-6) { // invisibly transparent, skip
                continue;
            }

            if (fabs(t[0]) + fabs (t[1]) + fabs(t[2]) + fabs(t[3]) < 1e-6) { // too small, skip
                continue;
            }

            // Create the clone
            Inkscape::XML::Node *clone = obj_repr->document()->createElement("svg:use");
            clone->setAttribute("x", "0");
            clone->setAttribute("y", "0");
            clone->setAttribute("inkscape:tiled-clone-of", id_href);
            clone->setAttribute("xlink:href", id_href);

            Geom::Point new_center;
            bool center_set = false;
            if (obj_repr->attribute("inkscape:transform-center-x") || obj_repr->attribute("inkscape:transform-center-y")) {
                new_center = scale_units*desktop->dt2doc(item->getCenter()) * orig_t;
                center_set = true;
            }

            clone->setAttributeOrRemoveIfEmpty("transform", sp_svg_transform_write(t));

            if (opacity < 1.0) {
                clone->setAttributeCssDouble("opacity", opacity);
            }

            if (*color_string) {
                clone->setAttribute("fill", color_string);
                clone->setAttribute("stroke", color_string);
            }

            // add the new clone to the top of the original's parent
            parent->getRepr()->appendChild(clone);

            if (blur > 0.0) {
                SPObject *clone_object = desktop->getDocument()->getObjectByRepr(clone);
                SPItem *item = dynamic_cast<SPItem *>(clone_object);
                double radius = blur * perimeter_original * t.descrim();
                // this is necessary for all newly added clones to have correct bboxes,
                // otherwise filters won't work:
                desktop->getDocument()->ensureUpToDate();
                SPFilter *constructed = new_filter_gaussian_blur(desktop->getDocument(), radius, t.descrim());
                constructed->update_filter_region(item);
                sp_style_set_property_url (clone_object, "filter", constructed, false);
            }

            if (center_set) {
                SPObject *clone_object = desktop->getDocument()->getObjectByRepr(clone);
                SPItem *item = dynamic_cast<SPItem *>(clone_object);
                if (clone_object && item) {
                    clone_object->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
                    item->setCenter(desktop->doc2dt(new_center));
                    clone_object->updateRepr();
                }
            }

            Inkscape::GC::release(clone);
        }
        cur[Geom::Y] = 0;
    }

    if (dotrace) {
        trace_finish ();
    }

    change_selection(selection);

    desktop->clearWaitingCursor();
    DocumentUndo::done(getDocument(), SP_VERB_DIALOG_CLONETILER, _("Create tiled clones"));
}

Gtk::Box * CloneTiler::new_tab(Gtk::Notebook *nb, const gchar *label)
{
    auto l = Gtk::manage(new Gtk::Label(label, true));
    auto vb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL, VB_MARGIN));
    vb->set_homogeneous(false);
    vb->set_border_width(VB_MARGIN);
    nb->append_page(*vb, *l);
    return vb;
}

void CloneTiler::checkbox_toggled(Gtk::ToggleButton   *tb,
                                  const Glib::ustring &attr)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(prefs_path + attr, tb->get_active());
}

Gtk::Widget * CloneTiler::checkbox(const char          *tip,
                                   const Glib::ustring &attr)
{
    auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, VB_MARGIN));
    auto b  = Gtk::manage(new UI::Widget::CheckButtonInternal());
    b->set_tooltip_text(tip);

    auto const prefs = Inkscape::Preferences::get();
    auto const value = prefs->getBool(prefs_path + attr);
    b->set_active(value);

    hb->pack_start(*b, false, true);
    b->signal_clicked().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::checkbox_toggled), b, attr));

    b->set_uncheckable();

    return hb;
}

void CloneTiler::value_changed(Glib::RefPtr<Gtk::Adjustment> &adj,
                               Glib::ustring const           &pref)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble(prefs_path + pref, adj->get_value());
}

Gtk::Widget * CloneTiler::spinbox(const char          *tip,
                                  const Glib::ustring &attr,
                                  double               lower,
                                  double               upper,
                                  const gchar         *suffix,
                                  bool                 exponent/* = false*/)
{
    auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));

    {
        // Parameters for adjustment
        auto const initial_value  = (exponent ? 1.0 : 0.0);
        auto const step_increment = (exponent ? 0.01 : 0.1);
        auto const page_increment = (exponent ? 0.05 : 0.4);

        auto a = Gtk::Adjustment::create(initial_value,
                                         lower,
                                         upper,
                                         step_increment,
                                         page_increment);

        auto const climb_rate = (exponent ? 0.01 : 0.1);
        auto const digits = (exponent ? 2 : 1);

        auto sb = new Inkscape::UI::Widget::SpinButton(a, climb_rate, digits);

        sb->set_tooltip_text (tip);
        sb->set_width_chars (5);
        sb->set_digits(3);
        hb->pack_start(*sb, false, false, SB_MARGIN);

        auto prefs = Inkscape::Preferences::get();
        auto value = prefs->getDoubleLimited(prefs_path + attr, exponent? 1.0 : 0.0, lower, upper);
        a->set_value (value);
        a->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &CloneTiler::value_changed), a, attr));

        if (exponent) {
            sb->set_oneable();
        } else {
            sb->set_zeroable();
        }
    }

    {
        auto l = Gtk::manage(new Gtk::Label(""));
        l->set_markup(suffix);
        hb->pack_start(*l);
    }

    return hb;
}

void CloneTiler::symgroup_changed(Gtk::ComboBox *cb)
{
    auto prefs = Inkscape::Preferences::get();
    auto group_new = cb->get_active_row_number();
    prefs->setInt(prefs_path + "symmetrygroup", group_new);
}

void CloneTiler::xy_changed(Glib::RefPtr<Gtk::Adjustment> &adj, Glib::ustring const &pref)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setInt(prefs_path + pref, (int) floor(adj->get_value() + 0.5));
}

void CloneTiler::keep_bbox_toggled()
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(prefs_path + "keepbbox", _cb_keep_bbox->get_active());
}

void CloneTiler::pick_to(Gtk::ToggleButton *tb, Glib::ustring const &pref)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(prefs_path + pref, tb->get_active());
}


void CloneTiler::reset_recursive(Gtk::Widget *w)
{
    if (w) {
        auto sb = dynamic_cast<Inkscape::UI::Widget::SpinButton *>(w);
        auto tb = dynamic_cast<Inkscape::UI::Widget::CheckButtonInternal *>(w);

        {
            if (sb && sb->get_zeroable()) { // spinbutton
                auto a = sb->get_adjustment();
                a->set_value(0);
            }
        }
        {
            if (sb && sb->get_oneable()) { // spinbutton
                auto a = sb->get_adjustment();
                a->set_value(1);
            }
        }
        {
            if (tb && tb->get_uncheckable()) { // checkbox
                tb->set_active(false);
            }
        }
    }

    auto container = dynamic_cast<Gtk::Container *>(w);

    if (container) {
        auto c = container->get_children();
        for (auto i : c) {
            reset_recursive(i);
        }
    }
}

void CloneTiler::reset()
{
    reset_recursive(this);
}

void CloneTiler::table_attach(Gtk::Grid *table, Gtk::Widget *widget, float align, int row, int col)
{
    widget->set_halign(Gtk::ALIGN_FILL);
    widget->set_valign(Gtk::ALIGN_CENTER);
    table->attach(*widget, col, row, 1, 1);
}

Gtk::Grid * CloneTiler::table_x_y_rand(int values)
{
    auto table = Gtk::manage(new Gtk::Grid());
    table->set_row_spacing(6);
    table->set_column_spacing(8);

    table->set_border_width(VB_MARGIN);

    {
	auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
	hb->set_homogeneous(false);

        auto i = Glib::wrap(sp_get_icon_image("object-rows", GTK_ICON_SIZE_MENU));
        hb->pack_start(*i, false, false, 2);

        auto l = Gtk::manage(new Gtk::Label(""));
        l->set_markup(_("<small>Per row:</small>"));
        hb->pack_start(*l, false, false, 2);

        table_attach(table, hb, 0, 1, 2);
    }

    {
	auto hb = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 0));
	hb->set_homogeneous(false);

        auto i = Glib::wrap(sp_get_icon_image("object-columns", GTK_ICON_SIZE_MENU));
        hb->pack_start(*i, false, false, 2);

        auto l = Gtk::manage(new Gtk::Label(""));
        l->set_markup(_("<small>Per column:</small>"));
        hb->pack_start(*l, false, false, 2);

        table_attach(table, hb, 0, 1, 3);
    }

    {
        auto l = Gtk::manage(new Gtk::Label(""));
        l->set_markup(_("<small>Randomize:</small>"));
        table_attach(table, l, 0, 1, 4);
    }

    return table;
}

void CloneTiler::pick_switched(PickType v)
{
    auto prefs = Inkscape::Preferences::get();
    prefs->setInt(prefs_path + "pick", v);
}

void CloneTiler::switch_to_create()
{
    if (_rowscols) {
        _rowscols->set_sensitive(true);
    }
    if (_widthheight) {
        _widthheight->set_sensitive(false);
    }

    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(prefs_path + "fillrect", false);
}


void CloneTiler::switch_to_fill()
{
    if (_rowscols) {
        _rowscols->set_sensitive(false);
    }
    if (_widthheight) {
        _widthheight->set_sensitive(true);
    }

    auto prefs = Inkscape::Preferences::get();
    prefs->setBool(prefs_path + "fillrect", true);
}

void CloneTiler::fill_width_changed()
{
    auto const raw_dist = fill_width->get_value();
    auto const unit     = unit_menu->getUnit();
    auto const pixels   = Inkscape::Util::Quantity::convert(raw_dist, unit, "px");

    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble(prefs_path + "fillwidth", pixels);
}

void CloneTiler::fill_height_changed()
{
    auto const raw_dist = fill_height->get_value();
    auto const unit     = unit_menu->getUnit();
    auto const pixels   = Inkscape::Util::Quantity::convert(raw_dist, unit, "px");

    auto prefs = Inkscape::Preferences::get();
    prefs->setDouble(prefs_path + "fillheight", pixels);
}

void CloneTiler::unit_changed()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gdouble width_pixels = prefs->getDouble(prefs_path + "fillwidth");
    gdouble height_pixels = prefs->getDouble(prefs_path + "fillheight");

    Inkscape::Util::Unit const *unit = unit_menu->getUnit();

    gdouble width_value = Inkscape::Util::Quantity::convert(width_pixels, "px", unit);
    gdouble height_value = Inkscape::Util::Quantity::convert(height_pixels, "px", unit);
    fill_width->set_value(width_value);
    fill_height->set_value(height_value);
}

void CloneTiler::do_pick_toggled()
{
    auto prefs  = Inkscape::Preferences::get();
    auto active = _b->get_active();
    prefs->setBool(prefs_path + "dotrace", active);

    if (_dotrace) {
        _dotrace->set_sensitive(active);
    }
}

void CloneTiler::show_page_trace()
{
    nb->set_current_page(6);
    _b->set_active(false);
}


}
}
}


/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
