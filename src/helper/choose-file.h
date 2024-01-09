// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SEEN_CHOOSE_FILE_H
#define SEEN_CHOOSE_FILE_H

#include <glibmm/ustring.h>
#include <gtkmm/window.h>
#include <string>
#include <vector>

namespace Inkscape {

// select file for saving data
std::string choose_file_save(Glib::ustring title, Gtk::Window* parent, Glib::ustring mime_type, Glib::ustring file_name, std::string& current_folder);

// open single file for reading data
std::string choose_file_open(Glib::ustring title, Gtk::Window* parent, std::vector<Glib::ustring> mime_types, std::string& current_folder);

} // namespace Inkscape

#endif // SEEN_CHOOSE_FILE_H
