// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors:
 * see git history
 *   bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <glibmm/i18n.h>

#include "ui/dialog-events.h"
#include "ui/widget/color-scales.h"
#include "ui/widget/color-slider.h"
#include "ui/widget/scrollprotected.h"

#define CSC_CHANNEL_R (1 << 0)
#define CSC_CHANNEL_G (1 << 1)
#define CSC_CHANNEL_B (1 << 2)
#define CSC_CHANNEL_A (1 << 3)
#define CSC_CHANNEL_H (1 << 0)
#define CSC_CHANNEL_S (1 << 1)
#define CSC_CHANNEL_V (1 << 2)
#define CSC_CHANNEL_C (1 << 0)
#define CSC_CHANNEL_M (1 << 1)
#define CSC_CHANNEL_Y (1 << 2)
#define CSC_CHANNEL_K (1 << 3)
#define CSC_CHANNEL_CMYKA (1 << 4)

#define CSC_CHANNELS_ALL 0

#define XPAD 4
#define YPAD 1

#define noDUMP_CHANGE_INFO 1

namespace Inkscape {
namespace UI {
namespace Widget {


static const gchar *sp_color_scales_hue_map();

const gchar *ColorScales::SUBMODE_NAMES[] = { N_("None"), N_("RGB"), N_("HSL"), N_("CMYK"), N_("HSV") };

ColorScales::ColorScales(SelectedColor &color, SPColorScalesMode mode)
    : Gtk::Grid()
    , _color(color)
    , _rangeLimit(255.0)
    , _updating(FALSE)
    , _dragging(FALSE)
    , _mode(SP_COLOR_SCALES_MODE_NONE)
{
    for (gint i = 0; i < 5; i++) {
        _l[i] = nullptr;
        _s[i] = nullptr;
        _b[i] = nullptr;
    }

    _initUI(mode);

    _color.signal_changed.connect(sigc::mem_fun(this, &ColorScales::_onColorChanged));
    _color.signal_dragged.connect(sigc::mem_fun(this, &ColorScales::_onColorChanged));
}

ColorScales::~ColorScales()
{
    for (gint i = 0; i < 5; i++) {
        _l[i] = nullptr;
        _s[i] = nullptr;
        _b[i] = nullptr;
    }
}

void ColorScales::_initUI(SPColorScalesMode mode)
{
    gint i;

    _updating = FALSE;
    _dragging = FALSE;

    GtkWidget *t = GTK_WIDGET(gobj());

    /* Create components */
    for (i = 0; i < 5; i++) {
        /* Label */
        _l[i] = gtk_label_new("");

        gtk_widget_set_halign(_l[i], GTK_ALIGN_START);
        gtk_widget_show(_l[i]);

        gtk_widget_set_margin_start(_l[i], XPAD);
        gtk_widget_set_margin_end(_l[i], XPAD);
        gtk_widget_set_margin_top(_l[i], YPAD);
        gtk_widget_set_margin_bottom(_l[i], YPAD);
        gtk_grid_attach(GTK_GRID(t), _l[i], 0, i, 1, 1);

        /* Adjustment */
        _a.push_back(Gtk::Adjustment::create(0.0, 0.0, _rangeLimit, 1.0, 10.0, 10.0));
        /* Slider */
        _s[i] = Gtk::manage(new Inkscape::UI::Widget::ColorSlider(_a[i]));
        _s[i]->show();

        _s[i]->set_margin_start(XPAD);
        _s[i]->set_margin_end(XPAD);
        _s[i]->set_margin_top(YPAD);
        _s[i]->set_margin_bottom(YPAD);
        _s[i]->set_hexpand(true);
        gtk_grid_attach(GTK_GRID(t), _s[i]->gobj(), 1, i, 1, 1);

        /* Spinbutton */
        auto spinbutton = Gtk::manage(new ScrollProtected<Gtk::SpinButton>(_a[i], 1.0));
        _b[i] = GTK_WIDGET(spinbutton->gobj());
        sp_dialog_defocus_on_enter(_b[i]);
        gtk_label_set_mnemonic_widget(GTK_LABEL(_l[i]), _b[i]);
        gtk_widget_show(_b[i]);

        gtk_widget_set_margin_start(_b[i], XPAD);
        gtk_widget_set_margin_end(_b[i], XPAD);
        gtk_widget_set_margin_top(_b[i], YPAD);
        gtk_widget_set_margin_bottom(_b[i], YPAD);
        gtk_widget_set_halign(_b[i], GTK_ALIGN_END);
        gtk_widget_set_valign(_b[i], GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(t), _b[i], 2, i, 1, 1);

        /* Signals */
	_a[i]->signal_value_changed().connect(sigc::bind(sigc::mem_fun(this, &ColorScales::adjustment_changed),i));
        _s[i]->signal_grabbed.connect(sigc::mem_fun(this, &ColorScales::_sliderAnyGrabbed));
        _s[i]->signal_released.connect(sigc::mem_fun(this, &ColorScales::_sliderAnyReleased));
        _s[i]->signal_value_changed.connect(sigc::mem_fun(this, &ColorScales::_sliderAnyChanged));
    }

    //Prevent 5th bar from being shown by PanelDialog::show_all_children
    gtk_widget_set_no_show_all(_l[4], TRUE);
    _s[4]->set_no_show_all(true);
    gtk_widget_set_no_show_all(_b[4], TRUE);

    /* Initial mode is none, so it works */
    setMode(mode);
}

void ColorScales::_recalcColor()
{
    SPColor color;
    gfloat alpha = 1.0;
    gfloat c[5];

    switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
        case SP_COLOR_SCALES_MODE_HSL:
        case SP_COLOR_SCALES_MODE_HSV:
            _getRgbaFloatv(c);
            color.set(c[0], c[1], c[2]);
            alpha = c[3];
            break;
        case SP_COLOR_SCALES_MODE_CMYK: {
            _getCmykaFloatv(c);

            float rgb[3];
            SPColor::cmyk_to_rgb_floatv(rgb, c[0], c[1], c[2], c[3]);
            color.set(rgb[0], rgb[1], rgb[2]);
            alpha = c[4];
            break;
        }
        default:
            g_warning("file %s: line %d: Illegal color selector mode %d", __FILE__, __LINE__, _mode);
            break;
    }

