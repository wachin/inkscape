// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Static style swatch (fill, stroke, opacity).
 */
/* Authors:
 *   buliabyak@gmail.com
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2005-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "style-swatch.h"

#include <glibmm/i18n.h>
#include <gtkmm/enums.h>
#include <gtkmm/grid.h>

#include "inkscape.h"
#include "style.h"

#include "actions/actions-tools.h"  // Open tool preferences.

#include "object/sp-linear-gradient.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"

#include "ui/widget/color-preview.h"
#include "util/units.h"

#include "widgets/spw-utilities.h"

#include "xml/sp-css-attr.h"
#include "xml/attribute-record.h"

enum {
    SS_FILL,
    SS_STROKE
};

namespace Inkscape {
namespace UI {
namespace Widget {

/**
 * Watches whether the tool uses the current style.
 */
class StyleSwatch::ToolObserver : public Inkscape::Preferences::Observer {
public:
    ToolObserver(Glib::ustring const &path, StyleSwatch &ss) : 
        Observer(path),
        _style_swatch(ss)
    {}
    void notify(Inkscape::Preferences::Entry const &val) override;
private:
    StyleSwatch &_style_swatch;
};

/**
 * Watches for changes in the observed style pref.
 */
class StyleSwatch::StyleObserver : public Inkscape::Preferences::Observer {
public:
    StyleObserver(Glib::ustring const &path, StyleSwatch &ss) :
        Observer(path),
        _style_swatch(ss)
    {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        this->notify(prefs->getEntry(path));
    }
    void notify(Inkscape::Preferences::Entry const &val) override {
        SPCSSAttr *css = val.getInheritedStyle();
        _style_swatch.setStyle(css);
        sp_repr_css_attr_unref(css);
    }
private:
    StyleSwatch &_style_swatch;
};

void StyleSwatch::ToolObserver::notify(Inkscape::Preferences::Entry const &val)
{
    bool usecurrent = val.getBool();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (_style_swatch._style_obs) delete _style_swatch._style_obs;

    if (usecurrent) {
        _style_swatch._style_obs = new StyleObserver("/desktop/style", _style_swatch);
        
        // If desktop's last-set style is empty, a tool uses its own fixed style even if set to use
        // last-set (so long as it's empty). To correctly show this, we get the tool's style
        // if the desktop's style is empty.
        SPCSSAttr *css = prefs->getStyle("/desktop/style");
        const auto & al = css->attributeList();
        if (al.empty()) {
            SPCSSAttr *css2 = prefs->getInheritedStyle(_style_swatch._tool_path + "/style");
            _style_swatch.setStyle(css2);
            sp_repr_css_attr_unref(css2);
        }
        sp_repr_css_attr_unref(css);
    } else {
        _style_swatch._style_obs = new StyleObserver(_style_swatch._tool_path + "/style", _style_swatch);
    }
    prefs->addObserver(*_style_swatch._style_obs);
}

