// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef INKSCAPE_UI_WIDGET_COMPLETION_POPUP_H
#define INKSCAPE_UI_WIDGET_COMPLETION_POPUP_H

#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/builder.h>
#include <gtkmm/entrycompletion.h>
#include <gtkmm/liststore.h>
#include <gtkmm/menu.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/searchentry.h>
#include <sigc++/connection.h>
#include "labelled.h"

namespace Inkscape {
namespace UI {
namespace Widget {

class CompletionPopup : public Gtk::Box {
public:
    CompletionPopup();

    Gtk::Menu& get_menu();
    Gtk::SearchEntry& get_entry();
    Glib::RefPtr<Gtk::ListStore> get_list();

    void clear_completion_list();
    void add_to_completion_list(int id, Glib::ustring name, Glib::ustring icon_name, Glib::ustring search_text = Glib::ustring());

    sigc::signal<void (int)>& on_match_selected();
    sigc::signal<void ()>& on_button_press();
    sigc::signal<bool ()>& on_focus();

private:
    Glib::RefPtr<Gtk::Builder> _builder;
    Glib::RefPtr<Gtk::ListStore> _list;
    Gtk::SearchEntry& _search;
    Gtk::MenuButton& _button;
    Gtk::Menu& _popup;
    Glib::RefPtr<Gtk::EntryCompletion> _completion;
    sigc::signal<void (int)> _match_selected;
    sigc::signal<void ()> _button_press;
    sigc::signal<bool ()> _on_focus;
};

}}} // namespaces

#endif // INKSCAPE_UI_WIDGET_COMPLETION_POPUP_H
