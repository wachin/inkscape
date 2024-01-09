// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *
 * Copyright (C) 1999-2007, 2021 Authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SP_EXPORT_BATCH_H
#define SP_EXPORT_BATCH_H

#include <gtkmm.h>

#include "helper/auto-connection.h"
#include "ui/widget/export-preview.h"
#include "ui/widget/scrollprotected.h"


class ExportProgressDialog;
class InkscapeApplication;
class SPDesktop;
class SPDocument;
class SPItem;
class SPPage;

namespace Inkscape {
class Preferences;
class Selection;

namespace UI {

namespace Widget {
class ColorPicker;
} // namespace Widget

namespace Dialog {

class ExportList;

class BatchItem : public Gtk::FlowBoxChild
{
public:
    BatchItem(SPItem *item, std::shared_ptr<PreviewDrawing> drawing);
    BatchItem(SPPage *page, std::shared_ptr<PreviewDrawing> drawing);
    ~BatchItem() override = default;

    Glib::ustring getLabel() { return _label_str; }
    SPItem *getItem() { return _item; }
    SPPage *getPage() { return _page; }
    void refresh(bool hide, guint32 bg_color);
    void setDrawing(std::shared_ptr<PreviewDrawing> drawing) { _preview.setDrawing(drawing); }

    auto get_radio_group() { return _option.get_group(); }
    void on_parent_changed(Gtk::Widget *) override;
    void on_mode_changed(Gtk::SelectionMode mode);
    void set_selected(bool selected);
    void update_selected();

private:
    void init(std::shared_ptr<PreviewDrawing> drawing);
    void update_label();

    Glib::ustring _label_str;
    Gtk::Grid _grid;
    Gtk::Label _label;
    Gtk::CheckButton _selector;
    Gtk::RadioButton _option;
    ExportPreview _preview;
    SPItem *_item = nullptr;
    SPPage *_page = nullptr;
    bool is_hide = false;

    auto_connection _selection_widget_changed_conn;
    auto_connection _object_modified_conn;
};

class BatchExport : public Gtk::Box
{
public:
    BatchExport(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder>& builder);
    ~BatchExport() override = default;

    void setApp(InkscapeApplication *app) { _app = app; }
    void setDocument(SPDocument *document);
    void setDesktop(SPDesktop *desktop);
    void selectionChanged(Inkscape::Selection *selection);
    void selectionModified(Inkscape::Selection *selection, guint flags);
    void pagesChanged();
    void queueRefreshItems();
    void queueRefresh();

private:
    enum selection_mode
    {
        SELECTION_LAYER = 0, // Default is alaways placed first
        SELECTION_SELECTION,
        SELECTION_PAGE,
    };

    typedef Inkscape::UI::Widget::ScrollProtected<Gtk::SpinButton> SpinButton;

    InkscapeApplication *_app;
    SPDesktop *_desktop = nullptr;
    SPDocument *_document = nullptr;
    std::shared_ptr<PreviewDrawing> _preview_drawing;
    bool setupDone = false; // To prevent setup() call add connections again.

    std::map<selection_mode, Gtk::RadioButton *> selection_buttons;
    Gtk::FlowBox *preview_container = nullptr;
    Gtk::CheckButton *show_preview = nullptr;
    Gtk::Label *num_elements = nullptr;
    Gtk::CheckButton *hide_all = nullptr;
    Gtk::Entry *filename_entry = nullptr;
    Gtk::Button *export_btn = nullptr;
    Gtk::Button *cancel_btn = nullptr;
    Gtk::ProgressBar *_prog = nullptr;
    Gtk::ProgressBar *_prog_batch = nullptr;
    ExportList *export_list = nullptr;
    Gtk::Widget *progress_box = nullptr;

    // Store all items to be displayed in flowbox
    std::map<std::string, std::unique_ptr<BatchItem>> current_items;

    Glib::ustring original_name;
    Glib::ustring doc_export_name;

    Inkscape::Preferences *prefs = nullptr;
    std::map<selection_mode, Glib::ustring> selection_names;
    selection_mode current_key;

    // initialise variables from builder
    void initialise(const Glib::RefPtr<Gtk::Builder> &builder);
    void setup();
    void setDefaultSelectionMode();
    void onFilenameModified();
    void onAreaTypeToggle(selection_mode key);
    void onExport();
    void onCancel();
    void onBrowse(Gtk::EntryIconPosition pos, const GdkEventButton *ev);

    void refreshPreview();
    void refreshItems();
    void loadExportHints();

    void setExporting(bool exporting, Glib::ustring const &text = "", Glib::ustring const &test_batch = "");

    static unsigned int onProgressCallback(float value, void *);

    bool interrupted;

    // Gtk Signals
    auto_connection filename_conn;
    auto_connection export_conn;
    auto_connection cancel_conn;
    auto_connection browse_conn;
    auto_connection refresh_conn;
    auto_connection refresh_items_conn;
    // SVG Signals
    auto_connection _pages_changed_connection;

    std::unique_ptr<Inkscape::UI::Widget::ColorPicker> _bgnd_color_picker;
};
} // namespace Dialog
} // namespace UI
} // namespace Inkscape
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
