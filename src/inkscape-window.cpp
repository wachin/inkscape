// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Inkscape - An SVG editor.
 */
/*
 * Authors:
 *   Tavmjong Bah
 *
 * Copyright (C) 2018 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 *
 */


#include "inkscape-window.h"
#include "inkscape.h"   // SP_ACTIVE_DESKTOP
#include "desktop-events.h" // Handle key events
#include "enums.h"      // PREFS_WINDOW_GEOMETRY_NONE

#include "inkscape-application.h"

#include "actions/actions-canvas-mode.h"
#include "actions/actions-canvas-snapping.h"
#include "actions/actions-canvas-transform.h"
#include "actions/actions-dialogs.h"
#include "actions/actions-edit-window.h"
#include "actions/actions-file-window.h"
#include "actions/actions-help-url.h"
#include "actions/actions-layer.h"
#include "actions/actions-node-align.h" // Node alignment.
#include "actions/actions-paths.h"  // TEMP
#include "actions/actions-selection-window.h"
#include "actions/actions-tools.h"
#include "actions/actions-view-mode.h"
#include "actions/actions-view-window.h"
#include "actions/actions-pages.h"

#include "object/sp-namedview.h"  // TODO Remove need for this!

#include "ui/dialog/dialog-container.h"
#include "ui/dialog/dialog-manager.h"
#include "ui/dialog/dialog-window.h"
#include "ui/drag-and-drop.h"  // Move to canvas?
#include "ui/interface.h" // main menu, sp_ui_close_view()

#include "ui/monitor.h" // get_monitor_geometry_at_point()

#include "ui/desktop/menubar.h"
#include "ui/desktop/menu-icon-shift.h"

#include "ui/drag-and-drop.h"

#include "ui/event-debug.h"
#include "ui/shortcuts.h"

#include "widgets/desktop-widget.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"

using Inkscape::UI::Dialog::DialogManager;
using Inkscape::UI::Dialog::DialogContainer;
using Inkscape::UI::Dialog::DialogWindow;

static gboolean _resize_children(Gtk::Window *win)
{
    Inkscape::UI::resize_widget_children(win);
    return false;
}


InkscapeWindow::InkscapeWindow(SPDocument* document)
    : _document(document)
{
    if (!_document) {
        std::cerr << "InkscapeWindow::InkscapeWindow: null document!" << std::endl;
        return;
    }

    _app = InkscapeApplication::instance();
    _app->gtk_app()->add_window(*this);

    set_resizable(true);

    // =============== Build interface ===============

    // Main box
    _mainbox = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    _mainbox->set_name("DesktopMainBox");
    _mainbox->show();
    add(*_mainbox);

    // Desktop widget (=> MultiPaned)
    _desktop_widget = new SPDesktopWidget(this, _document);
    _desktop_widget->window = this;
    _desktop_widget->show();
    _desktop = _desktop_widget->desktop;

    // =================== Actions ===================

    // After canvas has been constructed.. move to canvas proper.
    add_actions_canvas_mode(this);          // Actions to change canvas display mode.
    add_actions_canvas_snapping(this);      // Actions to toggle on/off snapping modes.
    add_actions_canvas_transform(this);     // Actions to transform canvas view.
    add_actions_dialogs(this);              // Actions to open dialogs.
    add_actions_edit_window(this);          // Actions to edit.
    add_actions_file_window(this);          // Actions for file actions which are desktop dependent.
    add_actions_help_url(this);             // Actions to help url.
    add_actions_layer(this);                // Actions for layer.
    add_actions_node_align(this);           // Actions to align and distribute nodes (requiring Node tool).
    add_actions_path(this);                 // Actions for paths. TEMP
    add_actions_select_window(this);        // Actions with desktop selection
    add_actions_tools(this);                // Actions to switch between tools.
    add_actions_view_mode(this);            // Actions to change how Inkscape canvas is displayed.
    add_actions_view_window(this);          // Actions to add/change window of Inkscape
    add_actions_page_tools(this);           // Actions specific to pages tool and toolbar

    // Add document action group to window and export to DBus.
    add_document_actions();

    auto connection = _app->gio_app()->get_dbus_connection();
    if (connection) {
        std::string document_action_group_name = _app->gio_app()->get_dbus_object_path() + "/document/" + std::to_string(get_id());
        connection->export_action_group(document_action_group_name, document->getActionGroup());
    }

    // This is called here (rather than in InkscapeApplication) solely to add win level action
    // tooltips to the menu label-to-tooltip map.
    build_menu();

    // ========== Drag and Drop of Documents =========
    ink_drag_setup(_desktop_widget);

    // Pallet

    // Status bar

    // The main section
    _mainbox->pack_start(*Gtk::manage(_desktop_widget), true, true);

    // ================== Callbacks ==================
    signal_window_state_event().connect(sigc::mem_fun(*_desktop, &SPDesktop::onWindowStateEvent));
    signal_focus_in_event().connect(    sigc::mem_fun(*_desktop_widget, &SPDesktopWidget::onFocusInEvent));


    // ================ Window Options ===============
    setup_view();

    // Show dialogs after the main window, otherwise dialogs may be associated as the main window of the program.
    // Restore short-lived floating dialogs state if this is the first window being opened
    bool include_short_lived = _app->get_number_of_windows() == 0;
    DialogManager::singleton().restore_dialogs_state(_desktop->getContainer(), include_short_lived);

    // This pokes the window to request the right size for the dialogs once loaded.
    g_idle_add(GSourceFunc(&_resize_children), this);

    // ================= Shift Icons =================
    // Note: The menu is defined at the app level but shifting icons requires actual widgets and
    // must be done on the window level.
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getInt("/theme/shiftIcons", true)) {
        bool shifted = false;
        for (auto child : get_children()) {
            auto menubar = dynamic_cast<Gtk::MenuBar *>(child);
            if (menubar) {
                menubar->get_style_context()->add_class("shifticonmenu");
                if (!shifted) {
                    shifted = shift_icons(menubar);
                }
            }
        }
    }

    // ========= Update text for Accellerators =======
    Inkscape::Shortcuts::getInstance().update_gui_text_recursive(this);
}

