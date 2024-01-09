// SPDX-License-Identifier: GPL-2.0-or-later

#include <functional>
#include <gtkmm/enums.h>
#include <optional>

#include <glibmm/ustring.h>
#include <gtkmm/grid.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <utility>

#ifndef COLUMN_MENU_BUILDER_INCLUDED
#define COLUMN_MENU_BUILDER_INCLUDED

namespace Inkscape {
namespace UI {

template<typename T>
class ColumnMenuBuilder {
public:
    ColumnMenuBuilder(Gtk::Menu& menu, int columns, Gtk::IconSize icon_size = Gtk::ICON_SIZE_MENU)
        : _menu(menu), _columns(columns), _icon_size(static_cast<int>(icon_size)) {}

    Gtk::MenuItem* add_item(Glib::ustring label, T section, Glib::ustring tooltip, Glib::ustring icon_name, bool sensitive, bool customtooltip, std::function<void ()> callback) {
        _new_section = false;
        _section = nullptr;
        if (!_last_section || *_last_section != section) {
            _new_section = true;
        }

        if (_new_section) {
            if (_col > 0) _row++;

            // add separator
            if (_row > 0) {
                auto separator = Gtk::make_managed<Gtk::SeparatorMenuItem>();
                separator->show();
                _menu.attach(*separator, 0, _columns, _row, _row + 1);
                _row++;
            }

            _last_section = section;

            auto sep = Gtk::make_managed<Gtk::MenuItem>();
            sep->get_style_context()->add_class("menu-category");
            sep->set_sensitive(false);
            sep->show();
            _menu.attach(*sep, 0, _columns, _row, _row + 1);
            _section = sep;
            _col = 0;
            _row++;
        }

        auto item = Gtk::make_managed<Gtk::MenuItem>();
        auto grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_column_spacing(8);
        grid->insert_row(0);
        grid->insert_column(0);
        grid->insert_column(1);
        grid->attach(*Gtk::make_managed<Gtk::Image>(std::move(icon_name), _icon_size), 0, 0);
        grid->attach(*Gtk::make_managed<Gtk::Label>(std::move(label), Gtk::ALIGN_START, Gtk::ALIGN_CENTER, true), 1, 0);
        grid->set_sensitive(sensitive);
        item->add(*grid);
        if (!customtooltip) {
            item->set_tooltip_markup(std::move(tooltip));
        }
        item->set_sensitive(sensitive);
        item->signal_activate().connect(callback);
        item->show_all();
        _menu.attach(*item, _col, _col + 1, _row, _row + 1);
        _col++;
        if (_col >= _columns) {
            _col = 0;
            _row++;
        }

        return item;
    }

    bool new_section() {
        return _new_section;
    }

    void set_section(Glib::ustring name) {
        // name lastest section
        if (_section) {
            _section->set_label(name.uppercase());
        }
    }

private:
    int _row = 0;
    int _col = 0;
    int _columns;
    Gtk::Menu& _menu;
    bool _new_section = false;
    std::optional<T> _last_section;
    Gtk::MenuItem* _section = nullptr;
    Gtk::IconSize _icon_size;
};

}} // namespace

#endif // COLUMN_MENU_BUILDER_INCLUDED