StyleSwatch::StyleSwatch(SPCSSAttr *css, gchar const *main_tip, Gtk::Orientation orient)
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL),
      _desktop(nullptr),
      _css(nullptr),
      _tool_obs(nullptr),
      _style_obs(nullptr),
      _table(Gtk::manage(new Gtk::Grid())),
      _sw_unit(nullptr),
      _stroke(Gtk::ORIENTATION_HORIZONTAL)
{
    set_name("StyleSwatch");
    _label[SS_FILL].set_markup(_("Fill:"));
    _label[SS_STROKE].set_markup(_("Stroke:"));

    for (int i = SS_FILL; i <= SS_STROKE; i++) {
        _label[i].set_halign(Gtk::ALIGN_START);
        _label[i].set_valign(Gtk::ALIGN_CENTER);
        _label[i].set_margin_top(0);
        _label[i].set_margin_bottom(0);
        _label[i].set_margin_start(0);
        _label[i].set_margin_end(0);

        _color_preview[i] = new Inkscape::UI::Widget::ColorPreview (0);
    }

    _opacity_value.set_halign(Gtk::ALIGN_START);
    _opacity_value.set_valign(Gtk::ALIGN_CENTER);
    _opacity_value.set_margin_top(0);
    _opacity_value.set_margin_bottom(0);
    _opacity_value.set_margin_start(0);
    _opacity_value.set_margin_end(0);

    _table->set_column_spacing(2);
    _table->set_row_spacing(0);

    _stroke.pack_start(_place[SS_STROKE]);
    _stroke_width_place.add(_stroke_width);
    _stroke.pack_start(_stroke_width_place, Gtk::PACK_SHRINK);
    
    _opacity_place.add(_opacity_value);

    if (orient == Gtk::ORIENTATION_VERTICAL) {
        _table->attach(_label[SS_FILL],   0, 0, 1, 1);
        _table->attach(_label[SS_STROKE], 0, 1, 1, 1);
        _table->attach(_place[SS_FILL],   1, 0, 1, 1);
        _table->attach(_stroke,           1, 1, 1, 1);
        _table->attach(_empty_space,      2, 0, 1, 2);
        _table->attach(_opacity_place,    2, 0, 1, 2);
        _swatch.add(*_table);
        pack_start(_swatch, true, true, 0);

        set_size_request (STYLE_SWATCH_WIDTH, -1);
    }
    else {
        _table->set_column_spacing(4);
        _table->attach(_label[SS_FILL],   0, 0, 1, 1);
        _table->attach(_place[SS_FILL],   1, 0, 1, 1);
        _label[SS_STROKE].set_margin_start(6);
        _table->attach(_label[SS_STROKE], 2, 0, 1, 1);
        _table->attach(_stroke,           3, 0, 1, 1);
        _opacity_place.set_margin_start(6);
        _table->attach(_opacity_place,    4, 0, 1, 1);
        _swatch.add(*_table);
        pack_start(_swatch, true, true, 0);

        int patch_w = 6 * 6;
        _place[SS_FILL].set_size_request(patch_w, -1);
        _place[SS_STROKE].set_size_request(patch_w, -1);
    }

    setStyle (css);

    _swatch.signal_button_press_event().connect(sigc::mem_fun(*this, &StyleSwatch::on_click));

    if (main_tip)
    {
        _swatch.set_tooltip_text(main_tip);
    }
}

void StyleSwatch::setToolName(const Glib::ustring& tool_name) {
    _tool_name = tool_name;
}

void StyleSwatch::setDesktop(SPDesktop *desktop) {
    _desktop = desktop;
}

bool
StyleSwatch::on_click(GdkEventButton */*event*/)
{
    if (_desktop && !_tool_name.empty()) {
        auto win = _desktop->getInkscapeWindow();
        open_tool_preferences(win, _tool_name);
        return true;
    }
    return false;
}

StyleSwatch::~StyleSwatch()
{
    if (_css)
        sp_repr_css_attr_unref (_css);

    for (int i = SS_FILL; i <= SS_STROKE; i++) {
        delete _color_preview[i];
    }

    if (_style_obs) delete _style_obs;
    if (_tool_obs) delete _tool_obs;
}

void
StyleSwatch::setWatchedTool(const char *path, bool synthesize)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    
    if (_tool_obs) {
        delete _tool_obs;
        _tool_obs = nullptr;
    }

    if (path) {
        _tool_path = path;
        _tool_obs = new ToolObserver(_tool_path + "/usecurrent", *this);
        prefs->addObserver(*_tool_obs);
    } else {
        _tool_path = "";
    }
    
    // hack until there is a real synthesize events function for prefs,
    // which shouldn't be hard to write once there is sufficient need for it
    if (synthesize && _tool_obs) {
        _tool_obs->notify(prefs->getEntry(_tool_path + "/usecurrent"));
    }
}


void StyleSwatch::setStyle(SPCSSAttr *css)
{
    if (_css)
        sp_repr_css_attr_unref (_css);

    if (!css)
        return;

    _css = sp_repr_css_attr_new();
    sp_repr_css_merge(_css, css);

    Glib::ustring css_string;
    sp_repr_css_write_string (_css, css_string);

    SPStyle style(_desktop ? _desktop->getDocument() : nullptr);
    if (!css_string.empty()) {
        style.mergeString(css_string.c_str());
    }
    setStyle (&style);
}

