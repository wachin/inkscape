// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Selector aux toolbar
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2003-2005 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "select-toolbar.h"

#include <glibmm/i18n.h>

#include <gtkmm/adjustment.h>
#include <gtkmm/separatortoolitem.h>

#include <2geom/rect.h>

#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "selection.h"
#include "message-stack.h"
#include "selection-chemistry.h"
#include "verbs.h"


#include "object/sp-item-transform.h"
#include "object/sp-namedview.h"

#include "ui/icon-names.h"
#include "ui/widget/canvas.h" // Focus widget
#include "ui/widget/combo-tool-item.h"
#include "ui/widget/spin-button-tool-item.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/unit-tracker.h"

#include "widgets/widget-sizes.h"

using Inkscape::UI::Widget::UnitTracker;
using Inkscape::Util::Unit;
using Inkscape::Util::Quantity;
using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;

namespace Inkscape {
namespace UI {
namespace Toolbar {

SelectToolbar::SelectToolbar(SPDesktop *desktop) :
    Toolbar(desktop),
    _tracker(new UnitTracker(Inkscape::Util::UNIT_TYPE_LINEAR)),
    _update(false),
    _lock_btn(Gtk::manage(new Gtk::ToggleToolButton())),
    _select_touch_btn(Gtk::manage(new Gtk::ToggleToolButton())),
    _transform_stroke_btn(Gtk::manage(new Gtk::ToggleToolButton())),
    _transform_corners_btn(Gtk::manage(new Gtk::ToggleToolButton())),
    _transform_gradient_btn(Gtk::manage(new Gtk::ToggleToolButton())),
    _transform_pattern_btn(Gtk::manage(new Gtk::ToggleToolButton()))
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    add_toolbutton_for_verb(SP_VERB_EDIT_SELECT_ALL);
    add_toolbutton_for_verb(SP_VERB_EDIT_SELECT_ALL_IN_ALL_LAYERS);
    auto deselect_button                 = add_toolbutton_for_verb(SP_VERB_EDIT_DESELECT);
    _context_items.push_back(deselect_button);

    _select_touch_btn->set_label(_("Select by touch"));
    _select_touch_btn->set_tooltip_text(_("Toggle selection box to select all touched objects."));
    _select_touch_btn->set_icon_name(INKSCAPE_ICON("selection-touch"));
    _select_touch_btn->set_active(prefs->getBool("/tools/select/touch_box", false));
    _select_touch_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_touch));

    add(*_select_touch_btn);

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    auto object_rotate_90_ccw_button     = add_toolbutton_for_verb(SP_VERB_OBJECT_ROTATE_90_CCW);
    _context_items.push_back(object_rotate_90_ccw_button);

    auto object_rotate_90_cw_button      = add_toolbutton_for_verb(SP_VERB_OBJECT_ROTATE_90_CW);
    _context_items.push_back(object_rotate_90_cw_button);

    auto object_flip_horizontal_button   = add_toolbutton_for_verb(SP_VERB_OBJECT_FLIP_HORIZONTAL);
    _context_items.push_back(object_flip_horizontal_button);

    auto object_flip_vertical_button     = add_toolbutton_for_verb(SP_VERB_OBJECT_FLIP_VERTICAL);
    _context_items.push_back(object_flip_vertical_button);

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    auto selection_to_front_button       = add_toolbutton_for_verb(SP_VERB_SELECTION_TO_FRONT);
    _context_items.push_back(selection_to_front_button);

    auto selection_raise_button          = add_toolbutton_for_verb(SP_VERB_SELECTION_RAISE);
    _context_items.push_back(selection_raise_button);

    auto selection_lower_button          = add_toolbutton_for_verb(SP_VERB_SELECTION_LOWER);
    _context_items.push_back(selection_lower_button);

    auto selection_to_back_button        = add_toolbutton_for_verb(SP_VERB_SELECTION_TO_BACK);
    _context_items.push_back(selection_to_back_button);

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    _tracker->addUnit(unit_table.getUnit("%"));
    _tracker->setActiveUnit( desktop->getNamedView()->display_units );

