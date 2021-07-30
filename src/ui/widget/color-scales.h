// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_COLOR_SCALES_H
#define SEEN_SP_COLOR_SCALES_H

#include <gtkmm/grid.h>

#include "ui/selected-color.h"

namespace Inkscape {
namespace UI {
namespace Widget {

class ColorSlider;

enum SPColorScalesMode {
    SP_COLOR_SCALES_MODE_NONE = 0,
    SP_COLOR_SCALES_MODE_RGB = 1,
    SP_COLOR_SCALES_MODE_HSL = 2,
    SP_COLOR_SCALES_MODE_CMYK = 3,
    SP_COLOR_SCALES_MODE_HSV = 4
};

class ColorScales
    : public Gtk::Grid
{
public:
    static const gchar *SUBMODE_NAMES[];

    static gfloat getScaled(const Glib::RefPtr<Gtk::Adjustment> &a);
    static void setScaled(Glib::RefPtr<Gtk::Adjustment> &a, gfloat v, bool constrained = false);

    ColorScales(SelectedColor &color, SPColorScalesMode mode);
    ~ColorScales() override;

    virtual void _initUI(SPColorScalesMode mode);

    void setMode(SPColorScalesMode mode);
    SPColorScalesMode getMode() const;

protected:
    void _onColorChanged();
    void on_show() override;

    void _sliderAnyGrabbed();
    void _sliderAnyReleased();
    void _sliderAnyChanged();
    void adjustment_changed(int channel);

    void _getRgbaFloatv(gfloat *rgba);
    void _getCmykaFloatv(gfloat *cmyka);
    guint32 _getRgba32();
    void _updateSliders(guint channels);
    void _recalcColor();
    void _updateDisplay();

    void _setRangeLimit(gdouble upper);

    SelectedColor &_color;
    SPColorScalesMode _mode;
    gdouble _rangeLimit;
    gboolean _updating : 1;
    gboolean _dragging : 1;
    std::vector<Glib::RefPtr<Gtk::Adjustment>> _a;        /* Channel adjustments */
    Inkscape::UI::Widget::ColorSlider *_s[5]; /* Channel sliders */
    GtkWidget *_b[5];                         /* Spinbuttons */
    GtkWidget *_l[5];                         /* Labels */

private:
    // By default, disallow copy constructor and assignment operator
    ColorScales(ColorScales const &obj) = delete;
    ColorScales &operator=(ColorScales const &obj) = delete;
};

class ColorScalesFactory : public Inkscape::UI::ColorSelectorFactory
{
public:
    ColorScalesFactory(SPColorScalesMode submode);
    ~ColorScalesFactory() override;

    Gtk::Widget *createWidget(Inkscape::UI::SelectedColor &color) const override;
    Glib::ustring modeName() const override;

private:
    SPColorScalesMode _submode;
};

}
}
}

#endif /* !SEEN_SP_COLOR_SCALES_H */
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
