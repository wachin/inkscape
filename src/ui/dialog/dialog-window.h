// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INKSCAPE_UI_DIALOG_WINDOW_H
#define INKSCAPE_UI_DIALOG_WINDOW_H

/** @file
 * @brief A window for floating docks.
 *
 * Authors: see git history
 *   Tavmjong Bah
 *
 * Copyright (c) 2018 Tavmjong Bah, Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/applicationwindow.h>

#include "inkscape-application.h"

using Gtk::Label;
using Gtk::Widget;

class InkscapeWindow;

namespace Inkscape {
namespace UI {
namespace Dialog {

class DialogContainer;
class DialogMultipaned;

/**
 * DialogWindow holds DialogContainer instances for undocked dialogs.
 *
 * It watches the last active InkscapeWindow and updates its inner dialogs, if any.
 */
class DialogWindow : public Gtk::Window
{
public:
    DialogWindow(InkscapeWindow* window, Gtk::Widget *page = nullptr);
    ~DialogWindow() override;

    void set_inkscape_window(InkscapeWindow *window);
    InkscapeWindow* get_inkscape_window() { return _inkscape_window; }
    void update_dialogs();
    void update_window_size_to_fit_children();

    // Getters
    DialogContainer *get_container() { return _container; }

private:
    bool on_key_press_event(GdkEventKey* key_event) override;

    InkscapeApplication *_app = nullptr;
    InkscapeWindow *_inkscape_window = nullptr; // The Inkscape window that dialog window is attached to, changes when mouse moves into new Inkscape window.
    DialogContainer *_container = nullptr;
    Glib::ustring _title;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_WINDOW_H

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
