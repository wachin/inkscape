// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Utility functions for UI
 *
 * Authors:
 *   Tavmjong Bah
 *   John Smith
 *
 * Copyright (C) 2013, 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef UI_UTIL_SEEN
#define UI_UTIL_SEEN

#include <glibmm/ustring.h>
#include <gtkmm.h>

Glib::ustring ink_ellipsize_text (Glib::ustring const &src, size_t maxlen);
void reveal_widget(Gtk::Widget *widget, bool show);

// check if widget in a container is actually visible
bool is_widget_effectively_visible(Gtk::Widget* widget);

#endif

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