    // x-value control
    auto x_val = prefs->getDouble("/tools/select/X", 0.0);
    _adj_x = Gtk::Adjustment::create(x_val, -1e6, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
    _adj_x->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &SelectToolbar::any_value_changed), _adj_x));
    _tracker->addAdjustment(_adj_x->gobj());

    auto x_btn = Gtk::manage(new UI::Widget::SpinButtonToolItem("select-x",
                                                                C_("Select toolbar", "X:"),
                                                                _adj_x,
                                                                SPIN_STEP, 3));
    x_btn->get_spin_button()->addUnitTracker(_tracker.get());
    x_btn->set_focus_widget(_desktop->getCanvas());
    x_btn->set_all_tooltip_text(C_("Select toolbar", "Horizontal coordinate of selection"));
    _context_items.push_back(x_btn);
    add(*x_btn);

    // y-value control
    auto y_val = prefs->getDouble("/tools/select/Y", 0.0);
    _adj_y = Gtk::Adjustment::create(y_val, -1e6, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
    _adj_y->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &SelectToolbar::any_value_changed), _adj_y));
    _tracker->addAdjustment(_adj_y->gobj());

    auto y_btn = Gtk::manage(new UI::Widget::SpinButtonToolItem("select-y",
                                                                C_("Select toolbar", "Y:"),
                                                                _adj_y,
                                                                SPIN_STEP, 3));
    y_btn->get_spin_button()->addUnitTracker(_tracker.get());
    y_btn->set_focus_widget(_desktop->getCanvas());
    y_btn->set_all_tooltip_text(C_("Select toolbar", "Vertical coordinate of selection"));
    _context_items.push_back(y_btn);
    add(*y_btn);

    // width-value control
    auto w_val = prefs->getDouble("/tools/select/width", 0.0);
    _adj_w = Gtk::Adjustment::create(w_val, 0.0, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
    _adj_w->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &SelectToolbar::any_value_changed), _adj_w));
    _tracker->addAdjustment(_adj_w->gobj());

    auto w_btn = Gtk::manage(new UI::Widget::SpinButtonToolItem("select-width",
                                                                C_("Select toolbar", "W:"),
                                                                _adj_w,
                                                                SPIN_STEP, 3));
    w_btn->get_spin_button()->addUnitTracker(_tracker.get());
    w_btn->set_focus_widget(_desktop->getCanvas());
    w_btn->set_all_tooltip_text(C_("Select toolbar", "Width of selection"));
    _context_items.push_back(w_btn);
    add(*w_btn);

    // lock toggle
    _lock_btn->set_label(_("Lock width and height"));
    _lock_btn->set_tooltip_text(_("When locked, change both width and height by the same proportion"));
    _lock_btn->set_icon_name(INKSCAPE_ICON("object-unlocked"));
    _lock_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_lock));
    _lock_btn->set_name("lock");
    add(*_lock_btn);

    // height-value control
    auto h_val = prefs->getDouble("/tools/select/height", 0.0);
    _adj_h = Gtk::Adjustment::create(h_val, 0.0, 1e6, SPIN_STEP, SPIN_PAGE_STEP);
    _adj_h->signal_value_changed().connect(sigc::bind(sigc::mem_fun(*this, &SelectToolbar::any_value_changed), _adj_h));
    _tracker->addAdjustment(_adj_h->gobj());

    auto h_btn = Gtk::manage(new UI::Widget::SpinButtonToolItem("select-height",
                                                                C_("Select toolbar", "H:"),
                                                                _adj_h,
                                                                SPIN_STEP, 3));
    h_btn->get_spin_button()->addUnitTracker(_tracker.get());
    h_btn->set_focus_widget(_desktop->getCanvas());
    h_btn->set_all_tooltip_text(C_("Select toolbar", "Height of selection"));
    _context_items.push_back(h_btn);
    add(*h_btn);

    // units menu
    auto unit_menu = _tracker->create_tool_item(_("Units"), ("") );
    add(*unit_menu);

    add(* Gtk::manage(new Gtk::SeparatorToolItem()));

    _transform_stroke_btn->set_label(_("Scale stroke width"));
    _transform_stroke_btn->set_tooltip_text(_("When scaling objects, scale the stroke width by the same proportion"));
    _transform_stroke_btn->set_icon_name(INKSCAPE_ICON("transform-affect-stroke"));
    _transform_stroke_btn->set_active(prefs->getBool("/options/transform/stroke", true));
    _transform_stroke_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_stroke));
    add(*_transform_stroke_btn);

    _transform_corners_btn->set_label(_("Scale rounded corners"));
    _transform_corners_btn->set_tooltip_text(_("When scaling rectangles, scale the radii of rounded corners"));
    _transform_corners_btn->set_icon_name(INKSCAPE_ICON("transform-affect-rounded-corners"));
    _transform_corners_btn->set_active(prefs->getBool("/options/transform/rectcorners", true));
    _transform_corners_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_corners));
    add(*_transform_corners_btn);

    _transform_gradient_btn->set_label(_("Move gradients"));
    _transform_gradient_btn->set_tooltip_text(_("Move gradients (in fill or stroke) along with the objects"));
    _transform_gradient_btn->set_icon_name(INKSCAPE_ICON("transform-affect-gradient"));
    _transform_gradient_btn->set_active(prefs->getBool("/options/transform/gradient", true));
    _transform_gradient_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_gradient));
    add(*_transform_gradient_btn);

    _transform_pattern_btn->set_label(_("Move patterns"));
    _transform_pattern_btn->set_tooltip_text(_("Move patterns (in fill or stroke) along with the objects"));
    _transform_pattern_btn->set_icon_name(INKSCAPE_ICON("transform-affect-pattern"));
    _transform_pattern_btn->set_active(prefs->getBool("/options/transform/pattern", true));
    _transform_pattern_btn->signal_toggled().connect(sigc::mem_fun(*this, &SelectToolbar::toggle_pattern));
    add(*_transform_pattern_btn);

    assert(desktop);
    auto *selection = desktop->getSelection();

    // Force update when selection changes.
    _connections.emplace_back( //
        selection->connectModified(sigc::mem_fun(*this, &SelectToolbar::on_inkscape_selection_modified)));
    _connections.emplace_back(
        selection->connectChanged(sigc::mem_fun(*this, &SelectToolbar::on_inkscape_selection_changed)));

    // Update now.
    layout_widget_update(selection);

    for (auto item : _context_items) {
        if ( item->is_sensitive() ) {
            item->set_sensitive(false);
        }
    }

    show_all();
}