    _color.preserveICC();
    _color.setColorAlpha(color, alpha);
}

void ColorScales::_updateDisplay()
{
#ifdef DUMP_CHANGE_INFO
    g_message("ColorScales::_onColorChanged( this=%p, %f, %f, %f,   %f)", this, _color.color().v.c[0],
              _color.color().v.c[1], _color.color().v.c[2], _color.alpha());
#endif
    gfloat tmp[3];
    gfloat c[5] = { 0.0, 0.0, 0.0, 0.0 };

    SPColor color = _color.color();

    switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
            color.get_rgb_floatv(c);
            c[3] = _color.alpha();
            c[4] = 0.0;
            break;
        case SP_COLOR_SCALES_MODE_HSL:
            color.get_rgb_floatv(tmp);
            SPColor::rgb_to_hsl_floatv(c, tmp[0], tmp[1], tmp[2]);
            c[3] = _color.alpha();
            c[4] = 0.0;
            break;
        case SP_COLOR_SCALES_MODE_HSV:
            color.get_rgb_floatv(tmp);
            SPColor::rgb_to_hsv_floatv(c, tmp[0], tmp[1], tmp[2]);
            c[3] = _color.alpha();
            c[4] = 0.0;
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            color.get_cmyk_floatv(c);
            c[4] = _color.alpha();
            break;
        default:
            g_warning("file %s: line %d: Illegal color selector mode %d", __FILE__, __LINE__, _mode);
            break;
    }

