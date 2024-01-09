// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Undo History dialog - implementation.
 */
/* Author:
 *   Gustav Broberg <broberg@kth.se>
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "undo-history.h"

#include "actions/actions-tools.h"
#include "document-undo.h"
#include "document.h"
#include "inkscape.h"
#include "ui/icon-loader.h"
#include "util/signal-blocker.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

/* Rendering functions for custom cell renderers */
void CellRendererSPIcon::render_vfunc(const Cairo::RefPtr<Cairo::Context>& cr,
                                      Gtk::Widget& widget,
                                      const Gdk::Rectangle& background_area,
                                      const Gdk::Rectangle& cell_area,
                                      Gtk::CellRendererState flags)
{
    // if there is no icon name.
    if ( _property_icon_name == "") return;

    // if the icon isn't cached, render it to a pixbuf
    if ( !_icon_cache[_property_icon_name] ) {

        Gtk::Image* icon = Gtk::manage(new Gtk::Image());
        icon = sp_get_icon_image(_property_icon_name, Gtk::ICON_SIZE_MENU);

        if (icon) {

            // check icon type (inkscape, gtk, none)
            if ( GTK_IS_IMAGE(icon->gobj()) ) {
                _property_icon = sp_get_icon_pixbuf(_property_icon_name, 16);
            } else {
                delete icon;
                return;
            }

            delete icon;
            property_pixbuf() = _icon_cache[_property_icon_name] = _property_icon.get_value();
        }

    } else {
        property_pixbuf() = _icon_cache[_property_icon_name];
    }

    Gtk::CellRendererPixbuf::render_vfunc(cr, widget, background_area,
                                          cell_area, flags);
}


void CellRendererInt::render_vfunc(const Cairo::RefPtr<Cairo::Context>& cr,
                                   Gtk::Widget& widget,
                                   const Gdk::Rectangle& background_area,
                                   const Gdk::Rectangle& cell_area,
                                   Gtk::CellRendererState flags)
{
    if( _filter(_property_number) ) {
        std::ostringstream s;
        s << _property_number << std::flush;
        property_text() = s.str();
        Gtk::CellRendererText::render_vfunc(cr, widget, background_area,
                                            cell_area, flags);
    }
}

const CellRendererInt::Filter& CellRendererInt::no_filter = CellRendererInt::NoFilter();

UndoHistory::UndoHistory()
    : DialogBase("/dialogs/undo-history", "UndoHistory"),
      _event_log(nullptr),
      _scrolled_window(),
      _event_list_store(),
      _event_list_selection(_event_list_view.get_selection()),
      _callback_connections()
{
    auto *_columns = &EventLog::getColumns();

    set_size_request(-1, -1);

    pack_start(_scrolled_window);
    _scrolled_window.set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

    _event_list_view.set_enable_search(false);
    _event_list_view.set_headers_visible(false);

    CellRendererSPIcon* icon_renderer = Gtk::manage(new CellRendererSPIcon());
    icon_renderer->property_xpad() = 2;
    icon_renderer->property_width() = 24;
    int cols_count = _event_list_view.append_column("Icon", *icon_renderer);

    Gtk::TreeView::Column* icon_column = _event_list_view.get_column(cols_count-1);
    icon_column->add_attribute(icon_renderer->property_icon_name(), _columns->icon_name);

    CellRendererInt* children_renderer = Gtk::manage(new CellRendererInt(greater_than_1));
    children_renderer->property_weight() = 600; // =Pango::WEIGHT_SEMIBOLD (not defined in old versions of pangomm)
    children_renderer->property_xalign() = 1.0;
    children_renderer->property_xpad() = 2;
    children_renderer->property_width() = 24;

    cols_count = _event_list_view.append_column("Children", *children_renderer);
    Gtk::TreeView::Column* children_column = _event_list_view.get_column(cols_count-1);
    children_column->add_attribute(children_renderer->property_number(), _columns->child_count);

    Gtk::CellRendererText* description_renderer = Gtk::manage(new Gtk::CellRendererText());
    description_renderer->property_ellipsize() = Pango::ELLIPSIZE_END;

    cols_count = _event_list_view.append_column("Description", *description_renderer);
    Gtk::TreeView::Column* description_column = _event_list_view.get_column(cols_count-1);
    description_column->add_attribute(description_renderer->property_text(), _columns->description);
    description_column->set_resizable();
    description_column->set_sizing(Gtk::TREE_VIEW_COLUMN_AUTOSIZE);
    description_column->set_min_width (150);

    _event_list_view.set_expander_column( *_event_list_view.get_column(cols_count-1) );

    _scrolled_window.add(_event_list_view);
    _scrolled_window.set_overlay_scrolling(false);
    // connect EventLog callbacks
    _callback_connections[EventLog::CALLB_SELECTION_CHANGE] =
        _event_list_selection->signal_changed().connect(sigc::mem_fun(*this, &Inkscape::UI::Dialog::UndoHistory::_onListSelectionChange));

    _callback_connections[EventLog::CALLB_EXPAND] =
        _event_list_view.signal_row_expanded().connect(sigc::mem_fun(*this, &Inkscape::UI::Dialog::UndoHistory::_onExpandEvent));

    _callback_connections[EventLog::CALLB_COLLAPSE] =
        _event_list_view.signal_row_collapsed().connect(sigc::mem_fun(*this, &Inkscape::UI::Dialog::UndoHistory::_onCollapseEvent));

    show_all_children();
}

UndoHistory::~UndoHistory()
{
    disconnectEventLog();
}

