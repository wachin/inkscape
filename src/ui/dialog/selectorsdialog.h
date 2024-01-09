// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief A dialog for CSS selectors
 */
/* Authors:
 *   Kamalpreet Kaur Grewal
 *   Tavmjong Bah
 *
 * Copyright (C) Kamalpreet Kaur Grewal 2016 <grewalkamal005@gmail.com>
 * Copyright (C) Tavmjong Bah 2017 <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SELECTORSDIALOG_H
#define SELECTORSDIALOG_H

#include <gtkmm/dialog.h>
#include <gtkmm/paned.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/switch.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>
#include <memory>
#include <vector>

#include "ui/dialog/dialog-base.h"
#include "ui/dialog/styledialog.h"
#include "xml/helper-observer.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

/**
 * @brief The SelectorsDialog class
 * A list of CSS selectors will show up in this dialog. This dialog allows one to
 * add and delete selectors. Elements can be added to and removed from the selectors
 * in the dialog. Selection of any selector row selects the matching  objects in
 * the drawing and vice-versa. (Only simple selectors supported for now.)
 *
 * This class must keep two things in sync:
 *   1. The text node of the style element.
 *   2. The Gtk::TreeModel.
 */
class SelectorsDialog : public DialogBase
{
public:
    SelectorsDialog();
    ~SelectorsDialog() override;

    void update() override;
    void desktopReplaced() override;
    void documentReplaced() override;
    void selectionChanged(Selection *selection) override;

  private:
    // Monitor <style> element for changes.
    class NodeObserver;

    void removeObservers();

    // Monitor all objects for addition/removal/attribute change
    class NodeWatcher;
    enum SelectorType { CLASS, ID, TAG };
    void _nodeAdded(   Inkscape::XML::Node &repr );
    void _nodeRemoved( Inkscape::XML::Node &repr );
    void _nodeChanged( Inkscape::XML::Node &repr );
    // Data structure
    enum coltype { OBJECT, SELECTOR, OTHER };
    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {
            add(_colSelector);
            add(_colExpand);
            add(_colType);
            add(_colObj);
            add(_colProperties);
            add(_colVisible);
            add(_colSelected);
        }
        Gtk::TreeModelColumn<Glib::ustring> _colSelector;       // Selector or matching object id.
        Gtk::TreeModelColumn<bool> _colExpand;                  // Open/Close store row.
        Gtk::TreeModelColumn<gint> _colType;                    // Selector row or child object row.
        Gtk::TreeModelColumn<SPObject *> _colObj;               // Matching object (if any).
        Gtk::TreeModelColumn<Glib::ustring> _colProperties;     // List of properties.
        Gtk::TreeModelColumn<bool> _colVisible;                 // Make visible or not.
        Gtk::TreeModelColumn<gint> _colSelected;                // Make selected.
    };
    ModelColumns _mColumns;

    // Override Gtk::TreeStore to control drag-n-drop (only allow dragging and dropping of selectors).
    // See: https://developer.gnome.org/gtkmm-tutorial/stable/sec-treeview-examples.html.en
    //
    // TreeStore implements simple drag and drop (DND) but there appears no way to know when a DND
    // has been completed (other than doing the whole DND ourselves). As a hack, we use
    // on_row_deleted to trigger write of style element.
    class TreeStore : public Gtk::TreeStore {
    protected:
        TreeStore();
        bool row_draggable_vfunc(const Gtk::TreeModel::Path& path) const override;
        bool row_drop_possible_vfunc(const Gtk::TreeModel::Path& path,
                                     const Gtk::SelectionData& selection_data) const override;
        void on_row_deleted(const TreeModel::Path& path) override;

    public:
      static Glib::RefPtr<SelectorsDialog::TreeStore> create(SelectorsDialog *styledialog);

    private:
      SelectorsDialog *_selectorsdialog;
    };

    // TreeView
    Glib::RefPtr<Gtk::TreeModelFilter> _modelfilter;
    Glib::RefPtr<TreeStore> _store;
    Gtk::TreeView _treeView;
    Gtk::TreeModel::Path _lastpath;
    // Widgets
    StyleDialog *_style_dialog;
    Gtk::Paned _paned;
    Glib::RefPtr<Gtk::Adjustment> _vadj;
    Gtk::Box _button_box;
    Gtk::Box _selectors_box;
    Gtk::ScrolledWindow _scrolled_window_selectors;

    Gtk::Button _del;
    Gtk::Button _create;
    // Reading and writing the style element.
    Inkscape::XML::Node *_getStyleTextNode(bool create_if_missing = false);
    void _readStyleElement();
    void _writeStyleElement();

    // Update watchers
    std::unique_ptr<Inkscape::XML::NodeObserver> m_nodewatcher;
    std::unique_ptr<Inkscape::XML::NodeObserver> m_styletextwatcher;

    // Manipulate Tree
    void _addToSelector(Gtk::TreeModel::Row row);
    void _removeFromSelector(Gtk::TreeModel::Row row);
    Glib::ustring _getIdList(std::vector<SPObject *>);
    std::vector<SPObject *> _getObjVec(Glib::ustring selector);
    void _insertClass(const std::vector<SPObject *>& objVec, const Glib::ustring& className);
    void _insertClass(SPObject *obj, const Glib::ustring &className);
    void _removeClass(const std::vector<SPObject *> &objVec, const Glib::ustring &className, bool all = false);
    void _removeClass(SPObject *obj, const Glib::ustring &className, bool all = false);
    void _toggleDirection(Gtk::RadioButton *vertical);
    void _showWidgets();

    void _selectObjects(int, int);
    // Variables
    double _scrollpos{0.0};
    bool _scrollock{false};
    bool _updating{false};          // Prevent cyclic actions: read <-> write, select via dialog <-> via desktop
    Inkscape::XML::Node *m_root{nullptr};
    Inkscape::XML::Node *_textNode{nullptr}; // Track so we know when to add a NodeObserver.

    void _rowExpand(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path);
    void _rowCollapse(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &path);
    void _closeDialog(Gtk::Dialog *textDialogPtr);

    Inkscape::XML::SignalObserver _objObserver; // Track object in selected row (for style change).

    // Signal and handlers - Internal
    void _addSelector();
    void _delSelector();
    static Glib::ustring _getSelectorClasses(Glib::ustring selector);
    bool _handleButtonEvent(GdkEventButton *event);
    void _buttonEventsSelectObjs(GdkEventButton *event);
    void _selectRow(); // Select row in tree when selection changed.
    void _vscroll();

    // GUI
    void _styleButton(Gtk::Button& btn, char const* iconName, char const* tooltip);
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // SELECTORSDIALOG_H

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