void SelectToolbar::on_unrealize()
{
    for (auto &conn : _connections) {
        conn.disconnect();
    }

    parent_type::on_unrealize();
}

GtkWidget *
SelectToolbar::create(SPDesktop *desktop)
{
    auto toolbar = new SelectToolbar(desktop);
    return GTK_WIDGET(toolbar->gobj());
}

void
SelectToolbar::any_value_changed(Glib::RefPtr<Gtk::Adjustment>& adj)
{
    if (_update) {
        return;
    }

    if ( !_tracker || _tracker->isUpdating() ) {
        /*
         * When only units are being changed, don't treat changes
         * to adjuster values as object changes.
         */
        return;
    }
    _update = true;

    SPDesktop *desktop = _desktop;
    Inkscape::Selection *selection = desktop->getSelection();
    SPDocument *document = desktop->getDocument();

    document->ensureUpToDate ();

    Geom::OptRect bbox_vis = selection->visualBounds();
    Geom::OptRect bbox_geom = selection->geometricBounds();
    Geom::OptRect bbox_user = selection->preferredBounds();

    if ( !bbox_user ) {
        _update = false;
        return;
    }

    Unit const *unit = _tracker->getActiveUnit();
    g_return_if_fail(unit != nullptr);

    gdouble old_w = bbox_user->dimensions()[Geom::X];
    gdouble old_h = bbox_user->dimensions()[Geom::Y];
    gdouble new_w, new_h, new_x, new_y = 0;

    if (unit->type == Inkscape::Util::UNIT_TYPE_LINEAR) {
        new_w = Quantity::convert(_adj_w->get_value(), unit, "px");
        new_h = Quantity::convert(_adj_h->get_value(), unit, "px");
        new_x = Quantity::convert(_adj_x->get_value(), unit, "px");
        new_y = Quantity::convert(_adj_y->get_value(), unit, "px");

    } else {
        gdouble old_x = bbox_user->min()[Geom::X] + (old_w * selection->anchor_x);
        gdouble old_y = bbox_user->min()[Geom::Y] + (old_h * selection->anchor_y);

        new_x = old_x * (_adj_x->get_value() / 100 / unit->factor);
        new_y = old_y * (_adj_y->get_value() / 100 / unit->factor);
        new_w = old_w * (_adj_w->get_value() / 100 / unit->factor);
        new_h = old_h * (_adj_h->get_value() / 100 / unit->factor);
    }

    // Adjust depending on the selected anchor.
    gdouble x0 = (new_x - (old_w * selection->anchor_x)) - ((new_w - old_w) * selection->anchor_x);
    gdouble y0 = (new_y - (old_h * selection->anchor_y)) - ((new_h - old_h) * selection->anchor_y);

    gdouble x1 = x0 + new_w;
    gdouble xrel = new_w / old_w;
    gdouble y1 = y0 + new_h;
    gdouble yrel = new_h / old_h;

    // Keep proportions if lock is on
    if ( _lock_btn->get_active() ) {
        if (adj == _adj_h) {
            x1 = x0 + yrel * bbox_user->dimensions()[Geom::X];
        } else if (adj == _adj_w) {
            y1 = y0 + xrel * bbox_user->dimensions()[Geom::Y];
        }
    }

    // scales and moves, in px
    double mh = fabs(x0 - bbox_user->min()[Geom::X]);
    double sh = fabs(x1 - bbox_user->max()[Geom::X]);
    double mv = fabs(y0 - bbox_user->min()[Geom::Y]);
    double sv = fabs(y1 - bbox_user->max()[Geom::Y]);

    // unless the unit is %, convert the scales and moves to the unit
    if (unit->type == Inkscape::Util::UNIT_TYPE_LINEAR) {
        mh = Quantity::convert(mh, "px", unit);
        sh = Quantity::convert(sh, "px", unit);
        mv = Quantity::convert(mv, "px", unit);
        sv = Quantity::convert(sv, "px", unit);
    }

    // do the action only if one of the scales/moves is greater than half the last significant
    // digit in the spinbox (currently spinboxes have 3 fractional digits, so that makes 0.0005). If
    // the value was changed by the user, the difference will be at least that much; otherwise it's
    // just rounding difference between the spinbox value and actual value, so no action is
    // performed
    char const * const actionkey = ( mh > 5e-4 ? "selector:toolbar:move:horizontal" :
                                     sh > 5e-4 ? "selector:toolbar:scale:horizontal" :
                                     mv > 5e-4 ? "selector:toolbar:move:vertical" :
                                     sv > 5e-4 ? "selector:toolbar:scale:vertical" : nullptr );

    if (actionkey != nullptr) {

        // FIXME: fix for GTK breakage, see comment in SelectedStyle::on_opacity_changed
        desktop->getCanvas()->forced_redraws_start(0);

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        bool transform_stroke = prefs->getBool("/options/transform/stroke", true);
        bool preserve = prefs->getBool("/options/preservetransform/value", false);

        Geom::Affine scaler;
        if (prefs->getInt("/tools/bounding_box") == 0) { // SPItem::VISUAL_BBOX
            scaler = get_scale_transform_for_variable_stroke (*bbox_vis, *bbox_geom, transform_stroke, preserve, x0, y0, x1, y1);
        } else {
            // 1) We could have use the newer get_scale_transform_for_variable_stroke() here, but to avoid regressions
            // we'll just use the old get_scale_transform_for_uniform_stroke() for now.
            // 2) get_scale_transform_for_uniform_stroke() is intended for visual bounding boxes, not geometrical ones!
            // we'll trick it into using a geometric bounding box though, by setting the stroke width to zero
            scaler = get_scale_transform_for_uniform_stroke (*bbox_geom, 0, 0, false, false, x0, y0, x1, y1);
        }

        selection->applyAffine(scaler);
        DocumentUndo::maybeDone(document, actionkey, SP_VERB_CONTEXT_SELECT,
                                _("Transform by toolbar"));

        // resume interruptibility
        desktop->getCanvas()->forced_redraws_stop();
    }

    _update = false;
}