    _updating = TRUE;
    setScaled(_a[0], c[0]);
    setScaled(_a[1], c[1]);
    setScaled(_a[2], c[2]);
    setScaled(_a[3], c[3]);
    setScaled(_a[4], c[4]);
    _updateSliders(CSC_CHANNELS_ALL);
    _updating = FALSE;
}

/* Helpers for setting color value */
gfloat ColorScales::getScaled(const Glib::RefPtr<Gtk::Adjustment> &a)
{
    gfloat val = a->get_value() / a->get_upper();
    return val;
}

void ColorScales::setScaled(Glib::RefPtr<Gtk::Adjustment> &a, gfloat v, bool constrained)
{
    auto upper = a->get_upper();
    gfloat val = v * upper;
    if (constrained) {
        // TODO: do we want preferences for these?
        if (upper == 255) {
            val = round(val/16) * 16;
        } else {
            val = round(val/10) * 10;
        }
    }
    a->set_value(val);
}

void ColorScales::_setRangeLimit(gdouble upper)
{
    _rangeLimit = upper;
    for (auto & i : _a) {
        i->set_upper(upper);
    }
}

void ColorScales::_onColorChanged()
{
    if (!get_visible()) {
        return;
    }
    _updateDisplay();
}

void ColorScales::on_show()
{
    Gtk::Grid::on_show();
    _updateDisplay();
}

void ColorScales::_getRgbaFloatv(gfloat *rgba)
{
    g_return_if_fail(rgba != nullptr);

    switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
            rgba[0] = getScaled(_a[0]);
            rgba[1] = getScaled(_a[1]);
            rgba[2] = getScaled(_a[2]);
            rgba[3] = getScaled(_a[3]);
            break;
        case SP_COLOR_SCALES_MODE_HSL:
            SPColor::hsl_to_rgb_floatv(rgba, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
            rgba[3] = getScaled(_a[3]);
            break;
        case SP_COLOR_SCALES_MODE_HSV:
            SPColor::hsv_to_rgb_floatv(rgba, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
            rgba[3] = getScaled(_a[3]);
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            SPColor::cmyk_to_rgb_floatv(rgba, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), getScaled(_a[3]));
            rgba[3] = getScaled(_a[4]);
            break;
        default:
            g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
            break;
    }
}

void ColorScales::_getCmykaFloatv(gfloat *cmyka)
{
    gfloat rgb[3];

    g_return_if_fail(cmyka != nullptr);

    switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
            SPColor::rgb_to_cmyk_floatv(cmyka, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
            cmyka[4] = getScaled(_a[3]);
            break;
        case SP_COLOR_SCALES_MODE_HSL:
            SPColor::hsl_to_rgb_floatv(rgb, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
            SPColor::rgb_to_cmyk_floatv(cmyka, rgb[0], rgb[1], rgb[2]);
            cmyka[4] = getScaled(_a[3]);
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            cmyka[0] = getScaled(_a[0]);
            cmyka[1] = getScaled(_a[1]);
            cmyka[2] = getScaled(_a[2]);
            cmyka[3] = getScaled(_a[3]);
            cmyka[4] = getScaled(_a[4]);
            break;
        default:
            g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
            break;
    }
}

guint32 ColorScales::_getRgba32()
{
    gfloat c[4];
    guint32 rgba;

    _getRgbaFloatv(c);

    rgba = SP_RGBA32_F_COMPOSE(c[0], c[1], c[2], c[3]);

    return rgba;
}

void ColorScales::setMode(SPColorScalesMode mode)
{
    gfloat rgba[4];
    gfloat c[4];

    if (_mode == mode)
        return;

    if ((_mode == SP_COLOR_SCALES_MODE_RGB) || (_mode == SP_COLOR_SCALES_MODE_HSL) ||
        (_mode == SP_COLOR_SCALES_MODE_CMYK) || (_mode == SP_COLOR_SCALES_MODE_HSV)) {
        _getRgbaFloatv(rgba);
    }
    else {
        rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1.0;
    }

    _mode = mode;

    switch (mode) {
        case SP_COLOR_SCALES_MODE_RGB:
            _setRangeLimit(255.0);
            _a[3]->set_upper(100.0);
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[0]), _("_R:"));
            _s[0]->set_tooltip_text(_("Red"));
            gtk_widget_set_tooltip_text(_b[0], _("Red"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[1]), _("_G:"));
            _s[1]->set_tooltip_text(_("Green"));
            gtk_widget_set_tooltip_text(_b[1], _("Green"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[2]), _("_B:"));
            _s[2]->set_tooltip_text(_("Blue"));
            gtk_widget_set_tooltip_text(_b[2], _("Blue"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[3]), _("_A:"));
            _s[3]->set_tooltip_text(_("Alpha (opacity)"));
            gtk_widget_set_tooltip_text(_b[3], _("Alpha (opacity)"));
            _s[0]->setMap(nullptr);
            gtk_widget_hide(_l[4]);
            _s[4]->hide();
            gtk_widget_hide(_b[4]);
            _updating = TRUE;
            setScaled(_a[0], rgba[0]);
            setScaled(_a[1], rgba[1]);
            setScaled(_a[2], rgba[2]);
            setScaled(_a[3], rgba[3]);
            _updateSliders(CSC_CHANNELS_ALL);
            _updating = FALSE;
            break;
        case SP_COLOR_SCALES_MODE_HSL:
            _setRangeLimit(100.0);

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[0]), _("_H:"));
            _s[0]->set_tooltip_text(_("Hue"));
            gtk_widget_set_tooltip_text(_b[0], _("Hue"));
            _a[0]->set_upper(360.0);

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[1]), _("_S:"));
            _s[1]->set_tooltip_text(_("Saturation"));
            gtk_widget_set_tooltip_text(_b[1], _("Saturation"));

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[2]), _("_L:"));
            _s[2]->set_tooltip_text(_("Lightness"));
            gtk_widget_set_tooltip_text(_b[2], _("Lightness"));

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[3]), _("_A:"));
            _s[3]->set_tooltip_text(_("Alpha (opacity)"));
            gtk_widget_set_tooltip_text(_b[3], _("Alpha (opacity)"));
            _s[0]->setMap((guchar *)(sp_color_scales_hue_map()));
            gtk_widget_hide(_l[4]);
            _s[4]->hide();
            gtk_widget_hide(_b[4]);
            _updating = TRUE;
            c[0] = 0.0;

            SPColor::rgb_to_hsl_floatv(c, rgba[0], rgba[1], rgba[2]);

            setScaled(_a[0], c[0]);
            setScaled(_a[1], c[1]);
            setScaled(_a[2], c[2]);
            setScaled(_a[3], rgba[3]);

            _updateSliders(CSC_CHANNELS_ALL);
            _updating = FALSE;
            break;
        case SP_COLOR_SCALES_MODE_HSV:
            _setRangeLimit(100.0);

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[0]), _("_H:"));
            _s[0]->set_tooltip_text(_("Hue"));
            gtk_widget_set_tooltip_text(_b[0], _("Hue"));
            _a[0]->set_upper(360.0);

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[1]), _("_S:"));
            _s[1]->set_tooltip_text(_("Saturation"));
            gtk_widget_set_tooltip_text(_b[1], _("Saturation"));

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[2]), _("_V:"));
            _s[2]->set_tooltip_text(_("Value"));
            gtk_widget_set_tooltip_text(_b[2], _("Value"));

            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[3]), _("_A:"));
            _s[3]->set_tooltip_text(_("Alpha (opacity)"));
            gtk_widget_set_tooltip_text(_b[3], _("Alpha (opacity)"));
            _s[0]->setMap((guchar *)(sp_color_scales_hue_map()));
            gtk_widget_hide(_l[4]);
            _s[4]->hide();
            gtk_widget_hide(_b[4]);
            _updating = TRUE;
            c[0] = 0.0;

            SPColor::rgb_to_hsv_floatv(c, rgba[0], rgba[1], rgba[2]);

            setScaled(_a[0], c[0]);
            setScaled(_a[1], c[1]);
            setScaled(_a[2], c[2]);
            setScaled(_a[3], rgba[3]);

            _updateSliders(CSC_CHANNELS_ALL);
            _updating = FALSE;
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            _setRangeLimit(100.0);
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[0]), _("_C:"));
            _s[0]->set_tooltip_text(_("Cyan"));
            gtk_widget_set_tooltip_text(_b[0], _("Cyan"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[1]), _("_M:"));
            _s[1]->set_tooltip_text(_("Magenta"));
            gtk_widget_set_tooltip_text(_b[1], _("Magenta"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[2]), _("_Y:"));
            _s[2]->set_tooltip_text(_("Yellow"));
            gtk_widget_set_tooltip_text(_b[2], _("Yellow"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[3]), _("_K:"));
            _s[3]->set_tooltip_text(_("Black"));
            gtk_widget_set_tooltip_text(_b[3], _("Black"));
            gtk_label_set_markup_with_mnemonic(GTK_LABEL(_l[4]), _("_A:"));
            _s[4]->set_tooltip_text(_("Alpha (opacity)"));
            gtk_widget_set_tooltip_text(_b[4], _("Alpha (opacity)"));
            _s[0]->setMap(nullptr);
            gtk_widget_show(_l[4]);
            _s[4]->show();
            gtk_widget_show(_b[4]);
            _updating = TRUE;

            SPColor::rgb_to_cmyk_floatv(c, rgba[0], rgba[1], rgba[2]);
            setScaled(_a[0], c[0]);
            setScaled(_a[1], c[1]);
            setScaled(_a[2], c[2]);
            setScaled(_a[3], c[3]);

            setScaled(_a[4], rgba[3]);
            _updateSliders(CSC_CHANNELS_ALL);
            _updating = FALSE;
            break;
        default:
            g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
            break;
    }
}

