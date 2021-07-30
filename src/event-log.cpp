// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Gustav Broberg <broberg@kth.se>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (c) 2014 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "util/signal-blocker.h"

#include "event-log.h"
#include <glibmm/i18n.h>

#include "desktop.h"
#include "inkscape.h"
#include "document.h"

namespace
{

class DialogConnection
{
public:
    DialogConnection(Gtk::TreeView *event_list_view, Inkscape::EventLog::CallbackMap *callback_connections) :
        _event_list_view(event_list_view),
        _callback_connections(callback_connections),
        _event_list_selection(_event_list_view->get_selection())
    {            
    }

    Gtk::TreeView *_event_list_view;

    // Map of connections used to temporary block/unblock callbacks in a TreeView
    Inkscape::EventLog::CallbackMap *_callback_connections;

    Glib::RefPtr<Gtk::TreeSelection> _event_list_selection; /// @todo remove this and use _event_list_view's call
};

class ConnectionMatcher
{
public:
    ConnectionMatcher(Gtk::TreeView *view,
                      Inkscape::EventLog::CallbackMap *callbacks) :
        _view(view),
        _callbacks(callbacks)
    {
    }

    bool operator() (DialogConnection const &dlg)
    {
        return (_view == dlg._event_list_view) && (_callbacks == dlg._callback_connections);
    }

    Gtk::TreeView *_view;
    Inkscape::EventLog::CallbackMap *_callbacks;
};

void addBlocker(std::vector<std::unique_ptr<SignalBlocker> > &blockers, sigc::connection *connection)
{
    blockers.emplace_back(new SignalBlocker(connection));
}


} // namespace

namespace Inkscape {

class EventLogPrivate
{
public:
    EventLogPrivate() :
        _connections()
    {
    }

    bool isConnected() const
    {
        return !_connections.empty();
    }

    void addDialogConnection(Gtk::TreeView *event_list_view,
                             Inkscape::EventLog::CallbackMap *callback_connections,
                             Glib::RefPtr<Gtk::TreeStore> event_list_store,
                             Inkscape::EventLog::iterator &curr_event)
    {
        if (std::find_if(_connections.begin(), _connections.end(), ConnectionMatcher(event_list_view, callback_connections)) != _connections.end()) {
            // skipping
        }
        else
        {
            DialogConnection dlg(event_list_view, callback_connections);

            dlg._event_list_selection->set_mode(Gtk::SELECTION_SINGLE);

            {
                std::vector<std::unique_ptr<SignalBlocker> > blockers;
                addBlocker(blockers, &(*dlg._callback_connections)[Inkscape::EventLog::CALLB_SELECTION_CHANGE]);
                addBlocker(blockers, &(*dlg._callback_connections)[Inkscape::EventLog::CALLB_EXPAND]);

                dlg._event_list_view->expand_to_path(event_list_store->get_path(curr_event));
                dlg._event_list_selection->select(curr_event);
            }
            _connections.push_back(dlg);
        }
    }

    void removeDialogConnection(Gtk::TreeView *event_list_view, Inkscape::EventLog::CallbackMap *callback_connections)
    {
        std::vector<DialogConnection>::iterator it = std::find_if(_connections.begin(), _connections.end(), ConnectionMatcher(event_list_view, callback_connections));
        if (it != _connections.end()) {
            _connections.erase(it);
        }
    }

    void collapseRow(Gtk::TreeModel::Path const &path)
    {
        std::vector<std::unique_ptr<SignalBlocker> > blockers;
        for (auto & _connection : _connections)
        {
            addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_SELECTION_CHANGE]);
            addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_COLLAPSE]);
        }

        for (auto & _connection : _connections)
        {
            _connection._event_list_view->collapse_row(path);
        }
    }

    void selectRow(Gtk::TreeModel::Path const &path)
    {
        std::vector<std::unique_ptr<SignalBlocker> > blockers;
        for (auto & _connection : _connections)
        {
            addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_SELECTION_CHANGE]);
            addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_EXPAND]);
        }

        for (auto & _connection : _connections)
        {
            _connection._event_list_view->expand_to_path(path);
            _connection._event_list_selection->select(path);
            _connection._event_list_view->scroll_to_row(path);
        }
    }

    void clearEventList(Glib::RefPtr<Gtk::TreeStore> eventListStore)
    {
        if (eventListStore) {
            std::vector<std::unique_ptr<SignalBlocker> > blockers;
            for (auto & _connection : _connections)
            {
                addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_SELECTION_CHANGE]);
                addBlocker(blockers, &(*_connection._callback_connections)[Inkscape::EventLog::CALLB_EXPAND]);
            }

            eventListStore->clear();
        }
    }

    std::vector<DialogConnection> _connections;
};