void UndoHistory::documentReplaced()
{
    disconnectEventLog();
    if (auto document = getDocument()) {
        g_assert (document->get_event_log() != nullptr);
        SignalBlocker blocker(&_callback_connections[EventLog::CALLB_SELECTION_CHANGE]);
        _event_list_view.unset_model();
        connectEventLog();
    }
}

void UndoHistory::disconnectEventLog()
{
    if (_event_log) {
        _event_log->removeDialogConnection(&_event_list_view, &_callback_connections);
        _event_log->remove_destroy_notify_callback(this);
    }
}

void UndoHistory::connectEventLog()
{
    if (auto document = getDocument()) {
        _event_log = document->get_event_log();
        _event_log->add_destroy_notify_callback(this, &_handleEventLogDestroyCB);
        _event_list_store = _event_log->getEventListStore();
        _event_list_view.set_model(_event_list_store);
        _event_log->addDialogConnection(&_event_list_view, &_callback_connections);
        _event_list_view.scroll_to_row(_event_list_store->get_path(_event_list_selection->get_selected()));
    }
}

void *UndoHistory::_handleEventLogDestroyCB(void *data)
{
    void *result = nullptr;
    if (data) {
        UndoHistory *self = reinterpret_cast<UndoHistory*>(data);
        result = self->_handleEventLogDestroy();
    }
    return result;
}

// called *after* _event_log has been destroyed.
void *UndoHistory::_handleEventLogDestroy()
{
    if (_event_log) {
        SignalBlocker blocker(&_callback_connections[EventLog::CALLB_SELECTION_CHANGE]);

        _event_list_view.unset_model();
        _event_list_store.reset();
        _event_log = nullptr;
    }

    return nullptr;
}

void
UndoHistory::_onListSelectionChange()
{

    EventLog::const_iterator selected = _event_list_selection->get_selected();

    /* If no event is selected in the view, find the right one and select it. This happens whenever
     * a branch we're currently in is collapsed.
     */
    if (!selected) {
        EventLog::iterator curr_event = _event_log->getCurrEvent();

        if (curr_event->parent()) {

            EventLog::iterator curr_event_parent = curr_event->parent();
            EventLog::iterator last = curr_event_parent->children().end();

            _event_log->blockNotifications();
            for ( --last ; curr_event != last ; ++curr_event ) {
                DocumentUndo::redo(getDocument());
            }
            _event_log->blockNotifications(false);

            _event_log->setCurrEvent(curr_event);
            _event_list_selection->select(curr_event_parent);

        } else {  // this should not happen
            _event_list_selection->select(curr_event);
        }

    } else {

        EventLog::const_iterator last_selected = _event_log->getCurrEvent();

        /* Selecting a collapsed parent event is equal to selecting the last child
         * of that parent's branch.
         */

        if ( !selected->children().empty() &&
             !_event_list_view.row_expanded(_event_list_store->get_path(selected)) )
        {
            selected = selected->children().end();
            --selected;
        }

        // An event before the current one has been selected. Undo to the selected event.
        if ( _event_list_store->get_path(selected) <
             _event_list_store->get_path(last_selected) )
        {
            _event_log->blockNotifications();

            while ( selected != last_selected ) {

                DocumentUndo::undo(getDocument());

                if ( last_selected->parent() &&
                     last_selected == last_selected->parent()->children().begin() )
                {
                    last_selected = last_selected->parent();
                    _event_log->setCurrEventParent((EventLog::iterator)nullptr);
                } else {
                    --last_selected;
                    if ( !last_selected->children().empty() ) {
                        _event_log->setCurrEventParent(last_selected);
                        last_selected = last_selected->children().end();
                        --last_selected;
                    }
                }
            }
            _event_log->blockNotifications(false);
            _event_log->updateUndoVerbs();

        } else { // An event after the current one has been selected. Redo to the selected event.

            _event_log->blockNotifications();

            while (last_selected && selected != last_selected ) {

                DocumentUndo::redo(getDocument());

                if ( !last_selected->children().empty() ) {
                    _event_log->setCurrEventParent(last_selected);
                    last_selected = last_selected->children().begin();
                } else {
                    ++last_selected;
                    if ( last_selected->parent() &&
                         last_selected == last_selected->parent()->children().end() )
                    {
                        last_selected = last_selected->parent();
                        ++last_selected;
                        _event_log->setCurrEventParent((EventLog::iterator)nullptr);
                    }
                }
            }
            _event_log->blockNotifications(false);

        }

        _event_log->setCurrEvent(selected);
        _event_log->updateUndoVerbs();
    }
}

void
UndoHistory::_onExpandEvent(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &/*path*/)
{
    if ( iter == _event_list_selection->get_selected() ) {
        _event_list_selection->select(_event_log->getCurrEvent());
    }
}

void
UndoHistory::_onCollapseEvent(const Gtk::TreeModel::iterator &iter, const Gtk::TreeModel::Path &/*path*/)
{
    // Collapsing a branch we're currently in is equal to stepping to the last event in that branch
    if ( iter == _event_log->getCurrEvent() ) {
        EventLog::const_iterator curr_event_parent = _event_log->getCurrEvent();
        EventLog::const_iterator curr_event = curr_event_parent->children().begin();
        EventLog::const_iterator last = curr_event_parent->children().end();

        _event_log->blockNotifications();
        DocumentUndo::redo(getDocument());

        for ( --last ; curr_event != last ; ++curr_event ) {
            DocumentUndo::redo(getDocument());
        }
        _event_log->blockNotifications(false);

        _event_log->setCurrEvent(curr_event);
        _event_log->setCurrEventParent(curr_event_parent);
        _event_list_selection->select(curr_event_parent);
    }
}

const CellRendererInt::Filter& UndoHistory::greater_than_1 = UndoHistory::GreaterThan(1);

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
