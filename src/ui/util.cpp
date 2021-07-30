// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Utility functions for UI
 *
 * Authors:
 *   Tavmjong Bah
 *   John Smith
 *
 * Copyright (C) 2004, 2013, 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/window.h>
#include "util.h"

/*
 * Ellipse text if longer than maxlen, "50% start text + ... + ~50% end text"
 * Text should be > length 8 or just return the original text
 */
Glib::ustring ink_ellipsize_text(Glib::ustring const &src, size_t maxlen)
{
    if (src.length() > maxlen && maxlen > 8) {
        size_t p1 = (size_t) maxlen / 2;
        size_t p2 = (size_t) src.length() - (maxlen - p1 - 1);
        return src.substr(0, p1) + "…" + src.substr(p2);
    }
    return src;
}

/**
 * Show widget, if the widget has a Gtk::Reveal parent, reveal instead.
 *
 * @param widget - The child widget to show.
 */
void reveal_widget(Gtk::Widget *widget, bool show)
{
    auto revealer = dynamic_cast<Gtk::Revealer *>(widget->get_parent());
    if (revealer) {
        revealer->set_reveal_child(show);
    }
    if (show) {
        widget->show();
    } else if (!revealer) {
        widget->hide();
    }
}


bool is_widget_effectively_visible(Gtk::Widget* widget) {
    if (!widget) return false;

    // TODO: what's the right way to determine if widget is visible on the screen?
    return widget->get_child_visible();
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