void
SelectToolbar::layout_widget_update(Inkscape::Selection *sel)
{
    if (_update) {
        return;
    }

    _update = true;
    using Geom::X;
    using Geom::Y;
    if ( sel && !sel->isEmpty() ) {
        Geom::OptRect const bbox(sel->preferredBounds());
        if ( bbox ) {
            Unit const *unit = _tracker->getActiveUnit();
            g_return_if_fail(unit != nullptr);

            auto width = bbox->dimensions()[X];
            auto height = bbox->dimensions()[Y];
            auto x = bbox->min()[X] + (width * sel->anchor_x);
            auto y = bbox->min()[Y] + (height * sel->anchor_y);

            if (unit->type == Inkscape::Util::UNIT_TYPE_DIMENSIONLESS) {
                double const val = unit->factor * 100;
                _adj_x->set_value(val);
                _adj_y->set_value(val);
                _adj_w->set_value(val);
                _adj_h->set_value(val);
                _tracker->setFullVal( _adj_x->gobj(), x );
                _tracker->setFullVal( _adj_y->gobj(), y );
                _tracker->setFullVal( _adj_w->gobj(), width );
                _tracker->setFullVal( _adj_h->gobj(), height );
            } else {
                _adj_x->set_value(Quantity::convert(x, "px", unit));
                _adj_y->set_value(Quantity::convert(y, "px", unit));
                _adj_w->set_value(Quantity::convert(width, "px", unit));
                _adj_h->set_value(Quantity::convert(height, "px", unit));
            }
        }
    }

    _update = false;
}