const EventLog::EventModelColumns &EventLog::getColumns()
{
    static const EventModelColumns columns;
    return columns;
}

EventLog::EventLog(SPDocument* document) :
    UndoStackObserver(),
    _priv(new EventLogPrivate()),
    _document (document),
    _event_list_store (Gtk::TreeStore::create(getColumns())),
    _curr_event_parent (nullptr),
    _notifications_blocked (false)
{
    // add initial pseudo event
    Gtk::TreeRow curr_row = *(_event_list_store->append());
    _curr_event = _last_saved = _last_event = curr_row;
    
    auto &_columns = getColumns();
    curr_row[_columns.description] = _("[Unchanged]");
    curr_row[_columns.icon_name] = "document-new";
}

EventLog::~EventLog() {
    // avoid crash by clearing entries here (see bug #1071082)
    _priv->clearEventList(_event_list_store);

    delete _priv;
    _priv = nullptr;
}

void
EventLog::notifyUndoEvent(Event* log) 
{
    if ( !_notifications_blocked ) {
        auto &_columns = getColumns();
    
        // make sure the supplied event matches the next undoable event
        g_return_if_fail ( _getUndoEvent() && (*(_getUndoEvent()))[_columns.event] == log );

        // if we're on the first child event...
        if ( _curr_event->parent() &&
             _curr_event == _curr_event->parent()->children().begin() )
	{
            // ...back up to the parent
            _curr_event = _curr_event->parent();
            _curr_event_parent = (iterator)nullptr;

	} else {

            // if we're about to leave a branch, collapse it
            if ( !_curr_event->children().empty() ) {
                _priv->collapseRow(_event_list_store->get_path(_curr_event));
            }

            --_curr_event;

            // if we're entering a branch, move to the end of it
            if (!_curr_event->children().empty()) {
                _curr_event_parent = _curr_event;
                _curr_event = _curr_event->children().end();
                --_curr_event;
            }
	}

        checkForVirginity();

        // update the view
        if (_priv->isConnected()) {
            Gtk::TreePath curr_path = _event_list_store->get_path(_curr_event);
            _priv->selectRow(curr_path);
        }

        updateUndoVerbs();
    }

}

void
EventLog::notifyRedoEvent(Event* log)
{
    if ( !_notifications_blocked ) {
        auto &_columns = getColumns();

        // make sure the supplied event matches the next redoable event
        g_return_if_fail ( _getRedoEvent() && (*(_getRedoEvent()))[_columns.event] == log );

        // if we're on a parent event...
        if ( !_curr_event->children().empty() ) {

            // ...move to its first child
            _curr_event_parent = _curr_event;
            _curr_event = _curr_event->children().begin();

        } else {
	
            ++_curr_event;

            // if we are about to leave a branch...
            if ( _curr_event->parent() &&
                 _curr_event == _curr_event->parent()->children().end() )
            {

                // ...collapse it
                _priv->collapseRow(_event_list_store->get_path(_curr_event->parent()));

                // ...and move to the next event at parent level
                _curr_event = _curr_event->parent();
                _curr_event_parent = (iterator)nullptr;

                ++_curr_event;
            }
        }

        checkForVirginity();

        // update the view
        if (_priv->isConnected()) {
            Gtk::TreePath curr_path = _event_list_store->get_path(_curr_event);
            _priv->selectRow(curr_path);
        }

        updateUndoVerbs();
    }

}

void 
EventLog::notifyUndoCommitEvent(Event* log)
{
    _clearRedo();

    auto icon_name = log->icon_name;

    Gtk::TreeRow curr_row;
    auto &_columns = getColumns();

    // if the new event is of the same type as the previous then create a new branch
    if ( icon_name == (*_curr_event)[_columns.icon_name] ) {
        if ( !_curr_event_parent ) {
            _curr_event_parent = _curr_event;
        }
        curr_row = *(_event_list_store->append(_curr_event_parent->children()));
        (*_curr_event_parent)[_columns.child_count] = _curr_event_parent->children().size() + 1;
    } else {
        curr_row = *(_event_list_store->append());
        curr_row[_columns.child_count] = 1;

        _curr_event = _last_event = curr_row;

        // collapse if we're leaving a branch
        if (_curr_event_parent) {
            _priv->collapseRow(_event_list_store->get_path(_curr_event_parent));
        }

        _curr_event_parent = (iterator)(nullptr);
    }      

    _curr_event = _last_event = curr_row;

    curr_row[_columns.event] = log;
    curr_row[_columns.icon_name] = icon_name;
    curr_row[_columns.description] = log->description;

    checkForVirginity();

    // update the view
    if (_priv->isConnected()) {
        Gtk::TreePath curr_path = _event_list_store->get_path(_curr_event);
        _priv->selectRow(curr_path);
    }

    updateUndoVerbs();
}