InkscapeWindow::~InkscapeWindow()
{
    g_idle_remove_by_data(this);
}

// Change a document, leaving desktop/view the same. (Eventually move all code here.)
void
InkscapeWindow::change_document(SPDocument* document)
{
    if (!_app) {
        std::cerr << "Inkscapewindow::change_document: app is nullptr!" << std::endl;
        return;
    }

    _document = document;
    _app->set_active_document(_document);
    add_document_actions();

    setup_view();
    update_dialogs();
}

// Sets up the window and view according to user preferences and <namedview> of the just loaded document
void
InkscapeWindow::setup_view()
{
    // Make sure the GdkWindow is fully initialized before resizing/moving
    // (ensures the monitor it'll be shown on is known)
    realize();

    // Resize the window to match the document properties
    sp_namedview_window_from_document(_desktop); // This should probably be a member function here.

    // Must show before setting zoom and view! (crashes otherwise)
    //
    // Showing after resizing/moving allows the window manager to correct an invalid size/position of the window
    // TODO: This does *not* work when called from 'change_document()', i.e. when the window is already visible.
    //       This can result in off-screen windows! We previously worked around this by hiding and re-showing
    //       the window, but a call to hide() causes Inkscape to just exit since the migration to Gtk::Application
    show();
    
    _desktop->schedule_zoom_from_document();
    sp_namedview_update_layers_from_document(_desktop);

    SPNamedView *nv = _desktop->namedview;
    if (nv && nv->lockguides) {
        nv->setLockGuides(true);
    }
}

bool
InkscapeWindow::on_key_press_event(GdkEventKey* event)
{
#ifdef EVENT_DEBUG
    ui_dump_event(reinterpret_cast<GdkEvent *>(event), "\nInkscapeWindow::on_key_press_event");
#endif

    // Key press and release events are normally sent first to Gtk::Window for processing as
    // accelerators and menomics before bubbling up from the "grab" or "focus" widget (unlike other
    // events which always bubble up). This would means that key combinations used for accelerators
    // won't reach the focus widget (and our tool event handlers). As we use single keys for
    // accelerators, we wouldn't even be able to type text! We can get around this by sending key
    // events first to the focus widget.
    //
    // See https://developer.gnome.org/gtk3/stable/chap-input-handling.html (Event Propagation)

    auto focus = get_focus();
    if (focus) {
        if (focus->event(reinterpret_cast<GdkEvent *>(event))) {
            return true;
        }
    }

    // Try to find action to call; calling it here makes it higher priority than dialog mnemonics;
    // this is needed because GTK tries to activate widgets with matching mnemonics first,
    // even if they are invisible (!) and/or disabled. That cripples some Alt+key shortcuts when
    // we open and dock some dialogs, whether they are visible or not.
    // On macOS situation is even worse, as dialogs can steal many common <option>+key shortcuts.
    if (Inkscape::Shortcuts::getInstance().invoke_action(event)) {
        return true;
    }

    if (Gtk::Window::on_key_press_event(event)) {
        return true;
    }

    // Not handled
    return false;
}