void
SelectToolbar::on_inkscape_selection_modified(Inkscape::Selection *selection, guint flags)
{
    assert(_desktop->getSelection() == selection);
    if ((flags & (SP_OBJECT_MODIFIED_FLAG        |
                     SP_OBJECT_PARENT_MODIFIED_FLAG |
                     SP_OBJECT_CHILD_MODIFIED_FLAG   )))
    {
        layout_widget_update(selection);
    }
}

void
SelectToolbar::on_inkscape_selection_changed(Inkscape::Selection *selection)
{
    assert(_desktop->getSelection() == selection);
    {
        bool setActive = (selection && !selection->isEmpty());

        for (auto item : _context_items) {
            if ( setActive != item->get_sensitive() ) {
                item->set_sensitive(setActive);
            }
        }

        layout_widget_update(selection);
    }
}

void
SelectToolbar::toggle_lock() {
    if ( _lock_btn->get_active() ) {
        _lock_btn->set_icon_name(INKSCAPE_ICON("object-locked"));
    } else {
        _lock_btn->set_icon_name(INKSCAPE_ICON("object-unlocked"));
    }
}

void
SelectToolbar::toggle_touch()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setBool("/tools/select/touch_box", _select_touch_btn->get_active());
}

void
SelectToolbar::toggle_stroke()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool active = _transform_stroke_btn->get_active();
    prefs->setBool("/options/transform/stroke", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>stroke width</b> is <b>scaled</b> when objects are scaled."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>stroke width</b> is <b>not scaled</b> when objects are scaled."));
    }
}

void
SelectToolbar::toggle_corners()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool active = _transform_corners_btn->get_active();
    prefs->setBool("/options/transform/rectcorners", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>rounded rectangle corners</b> are <b>scaled</b> when rectangles are scaled."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>rounded rectangle corners</b> are <b>not scaled</b> when rectangles are scaled."));
    }
}

void
SelectToolbar::toggle_gradient()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool active = _transform_gradient_btn->get_active();
    prefs->setBool("/options/transform/gradient", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>gradients</b> are <b>transformed</b> along with their objects when those are transformed (moved, scaled, rotated, or skewed)."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>gradients</b> remain <b>fixed</b> when objects are transformed (moved, scaled, rotated, or skewed)."));
    }
}

void
SelectToolbar::toggle_pattern()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool active = _transform_pattern_btn->get_active();
    prefs->setInt("/options/transform/pattern", active);
    if ( active ) {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>patterns</b> are <b>transformed</b> along with their objects when those are transformed (moved, scaled, rotated, or skewed)."));
    } else {
        _desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Now <b>patterns</b> remain <b>fixed</b> when objects are transformed (moved, scaled, rotated, or skewed)."));
    }
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