SPColorScalesMode ColorScales::getMode() const { return _mode; }

void ColorScales::_sliderAnyGrabbed()
{
    if (_updating) {
        return;
    }
    if (!_dragging) {
        _dragging = TRUE;
        _color.setHeld(true);
    }
}

void ColorScales::_sliderAnyReleased()
{
    if (_updating) {
        return;
    }
    if (_dragging) {
        _dragging = FALSE;
        _color.setHeld(false);
    }
}

void ColorScales::_sliderAnyChanged()
{
    if (_updating) {
        return;
    }
    _recalcColor();
}

void ColorScales::adjustment_changed(int channel)
{
    if (_updating) {
        return;
    }

    _updateSliders((1 << channel));
    _recalcColor();
}

void ColorScales::_updateSliders(guint channels)
{
    gfloat rgb0[3], rgbm[3], rgb1[3];
#ifdef SPCS_PREVIEW
    guint32 rgba;
#endif
    switch (_mode) {
        case SP_COLOR_SCALES_MODE_RGB:
            if ((channels != CSC_CHANNEL_R) && (channels != CSC_CHANNEL_A)) {
                /* Update red */
                _s[0]->setColors(SP_RGBA32_F_COMPOSE(0.0, getScaled(_a[1]), getScaled(_a[2]), 1.0),
                                 SP_RGBA32_F_COMPOSE(0.5, getScaled(_a[1]), getScaled(_a[2]), 1.0),
                                 SP_RGBA32_F_COMPOSE(1.0, getScaled(_a[1]), getScaled(_a[2]), 1.0));
            }
            if ((channels != CSC_CHANNEL_G) && (channels != CSC_CHANNEL_A)) {
                /* Update green */
                _s[1]->setColors(SP_RGBA32_F_COMPOSE(getScaled(_a[0]), 0.0, getScaled(_a[2]), 1.0),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), 0.5, getScaled(_a[2]), 1.0),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), 1.0, getScaled(_a[2]), 1.0));
            }
            if ((channels != CSC_CHANNEL_B) && (channels != CSC_CHANNEL_A)) {
                /* Update blue */
                _s[2]->setColors(SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), 0.0, 1.0),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), 0.5, 1.0),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), 1.0, 1.0));
            }
            if (channels != CSC_CHANNEL_A) {
                /* Update alpha */
                _s[3]->setColors(SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 0.0),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 0.5),
                                 SP_RGBA32_F_COMPOSE(getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 1.0));
            }
            break;
        case SP_COLOR_SCALES_MODE_HSL:
            /* Hue is never updated */
            if ((channels != CSC_CHANNEL_S) && (channels != CSC_CHANNEL_A)) {
                /* Update saturation */
                SPColor::hsl_to_rgb_floatv(rgb0, getScaled(_a[0]), 0.0, getScaled(_a[2]));
                SPColor::hsl_to_rgb_floatv(rgbm, getScaled(_a[0]), 0.5, getScaled(_a[2]));
                SPColor::hsl_to_rgb_floatv(rgb1, getScaled(_a[0]), 1.0, getScaled(_a[2]));
                _s[1]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if ((channels != CSC_CHANNEL_V) && (channels != CSC_CHANNEL_A)) {
                /* Update value */
                SPColor::hsl_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), 0.0);
                SPColor::hsl_to_rgb_floatv(rgbm, getScaled(_a[0]), getScaled(_a[1]), 0.5);
                SPColor::hsl_to_rgb_floatv(rgb1, getScaled(_a[0]), getScaled(_a[1]), 1.0);
                _s[2]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if (channels != CSC_CHANNEL_A) {
                /* Update alpha */
                SPColor::hsl_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
                _s[3]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.0),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.5),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0));
            }
            break;
        case SP_COLOR_SCALES_MODE_HSV:
            /* Hue is never updated */
            if ((channels != CSC_CHANNEL_S) && (channels != CSC_CHANNEL_A)) {
                /* Update saturation */
                SPColor::hsv_to_rgb_floatv(rgb0, getScaled(_a[0]), 0.0, getScaled(_a[2]));
                SPColor::hsv_to_rgb_floatv(rgbm, getScaled(_a[0]), 0.5, getScaled(_a[2]));
                SPColor::hsv_to_rgb_floatv(rgb1, getScaled(_a[0]), 1.0, getScaled(_a[2]));
                _s[1]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if ((channels != CSC_CHANNEL_V) && (channels != CSC_CHANNEL_A)) {
                /* Update value */
                SPColor::hsv_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), 0.0);
                SPColor::hsv_to_rgb_floatv(rgbm, getScaled(_a[0]), getScaled(_a[1]), 0.5);
                SPColor::hsv_to_rgb_floatv(rgb1, getScaled(_a[0]), getScaled(_a[1]), 1.0);
                _s[2]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if (channels != CSC_CHANNEL_A) {
                /* Update alpha */
                SPColor::hsv_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]));
                _s[3]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.0),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.5),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0));
            }
            break;
        case SP_COLOR_SCALES_MODE_CMYK:
            if ((channels != CSC_CHANNEL_C) && (channels != CSC_CHANNEL_CMYKA)) {
                /* Update C */
                SPColor::cmyk_to_rgb_floatv(rgb0, 0.0, getScaled(_a[1]), getScaled(_a[2]), getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgbm, 0.5, getScaled(_a[1]), getScaled(_a[2]), getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgb1, 1.0, getScaled(_a[1]), getScaled(_a[2]), getScaled(_a[3]));
                _s[0]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if ((channels != CSC_CHANNEL_M) && (channels != CSC_CHANNEL_CMYKA)) {
                /* Update M */
                SPColor::cmyk_to_rgb_floatv(rgb0, getScaled(_a[0]), 0.0, getScaled(_a[2]), getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgbm, getScaled(_a[0]), 0.5, getScaled(_a[2]), getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgb1, getScaled(_a[0]), 1.0, getScaled(_a[2]), getScaled(_a[3]));
                _s[1]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if ((channels != CSC_CHANNEL_Y) && (channels != CSC_CHANNEL_CMYKA)) {
                /* Update Y */
                SPColor::cmyk_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), 0.0, getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgbm, getScaled(_a[0]), getScaled(_a[1]), 0.5, getScaled(_a[3]));
                SPColor::cmyk_to_rgb_floatv(rgb1, getScaled(_a[0]), getScaled(_a[1]), 1.0, getScaled(_a[3]));
                _s[2]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if ((channels != CSC_CHANNEL_K) && (channels != CSC_CHANNEL_CMYKA)) {
                /* Update K */
                SPColor::cmyk_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 0.0);
                SPColor::cmyk_to_rgb_floatv(rgbm, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 0.5);
                SPColor::cmyk_to_rgb_floatv(rgb1, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]), 1.0);
                _s[3]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgbm[0], rgbm[1], rgbm[2], 1.0),
                                 SP_RGBA32_F_COMPOSE(rgb1[0], rgb1[1], rgb1[2], 1.0));
            }
            if (channels != CSC_CHANNEL_CMYKA) {
                /* Update alpha */
                SPColor::cmyk_to_rgb_floatv(rgb0, getScaled(_a[0]), getScaled(_a[1]), getScaled(_a[2]),
                                            getScaled(_a[3]));
                _s[4]->setColors(SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.0),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 0.5),
                                 SP_RGBA32_F_COMPOSE(rgb0[0], rgb0[1], rgb0[2], 1.0));
            }
            break;
        default:
            g_warning("file %s: line %d: Illegal color selector mode", __FILE__, __LINE__);
            break;
    }