/**
 * If "dialogs on top" is activated in the preferences, set `parent` as the
 * new transient parent for all DialogWindow windows of the application.
 */
static void retransientize_dialogs(Gtk::Window &parent)
{
    assert(!dynamic_cast<DialogWindow *>(&parent));

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool window_above =
        prefs->getInt("/options/transientpolicy/value", PREFS_DIALOGS_WINDOWS_NORMAL) != PREFS_DIALOGS_WINDOWS_NONE;

    for (auto const &window : parent.get_application()->get_windows()) {
        if (auto dialog_window = dynamic_cast<DialogWindow *>(window)) {
            if (window_above) {
                dialog_window->set_transient_for(parent);
            } else {
                dialog_window->unset_transient_for();
            }
        }
    }
}

bool
InkscapeWindow::on_focus_in_event(GdkEventFocus* event)
{
    if (_app) {
        _app->set_active_window(this);
        _app->set_active_document(_document);
        _app->set_active_view(_desktop);
        _app->set_active_selection(_desktop->getSelection());
        _app->windows_update(_document);
        update_dialogs();
        retransientize_dialogs(*this);
    } else {
        std::cerr << "Inkscapewindow::on_focus_in_event: app is nullptr!" << std::endl;
    }

    return Gtk::ApplicationWindow::on_focus_in_event(event);
}

// Called when a window is closed via the 'X' in the window bar.
bool
InkscapeWindow::on_delete_event(GdkEventAny* event)
{
    if (_app) {
        _app->destroy_window(this);
    }
    return true;
};

/**
 * Configure is called when the widget's size, position or stack changes.
 */
bool InkscapeWindow::on_configure_event(GdkEventConfigure *event)
{
    bool ret = Gtk::ApplicationWindow::on_configure_event(event);
    // Store the desktop widget size on resize.
    if (!_desktop || !get_realized())
        return ret;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool maxed = _desktop->is_maximized();
    bool full = _desktop->is_fullscreen();
    prefs->setBool("/desktop/geometry/fullscreen", full);
    prefs->setBool("/desktop/geometry/maximized", maxed);

    // Don't save geom for maximized, fullscreen or iconified windows.
    // It just tells you the current maximized size, which is not
    // as useful as whatever value it had previously.
    if (!_desktop->is_iconified() && !maxed && !full) {

        // Get size is more accurate than frame extends for window size.
        int w,h = 0;
        get_size(w, h);
        prefs->setInt("/desktop/geometry/width", w);
        prefs->setInt("/desktop/geometry/height", h);

        // Frame extends returns real positions, unlike get_position()
        if (Glib::RefPtr<Gdk::Window> gdkw = get_window()) {
            Gdk::Rectangle rect;
            gdkw->get_frame_extents(rect);
            prefs->setInt("/desktop/geometry/x", rect.get_x());
            prefs->setInt("/desktop/geometry/y", rect.get_y());
        }
    }
    return ret;
}

void InkscapeWindow::update_dialogs()
{
    std::vector<Gtk::Window *> windows = _app->gtk_app()->get_windows();
    for (auto const &window : windows) {
        DialogWindow *dialog_window = dynamic_cast<DialogWindow *>(window);
        if (dialog_window) {
            // Update the floating dialogs, reset them to the new desktop.
            dialog_window->set_inkscape_window(this);
        }
    }

    // Update the docked dialogs in this InkscapeWindow
    _desktop->updateDialogs();
}

/**
 * Make document actions accessible from the window
 */
void InkscapeWindow::add_document_actions()
{
    auto doc_action_group = _document->getActionGroup();

    insert_action_group("doc", doc_action_group);

#ifdef __APPLE__
    // Workaround for https://gitlab.gnome.org/GNOME/gtk/-/issues/5667
    // Copy the document ("doc") actions to the window ("win") so that the
    // application menu on macOS can handle them. The menu only handles the
    // window actions (in gtk_application_impl_quartz_active_window_changed),
    // not the ones attached with "insert_action_group".
    for (auto const &action_name : doc_action_group->list_actions()) {
        add_action(doc_action_group->lookup_action(action_name));
    }
#endif
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
