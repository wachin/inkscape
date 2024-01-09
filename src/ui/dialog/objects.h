// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A simple dialog for objects UI.
 *
 * Authors:
 *   Theodore Janeczko
 *   Tavmjong Bah
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *               Tavmjong Bah 2017
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_OBJECTS_PANEL_H
#define SEEN_OBJECTS_PANEL_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/dialog.h>
#include <gtkmm/modelbutton.h>
#include <gtkmm/popover.h>
#include <gtkmm/scale.h>

#include "helper/auto-connection.h"
#include "xml/node-observer.h"

#include "ui/dialog/dialog-base.h"
#include "ui/widget/color-picker.h"
#include "ui/widget/preferences-widget.h"

#include "selection.h"
#include "style-enums.h"
#include "color-rgba.h"

using Inkscape::XML::Node;
using namespace Inkscape::UI::Widget;

class SPObject;
class SPGroup;
// struct SPColorSelector;

namespace Inkscape {
namespace UI {

namespace Widget { class ImageToggler; }
namespace Dialog {

class ObjectsPanel;
class ObjectWatcher;

enum {COL_LABEL, COL_VISIBLE, COL_LOCKED};

using SelectionState = int;
enum SelectionStates : SelectionState {
    SELECTED_NOT = 0,     // Object is NOT in desktop's selection
    SELECTED_OBJECT = 1,  // Object is in the desktop's selection
    LAYER_FOCUSED = 2,    // This layer is the desktop's focused layer
    LAYER_FOCUS_CHILD = 4 // This object is a child of the focused layer
};

/**
 * A panel that displays objects.
 */
class ObjectsPanel : public DialogBase
{
public:
    ObjectsPanel();
    ~ObjectsPanel() override;

    class ModelColumns;

protected:
    void desktopReplaced() override;
    void documentReplaced() override;
    void layerChanged(SPObject *obj);
    void selectionChanged(Selection *selected) override;
    ObjectWatcher *unpackToObject(SPObject *item);

    // Accessed by ObjectWatcher directly (friend class)
    SPObject* getObject(Node *node);
    ObjectWatcher* getWatcher(Node *node);
    ObjectWatcher *getRootWatcher() const { return root_watcher; };
    bool showChildInTree(SPItem *item);

    Node *getRepr(Gtk::TreeModel::Row const &row) const;
    SPItem *getItem(Gtk::TreeModel::Row const &row) const;
    std::optional<Gtk::TreeRow> getRow(SPItem *item) const;

    bool isDummy(Gtk::TreeModel::Row const &row) const { return getRepr(row) == nullptr; }
    bool hasDummyChildren(Gtk::TreeModel::Row const &row) const;
    bool removeDummyChildren(Gtk::TreeModel::Row const &row);
    bool cleanDummyChildren(Gtk::TreeModel::Row const &row);

    Glib::RefPtr<Gtk::TreeStore> _store;
    ModelColumns* _model;

    void setRootWatcher();
private:

    Glib::RefPtr<Gtk::Builder> _builder;
    Inkscape::PrefObserver _watch_object_mode;
    ObjectWatcher* root_watcher;
    SPItem *current_item = nullptr;

    Inkscape::auto_connection layer_changed;
    SPObject *_layer;
    Gtk::TreeModel::RowReference _hovered_row_ref;

    //Show icons in the context menu
    bool _show_contextmenu_icons;
    bool _is_editing;
    bool _scroll_lock = false;

    std::vector<Gtk::Widget*> _watching;
    std::vector<Gtk::Widget*> _watchingNonTop;
    std::vector<Gtk::Widget*> _watchingNonBottom;

    Gtk::TreeView _tree;
    Gtk::CellRendererText *_text_renderer;
    Gtk::TreeView::Column *_name_column;
    Gtk::TreeView::Column *_blend_mode_column = nullptr;
    Gtk::TreeView::Column *_eye_column = nullptr;
    Gtk::TreeView::Column *_lock_column = nullptr;
    Gtk::TreeView::Column *_color_tag_column = nullptr;
    Gtk::Box _buttonsRow;
    Gtk::Box _buttonsPrimary;
    Gtk::Box _buttonsSecondary;
    Gtk::SearchEntry& _searchBox;
    Gtk::ScrolledWindow _scroller;
    Gtk::Menu _popupMenu;
    Gtk::Box _page;
    Inkscape::auto_connection _tree_style;
    Inkscape::UI::Widget::ColorPicker _color_picker;
    Gtk::TreeRow _clicked_item_row;

    Gtk::Button *_addBarButton(char const* iconName, char const* tooltip, char const *action_name);

    void _activateAction(const std::string& layerAction, const std::string& selectionAction);

    bool blendModePopup(GdkEventButton* event, Gtk::TreeModel::Row row);
    bool toggleVisible(unsigned int state, Gtk::TreeModel::Row row);
    bool toggleLocked(unsigned int state, Gtk::TreeModel::Row row);

    bool _handleButtonEvent(GdkEventButton *event);
    bool _handleKeyPress(GdkEventKey *event);
    bool _handleKeyEvent(GdkEventKey *event);
    bool _handleMotionEvent(GdkEventMotion* motion_event);
    void _searchActivated();
    void _searchChanged();
    
    void _handleEdited(const Glib::ustring& path, const Glib::ustring& new_text);
    void _handleTransparentHover(bool enabled);
    void _generateTranslucentItems(SPItem *parent);

    bool select_row( Glib::RefPtr<Gtk::TreeModel> const & model, Gtk::TreeModel::Path const & path, bool b );

    bool on_drag_motion(const Glib::RefPtr<Gdk::DragContext> &, int, int, guint) override;
    bool on_drag_drop(const Glib::RefPtr<Gdk::DragContext> &, int, int, guint) override;
    void on_drag_start(const Glib::RefPtr<Gdk::DragContext> &);
    void on_drag_end(const Glib::RefPtr<Gdk::DragContext> &) override;

    bool selectCursorItem(unsigned int state);
    SPItem *_getCursorItem(Gtk::TreeViewColumn *column);

    friend class ObjectWatcher;

    SPItem *_solid_item;
    std::list<SPItem *> _translucent_items;
    int _msg_id;
    Gtk::Popover& _settings_menu;
    Gtk::Popover& _object_menu;
    Gtk::Scale& _opacity_slider;
    std::map<SPBlendMode, Gtk::ModelButton*> _blend_items;
    std::map<SPBlendMode, Glib::ustring> _blend_mode_names;
    Inkscape::UI::Widget::ImageToggler* _item_state_toggler;
    // Special column dragging mode
    Gtk::TreeViewColumn* _drag_column = nullptr;
    PrefCheckButton& _setting_layers;
    PrefCheckButton& _setting_track;
    bool _drag_flip;

    bool _selectionChanged();
    auto_connection _idle_connection;
};



} //namespace Dialogs
} //namespace UI
} //namespace Inkscape



#endif // SEEN_OBJECTS_PANEL_H

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
