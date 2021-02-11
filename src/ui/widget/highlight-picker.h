// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef __UI_DIALOG_HIGHLIGHT_PICKER_H__
#define __UI_DIALOG_HIGHLIGHT_PICKER_H__
/*
 * Authors:
 *   Theodore Janeczko
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/widget.h>
#include <glibmm/property.h>

namespace Inkscape {
namespace UI {
namespace Widget {

class HighlightPicker : public Gtk::CellRendererPixbuf {
public:
    HighlightPicker();
    ~HighlightPicker() override;

    Glib::PropertyProxy<guint32> property_active() { return _property_active.get_proxy(); }

protected:
    void render_vfunc( const Cairo::RefPtr<Cairo::Context>& cr,
                               Gtk::Widget& widget,
                               const Gdk::Rectangle& background_area,
                               const Gdk::Rectangle& cell_area,
                               Gtk::CellRendererState flags ) override;

    void get_preferred_width_vfunc(Gtk::Widget& widget,
                                           int& min_w,
                                           int& nat_w) const override;
    
    void get_preferred_height_vfunc(Gtk::Widget& widget,
                                            int& min_h,
                                            int& nat_h) const override;

    bool activate_vfunc(GdkEvent *event,
                                Gtk::Widget &widget,
                                const Glib::ustring &path,
                                const Gdk::Rectangle &background_area,
                                const Gdk::Rectangle &cell_area,
                                Gtk::CellRendererState flags) override;

private:
    
    Glib::Property<guint32> _property_active;
};



} // namespace Widget
} // namespace UI
} // namespace Inkscape


#endif /* __UI_DIALOG_IMAGETOGGLER_H__ */

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
