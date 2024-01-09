// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Static style swatch (fill, stroke, opacity)
 */
/* Authors:
 *   buliabyak@gmail.com
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2005-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_CURRENT_STYLE_H
#define INKSCAPE_UI_CURRENT_STYLE_H

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/enums.h>

#include "desktop.h"
#include "preferences.h"

constexpr int STYLE_SWATCH_WIDTH = 135;

class SPStyle;
class SPCSSAttr;

namespace Gtk {
class Grid;
}

namespace Inkscape {

namespace Util {
    class Unit;
}

namespace UI {
namespace Widget {

class StyleSwatch : public Gtk::Box
{
public:
    StyleSwatch (SPCSSAttr *attr, gchar const *main_tip, Gtk::Orientation orient = Gtk::ORIENTATION_VERTICAL);

    ~StyleSwatch() override;

    void setStyle(SPStyle *style);
    void setStyle(SPCSSAttr *attr);
    SPCSSAttr *getStyle();

    void setWatchedTool (const char *path, bool synthesize);
    void setToolName(const Glib::ustring& tool_name);
    void setDesktop(SPDesktop *desktop);
    bool on_click(GdkEventButton *event);

private:
    class ToolObserver;
    class StyleObserver;

    SPDesktop *_desktop;
    Glib::ustring _tool_name;
    SPCSSAttr *_css;
    ToolObserver *_tool_obs;
    StyleObserver *_style_obs;
    Glib::ustring _tool_path;

    Gtk::EventBox _swatch;

    Gtk::Grid *_table;

    Gtk::Label _label[2];
    Gtk::Box _empty_space;
    Gtk::EventBox _place[2];
    Gtk::EventBox _opacity_place;
    Gtk::Label _value[2];
    Gtk::Label _opacity_value;
    Gtk::Widget *_color_preview[2];
    Glib::ustring __color[2];
    Gtk::Box _stroke;
    Gtk::EventBox _stroke_width_place;
    Gtk::Label _stroke_width;

    Inkscape::Util::Unit *_sw_unit;

friend class ToolObserver;
};


} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_BUTTON_H

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
