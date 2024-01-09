// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_PENCIL_TOOLBAR_H
#define SEEN_PENCIL_TOOLBAR_H

/**
 * @file
 * Pencil and pen toolbars
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2011 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "toolbar.h"

#include <gtkmm/adjustment.h>
#include <vector>

class SPDesktop;

namespace Gtk {
class RadioToolButton;
class ToggleToolButton;
class ToolButton;
}

namespace Inkscape {
namespace XML {
class Node;
}

namespace UI {
namespace Widget {
class SpinButtonToolItem;
class ComboToolItem;
}

namespace Toolbar {
class PencilToolbar : public Toolbar {
private:
    bool const _tool_is_pencil;
    std::vector<Gtk::RadioToolButton *> _mode_buttons;

    Gtk::ToggleToolButton *_pressure_item = nullptr;
    UI::Widget::SpinButtonToolItem *_minpressure = nullptr;
    UI::Widget::SpinButtonToolItem *_maxpressure = nullptr;
    UI::Widget::SpinButtonToolItem *_shapescale = nullptr;

    XML::Node *_repr = nullptr;
    Gtk::ToolButton *_flatten_spiro_bspline = nullptr;
    Gtk::ToolButton *_flatten_simplify = nullptr;

    UI::Widget::ComboToolItem *_shape_item = nullptr;
    UI::Widget::ComboToolItem *_cap_item = nullptr;

    Gtk::ToggleToolButton *_simplify = nullptr;

    bool _freeze = false;

    Glib::RefPtr<Gtk::Adjustment> _minpressure_adj;
    Glib::RefPtr<Gtk::Adjustment> _maxpressure_adj;
    Glib::RefPtr<Gtk::Adjustment> _tolerance_adj;
    Glib::RefPtr<Gtk::Adjustment> _shapescale_adj;

    void add_freehand_mode_toggle();
    void mode_changed(int mode);
    Glib::ustring const freehand_tool_name();
    void minpressure_value_changed();
    void maxpressure_value_changed();
    void shapewidth_value_changed();
    void use_pencil_pressure();
    void tolerance_value_changed();
    void add_advanced_shape_options();
    void add_powerstroke_cap();
    void change_shape(int shape);
    void update_width_value(int shape);
    void change_cap(int cap);
    void simplify_lpe();
    void simplify_flatten();
    void flatten_spiro_bspline();

protected:
  PencilToolbar(SPDesktop *desktop, bool pencil_mode);
  ~PencilToolbar() override;

public:
    static GtkWidget * create_pencil(SPDesktop *desktop);
    static GtkWidget * create_pen(SPDesktop *desktop);
};
}
}
}

#endif /* !SEEN_PENCIL_TOOLBAR_H */
