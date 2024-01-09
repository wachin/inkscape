// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shift Gtk::MenuItems with icons to align with Toggle and Radio buttons.
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *
 * Copyright (C) 2020 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 *
 */

#include "menu-icon-shift.h"

#include <iostream>
#include <gtkmm.h>

#include "inkscape-application.h"  // Action extra data

// Could be used to update status bar.
// bool on_enter_notify(GdkEventCrossing* crossing_event, Gtk::MenuItem* menuitem)
// {
//     return false;
// }

/*
 *  Install CSS to shift icons into the space reserved for toggles (i.e. check and radio items).
 *  The CSS will apply to all menu icons but is updated as each menu is shown.
 */

bool
shift_icons(Gtk::MenuShell* menu)
{
    gint width, height;
    gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, &height);
    bool shifted = false;    
    // Calculate required shift. We need an example!
    // Search for Gtk::MenuItem -> Gtk::Box -> Gtk::Image
    static auto app = InkscapeApplication::instance();
    auto &label_to_tooltip_map = app->get_menu_label_to_tooltip_map();

    for (auto child : menu->get_children()) {
        auto menuitem = dynamic_cast<Gtk::MenuItem *>(child);
        if (menuitem) { //we need to go here to know we are in RTL maybe we can check in otehr way and simplify
            auto submenu = menuitem->get_submenu();
            if (submenu) {
                shifted = shift_icons(submenu);
            }
            Gtk::Box *box = nullptr;
            auto label = menuitem->get_label();
            if (label.empty()) {
                box = dynamic_cast<Gtk::Box *>(menuitem->get_child());
                if (!box) {
                    continue;
                }  
                std::vector<Gtk::Widget *> children = box->get_children();
                if (children.size() == 2) {
                    auto label_widget = dynamic_cast<Gtk::Label *>(children[1]);
                    if (!label_widget) {
                        label_widget = dynamic_cast<Gtk::Label *>(children[0]);
                    }
                    if (label_widget) {
                        label = label_widget->get_label();
                    }
                }
            }
            if (label.empty()) {
                continue;
            } 
            auto it = label_to_tooltip_map.find(label);
            if (it != label_to_tooltip_map.end()) {
                menuitem->set_tooltip_text(it->second);
            }
            if (shifted || !box) {
                continue;
            }
            width += box->get_spacing() * 1.5; //2 elements 3 halfs to measure
            std::string css_str;
            Glib::RefPtr<Gtk::CssProvider> provider = Gtk::CssProvider::create();
            auto const screen = Gdk::Screen::get_default();
            Gtk::StyleContext::add_provider_for_screen(screen, provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            if (menuitem->get_direction() == Gtk::TEXT_DIR_RTL) {
                css_str = ".shifticonmenu box {margin-right:-" + std::to_string(width) + "px;}";
            } else {
                css_str = ".shifticonmenu box {margin-left:-" + std::to_string(width) + "px;}";
            }
            provider->load_from_data(css_str);
            shifted = true;
        }
    }
    return shifted;
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
