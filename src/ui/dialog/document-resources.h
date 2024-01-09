// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A simple dialog for previewing document resources
 *
 * Copyright (C) 2023 Michael Kowalski
 */

#ifndef SEEN_DOC_RESOURCES_H
#define SEEN_DOC_RESOURCES_H

#include "document.h"
#include "helper/auto-connection.h"
#include "ui/dialog/dialog-base.h"
#include "ui/widget/entity-entry.h"
#include "ui/widget/registry.h"
#include <cstddef>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/iconview.h>
#include <gtkmm/liststore.h>
#include <gtkmm/searchentry.h>
#include <gtkmm/treeview.h>
#include <memory>
#include <string>
#include <boost/ptr_container/ptr_vector.hpp>

namespace Inkscape {
namespace UI {
namespace Dialog {

namespace details {
    struct Statistics {
        size_t nodes = 0;
        size_t groups = 0;
        size_t layers = 0;
        size_t paths = 0;
        size_t images = 0;
        size_t patterns = 0;
        size_t symbols = 0;
        size_t markers = 0;
        size_t fonts = 0;
        size_t filters = 0;
        size_t svg_fonts = 0;
        size_t colors = 0;
        size_t gradients = 0;
        size_t swatches = 0;
        size_t metadata = 0;
        size_t styles = 0;
        size_t meshgradients = 0;
        size_t colorprofiles = 0;
        size_t external_uris = 0;
    };
}

class DocumentResources : public DialogBase {
public:
    DocumentResources();

private:
    void documentReplaced() override;
    void select_page(const Glib::ustring& id);
    void refresh_page(const Glib::ustring& id);
    void refresh_current_page();
    void rebuild_stats();
    details::Statistics collect_statistics();
    void start_editing(Gtk::CellEditable* cell, const Glib::ustring& path);
    void end_editing(const Glib::ustring& path, const Glib::ustring& new_text);
    void selectionModified(Inkscape::Selection *selection, guint flags) override;
    void update_buttons();
    Gtk::TreeModel::Row selected_item();
    void clear_stores();

    Glib::RefPtr<Gtk::Builder> _builder;
    Glib::RefPtr<Gtk::ListStore> _item_store;
    Glib::RefPtr<Gtk::TreeModelFilter> _categories;
    Glib::RefPtr<Gtk::ListStore> _info_store;
    Gtk::CellRendererPixbuf _image_renderer;
    SPDocument* _document = nullptr;
    auto_connection _selection_change;
    details::Statistics _stats;
    std::string _cur_page_id; // the last category that user selected
    int _showing_resource = -1; // ID of the resource that's currently presented
    Glib::RefPtr<Gtk::TreeSelection> _page_selection;
    Gtk::IconView& _iconview;
    Gtk::TreeView& _treeview;
    Gtk::TreeView& _selector;
    Gtk::Button& _edit;
    Gtk::Button& _select;
    Gtk::Button& _delete;
    Gtk::Button& _extract;
    Gtk::SearchEntry& _search;
    boost::ptr_vector<Inkscape::UI::Widget::EntityEntry> _rdf_list;
    UI::Widget::Registry _wr;
    Gtk::CellRendererText* _label_renderer;
    auto_connection _document_modified;
    auto_connection _idle_refresh;
};

} } } // namespaces

#endif // SEEN_DOC_RESOURCES_H