void
EventLog::notifyClearUndoEvent()
{
    _clearUndo();    
    updateUndoVerbs();
}

void
EventLog::notifyClearRedoEvent()
{
    _clearRedo();
    updateUndoVerbs();
}

void  EventLog::addDialogConnection(Gtk::TreeView *event_list_view, CallbackMap *callback_connections)
{
    _priv->addDialogConnection(event_list_view, callback_connections, _event_list_store, _curr_event);
}

void EventLog::removeDialogConnection(Gtk::TreeView *event_list_view, CallbackMap *callback_connections)
{
    _priv->removeDialogConnection(event_list_view, callback_connections);
}

void
EventLog::updateUndoVerbs()
{
    if(_document) {
        auto &_columns = getColumns();

        if(_getUndoEvent()) { 
            Inkscape::Verb::get(SP_VERB_EDIT_UNDO)->sensitive(_document, true);

            Inkscape::Verb::get(SP_VERB_EDIT_UNDO)->name(_document,
                      Glib::ustring(_("_Undo")) + ": " +
                      Glib::ustring((*_getUndoEvent())[_columns.description]));
        } else {
            Inkscape::Verb::get(SP_VERB_EDIT_UNDO)->name(_document, _("_Undo"));
            Inkscape::Verb::get(SP_VERB_EDIT_UNDO)->sensitive(_document, false);
        }

        if(_getRedoEvent()) {
            Inkscape::Verb::get(SP_VERB_EDIT_REDO)->sensitive(_document, true);
            Inkscape::Verb::get(SP_VERB_EDIT_REDO)->name(_document,
                      Glib::ustring(_("_Redo")) + ": " +
                      Glib::ustring((*_getRedoEvent())[_columns.description]));

        } else {
            Inkscape::Verb::get(SP_VERB_EDIT_REDO)->name(_document, _("_Redo"));
            Inkscape::Verb::get(SP_VERB_EDIT_REDO)->sensitive(_document, false);
        }

    }

}


EventLog::const_iterator
EventLog::_getUndoEvent() const
{
    const_iterator undo_event = (const_iterator)nullptr;
    if( _curr_event != _event_list_store->children().begin() )
        undo_event = _curr_event;
    return undo_event;
}

EventLog::const_iterator
EventLog::_getRedoEvent() const
{
    const_iterator redo_event = (const_iterator)nullptr;

    if ( _curr_event != _last_event ) {

        if ( !_curr_event->children().empty() )
            redo_event = _curr_event->children().begin();
        else  {
            redo_event = _curr_event;
            ++redo_event;

            if ( redo_event->parent() &&
                 redo_event == redo_event->parent()->children().end() ) {

                redo_event = redo_event->parent();
                ++redo_event;

            }
        }

    }

    return redo_event;
}

void
EventLog::_clearUndo()
{
    // TODO: Implement when needed
}

void
EventLog::_clearRedo()
{
    if ( _last_event != _curr_event ) {
        auto &_columns = getColumns();

        _last_event = _curr_event;

        if ( !_last_event->children().empty() ) {
            _last_event = _last_event->children().begin();
        } else {
            ++_last_event;
        }

        while ( _last_event != _event_list_store->children().end() ) {

            if (_last_event->parent()) {
                while ( _last_event != _last_event->parent()->children().end() ) {
                    _last_event = _event_list_store->erase(_last_event);
                }
                _last_event = _last_event->parent();

                (*_last_event)[_columns.child_count] = _last_event->children().size() + 1;

                ++_last_event;
            } else {
                _last_event = _event_list_store->erase(_last_event);
            }

        }

    }
}

/* mark document as untouched if we reach a state where the document was previously saved */
void
EventLog::checkForVirginity() {
    g_return_if_fail (_document);
    if (_curr_event == _last_saved) {
        _document->setModifiedSinceSave(false);
    }
}

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