void StyleSwatch::setStyle(SPStyle *query)
{
    _place[SS_FILL].remove();
    _place[SS_STROKE].remove();

    bool has_stroke = true;

    for (int i = SS_FILL; i <= SS_STROKE; i++) {
        Gtk::EventBox *place = &(_place[i]);

        SPIPaint *paint;
        if (i == SS_FILL) {
            paint = &(query->fill);
        } else {
            paint = &(query->stroke);
        }

        if (paint->set && paint->isPaintserver()) {
            SPPaintServer *server = (i == SS_FILL)? SP_STYLE_FILL_SERVER (query) : SP_STYLE_STROKE_SERVER (query);

            if (is<SPLinearGradient>(server)) {
                _value[i].set_markup(_("L Gradient"));
                place->add(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Linear gradient (fill)")) : (_("Linear gradient (stroke)")));
            } else if (is<SPRadialGradient>(server)) {
                _value[i].set_markup(_("R Gradient"));
                place->add(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Radial gradient (fill)")) : (_("Radial gradient (stroke)")));
            } else if (is<SPPattern>(server)) {
                _value[i].set_markup(_("Pattern"));
                place->add(_value[i]);
                place->set_tooltip_text((i == SS_FILL)? (_("Pattern (fill)")) : (_("Pattern (stroke)")));
            }

        } else if (paint->set && paint->isColor()) {
            guint32 color = paint->value.color.toRGBA32( SP_SCALE24_TO_FLOAT ((i == SS_FILL)? query->fill_opacity.value : query->stroke_opacity.value) );
            ((Inkscape::UI::Widget::ColorPreview*)_color_preview[i])->setRgba32 (color);
            _color_preview[i]->show_all();
            place->add(*_color_preview[i]);
            gchar *tip;
            if (i == SS_FILL) {
                tip = g_strdup_printf (_("Fill: %06x/%.3g"), color >> 8, SP_RGBA32_A_F(color));
            } else {
                tip = g_strdup_printf (_("Stroke: %06x/%.3g"), color >> 8, SP_RGBA32_A_F(color));
            }
            place->set_tooltip_text(tip);
            g_free (tip);
        } else if (paint->set && paint->isNone()) {
            _value[i].set_markup(C_("Fill and stroke", "<i>None</i>"));
            place->add(_value[i]);
            place->set_tooltip_text((i == SS_FILL)? (C_("Fill and stroke", "No fill")) : (C_("Fill and stroke", "No stroke")));
            if (i == SS_STROKE) has_stroke = false;
        } else if (!paint->set) {
            _value[i].set_markup(_("<b>Unset</b>"));
            place->add(_value[i]);
            place->set_tooltip_text((i == SS_FILL)? (_("Unset fill")) : (_("Unset stroke")));
            if (i == SS_STROKE) has_stroke = false;
        }
    }

// Now query stroke_width
    if (has_stroke) {
        if (query->stroke_extensions.hairline) {
            Glib::ustring swidth = "<small>";
            swidth += _("Hairline");
            swidth += "</small>";
            _stroke_width.set_markup(swidth.c_str());
            auto str = Glib::ustring::compose(_("Stroke width: %1"), _("Hairline"));
            _stroke_width_place.set_tooltip_text(str);
        } else {
            double w;
            if (_sw_unit) {
                w = Inkscape::Util::Quantity::convert(query->stroke_width.computed, "px", _sw_unit);
            } else {
                w = query->stroke_width.computed;
            }

            {
                gchar *str = g_strdup_printf(" %.3g", w);
                Glib::ustring swidth = "<small>";
                swidth += str;
                swidth += "</small>";
                _stroke_width.set_markup(swidth.c_str());
                g_free (str);
            }
            {
                gchar *str = g_strdup_printf(_("Stroke width: %.5g%s"),
                                             w,
                                             _sw_unit? _sw_unit->abbr.c_str() : "px");
                _stroke_width_place.set_tooltip_text(str);
                g_free (str);
            }
        }
    } else {
        _stroke_width_place.set_tooltip_text("");
        _stroke_width.set_markup("");
        _stroke_width.set_has_tooltip(false);
    }

    gdouble op = SP_SCALE24_TO_FLOAT(query->opacity.value);
    if (op != 1) {
        {
            gchar *str;
            str = g_strdup_printf(_("O: %2.0f"), (op*100.0));
            Glib::ustring opacity = "<small>";
            opacity += str;
            opacity += "</small>";
            _opacity_value.set_markup (opacity.c_str());
            g_free (str);
        }
        {
            gchar *str = g_strdup_printf(_("Opacity: %2.1f %%"), (op*100.0));
            _opacity_place.set_tooltip_text(str);
            g_free (str);
        }
    } else {
        _opacity_place.set_tooltip_text("");
        _opacity_value.set_markup("");
        _opacity_value.set_has_tooltip(false);
    }

    show_all();
}

} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
