// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Ruler widget. Indicates horizontal or vertical position of a cursor in a specified widget.
 *
 * Copyright (C) 2019 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_RULER_H
#define INK_RULER_H

/* Rewrite of the C Ruler. */

#include "preferences.h"
#include <gtkmm.h>
#include <unordered_map>

namespace Inkscape {
namespace Util {
class Unit;
}
}

namespace Inkscape {
namespace UI {
namespace Widget {
  
class Ruler : public Gtk::DrawingArea
{
public:
    Ruler(Gtk::Orientation orientation);

    void set_unit(Inkscape::Util::Unit const *unit);
    void set_range(double lower, double upper);
    void set_page(double lower, double upper);
    void set_selection(double lower, double upper);

    void add_track_widget(Gtk::Widget& widget);

    void size_request(Gtk::Requisition& requisition) const;
    void get_preferred_width_vfunc( int& minimum_width,  int& natural_width ) const override;
    void get_preferred_height_vfunc(int& minimum_height, int& natural_height) const override;

protected:
    bool draw_scale(const Cairo::RefPtr<::Cairo::Context>& cr);
    void draw_marker(const Cairo::RefPtr<::Cairo::Context>& cr);
    Cairo::RectangleInt marker_rect();
    bool on_draw(const::Cairo::RefPtr<::Cairo::Context>& cr) override;
    void on_style_updated() override;
    void on_prefs_changed();

    bool on_motion_notify_event(GdkEventMotion *motion_event) override;
    bool on_button_press_event(GdkEventButton *button_event) override;

private:
    Inkscape::PrefObserver _watch_prefs;

    Gtk::Menu *getContextMenu();
    Cairo::RefPtr<Cairo::Surface> draw_label(Cairo::RefPtr<Cairo::Surface> const &surface_in, int label_value);

    Gtk::Orientation    _orientation;

    Inkscape::Util::Unit const* _unit;
    double _lower;
    double _upper;
    double _position;
    double _max_size;

    // Page block
    double _page_lower = 0.0;
    double _page_upper = 0.0;

    // Selection block
    double _sel_lower = 0.0;
    double _sel_upper = 0.0;
    double _sel_visible = true;

    bool   _backing_store_valid;

    Cairo::RefPtr<::Cairo::Surface> _backing_store;
    Cairo::RectangleInt _rect;

    std::unordered_map<int, Cairo::RefPtr<::Cairo::Surface>> _label_cache;

    // Cached style properties
    Gtk::Border _border;
    Gdk::RGBA _shadow;
    Gdk::RGBA _foreground;
    Pango::FontDescription _font;
    int _font_size;
    Gdk::RGBA _page_fill;
    Gdk::RGBA _select_fill;
    Gdk::RGBA _select_stroke;
};

} // Namespace Inkscape
}
}
#endif // INK_RULER_H

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
