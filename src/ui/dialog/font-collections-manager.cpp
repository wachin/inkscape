// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog to manage the font collections.
 */
/* Authors:
 *   Vaibhav Malik
 *
 * Released under GNU GPLv2 or later, read the file 'COPYING' for more information
 */

#include "font-collections-manager.h"

#include "io/resource.h"
#include "ui/icon-names.h"
#include "util/font-collections.h"

#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>
#include "libnrtype/font-lister.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

FontCollectionsManager::FontCollectionsManager()
    : DialogBase("/dialogs/fontcollections", "FontCollections")
{
    std::string gladefile = get_filename_string(Inkscape::IO::Resource::UIS, "dialog-font-collections.glade");
    Glib::RefPtr<Gtk::Builder> builder;
    try {
        builder = Gtk::Builder::create_from_file(gladefile);
    } catch (const Glib::Error &ex) {
        g_error("Cannot load glade file: %s", ex.what().c_str());
        throw;
    }

    builder->get_widget("contents", _contents);
    builder->get_widget("paned", _paned);
    builder->get_widget("collections_box", _collections_box);
    builder->get_widget("buttons_box", _buttons_box);
    builder->get_widget("font_list_box", _font_list_box);
    builder->get_widget("font_count_label", _font_count_label);
    builder->get_widget("font_list_filter_box", _font_list_filter_box);
    builder->get_widget("search_entry", _search_entry);
    builder->get_widget("reset_button", _reset_button);
    builder->get_widget("create_button", _create_button);
    builder->get_widget("edit_button", _edit_button);
    builder->get_widget("delete_button", _delete_button);

    _font_list_box->pack_start(_font_selector, true, true);
    _font_list_box->reorder_child(_font_selector, 1);

    _collections_box->pack_start(_user_font_collections, true, true);
    _collections_box->reorder_child(_user_font_collections, 0);

    _user_font_collections.populate_system_collections();
    _user_font_collections.populate_user_collections();
    _user_font_collections.change_frame_name(_("Font Collections"));

    add(*_contents);

    // Set the button images.
    _create_button->set_image_from_icon_name(INKSCAPE_ICON("list-add"));
    _edit_button->set_image_from_icon_name(INKSCAPE_ICON("document-edit"));
    _delete_button->set_image_from_icon_name(INKSCAPE_ICON("edit-delete"));

    // Paned settings.
    _paned->child_property_resize(*_paned->get_child1()) = false;
    _paned->child_property_resize(*_paned->get_child2()) = true;

    change_font_count_label();
    _font_selector.hide_others();
    show_all_children();

    // Setup the signals.
    _font_count_changed_connection = Inkscape::FontLister::get_instance()->connectUpdate(sigc::mem_fun(*this, &FontCollectionsManager::change_font_count_label));
    _search_entry->signal_search_changed().connect([=](){ on_search_entry_changed(); });
    _user_font_collections.connect_signal_changed([=](int s){ on_selection_changed(s); });
    _create_button->signal_clicked().connect([=](){ on_create_button_pressed(); });
    _edit_button->signal_clicked().connect([=](){ on_edit_button_pressed(); });
    _delete_button->signal_clicked().connect([=](){ on_delete_button_pressed(); });
    _reset_button->signal_clicked().connect([=](){ on_reset_button_pressed(); });

    // Edit and delete are initially insensitive because nothing is selected.
    _edit_button->set_sensitive(false);
    _delete_button->set_sensitive(false);
}

void FontCollectionsManager::on_search_entry_changed()
{
    auto search_txt = _search_entry->get_text();
    _font_selector.unset_model();
    Inkscape::FontLister *font_lister = Inkscape::FontLister::get_instance();
    font_lister->show_results(search_txt);
    _font_selector.set_model();
    change_font_count_label();
}

void FontCollectionsManager::on_create_button_pressed()
{
    _user_font_collections.on_create_collection();
}

void FontCollectionsManager::on_delete_button_pressed()
{
    _user_font_collections.on_delete_button_pressed();
}

void FontCollectionsManager::on_edit_button_pressed()
{
    _user_font_collections.on_edit_button_pressed();
}

void FontCollectionsManager::on_reset_button_pressed()
{
    _search_entry->set_text("");
    Inkscape::FontLister* font_lister = Inkscape::FontLister::get_instance();

    if(font_lister->get_font_families_size() == font_lister->get_font_list()->children().size()) {
        // _user_font_collections.populate_collections();
        return;
    }

    Inkscape::FontCollections::get()->clear_selected_collections();
    font_lister->init_font_families();
    font_lister->init_default_styles();
    SPDocument *document = getDesktop()->getDocument();
    font_lister->add_document_fonts_at_top(document);
}

void FontCollectionsManager::change_font_count_label()
{
    auto label = Inkscape::FontLister::get_instance()->get_font_count_label();
    _font_count_label->set_label(label);
}

// This function will set the sensitivity of the edit and delete buttons
// Whenever the selection changes.
void FontCollectionsManager::on_selection_changed(int state)
{
    bool edit = false, del = false;
    switch(state) {
        case SYSTEM_COLLECTION:
            break;
        case USER_COLLECTION:
            edit = true;
            del = true;
            break;
        case USER_COLLECTION_FONT:
            edit = false;
            del = true;
        default:
            break;
    }
    _edit_button->set_sensitive(edit);
    _delete_button->set_sensitive(del);
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

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