#ifdef SPCS_PREVIEW
    rgba = sp_color_scales_get_rgba32(cs);
    sp_color_preview_set_rgba32(SP_COLOR_PREVIEW(_p), rgba);
#endif
}

static const gchar *sp_color_scales_hue_map()
{
    static gchar *map = nullptr;

    if (!map) {
        gchar *p;
        gint h;
        map = g_new(gchar, 4 * 1024);
        p = map;
        for (h = 0; h < 1024; h++) {
            gfloat rgb[3];
            SPColor::hsl_to_rgb_floatv(rgb, h / 1024.0, 1.0, 0.5);
            *p++ = SP_COLOR_F_TO_U(rgb[0]);
            *p++ = SP_COLOR_F_TO_U(rgb[1]);
            *p++ = SP_COLOR_F_TO_U(rgb[2]);
            *p++ = 0xFF;
        }
    }

    return map;
}

ColorScalesFactory::ColorScalesFactory(SPColorScalesMode submode)
    : _submode(submode)
{
}

ColorScalesFactory::~ColorScalesFactory() = default;

Gtk::Widget *ColorScalesFactory::createWidget(Inkscape::UI::SelectedColor &color) const
{
    Gtk::Widget *w = Gtk::manage(new ColorScales(color, _submode));
    return w;
}

Glib::ustring ColorScalesFactory::modeName() const {
    return gettext(ColorScales::SUBMODE_NAMES[_submode]);
}

}
}
}
