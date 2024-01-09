// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Action Accel
 * A simple tracker for accelerator keys associated to an action
 *
 * Authors:
 *   Rafael Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2022 the Authors.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef ACTION_ACCEL_H_SEEN
#define ACTION_ACCEL_H_SEEN

#include <gdk/gdk.h>
#include <glibmm/ustring.h>
#include <gtkmm.h>
#include <set>
#include <sigc++/sigc++.h>
#include <vector>

namespace Inkscape {
namespace Util {

/** Gtk::AccelKey but with equality and less-than operators */
class AcceleratorKey : public Gtk::AccelKey
{
public:
    bool operator==(AcceleratorKey const &other) const
    {
        return (get_key() == other.get_key()) && (get_mod() == other.get_mod());
    }

    bool operator<(AcceleratorKey const &other) const
    {
        return (get_key() < other.get_key()) || (get_key() == other.get_key()
                                                 && get_mod() < other.get_mod());
    }

    AcceleratorKey(Gtk::AccelKey const &ak) : Gtk::AccelKey{ak} {};
};

/**
 * \brief The ActionAccel class stores the keyboard shortcuts for a given action
 * and automatically keeps track of changes in the keybindings.
 *
 * Additionally, a signal is emitted when the keybindings for the action change.
 *
 * In order to create an ActionAccel object, one must pass a Glib::ustring containing the
 * action name to the constructor. The object will automatically observe the
 * keybindings for that action, so you always get up-to-date keyboard shortcuts.
 * To check if a given key event triggers one of these keybindings, use `isTriggeredBy()`.
 *
 * Typical usage example:
 * \code{.cpp}
    auto accel = Inkscape::Util::ActionAccel("doc.undo");
    GdkEventKey *key = get_from_somewhere();
    if (accel.isTriggeredBy(key)) {
        ... // do stuff
    }
    accel.connectModified( []() { // This code will run when the user changes
                                  // the keybindings for this action.
                                } );
   \endcode
 */
class ActionAccel
{
private:
    sigc::signal<void ()> _we_changed;   ///< Emitted when the keybindings for the action are changed
    sigc::connection _prefs_changed;  ///< To listen to changes to the keyboard shortcuts
    Glib::ustring _action;            ///< Name of the action
    std::set<AcceleratorKey> _accels; ///< Stores the accelerator keys for the action

    /** Queries and updates the stored shortcuts, returning true if they have changed. */
    bool _query();

    /** Runs when the keyboard shortcut settings have changed */
    void _onShortcutsModified();

public:
    /**
     * @brief Construct an ActionAccel object which will keep track of keybindings for a given action.
     * @param action_name - the name of the action to hold and observe the keybindings of.
     */
    ActionAccel(Glib::ustring action_name);

    ~ActionAccel();

    /**
     * @brief Returns all keyboard shortcuts for the action.
     * @return a vector containing a Gtk::AccelKey for each of the keybindings present for the action.
     */
    std::vector<Gtk::AccelKey> getKeys() const
    {
        return std::vector<Gtk::AccelKey>(_accels.begin(), _accels.end());
    }

    /**
     * @brief Connects a void callback which will run whenever the keybindings for the action change.
     *        At the time when the callback runs, the values stored in the ActionAccel object will have
     *        already been updated. This means that the new keybindings can be queried by the callback.
     * @param slot - the sigc::slot representing the callback function.
     * @return the resulting sigc::connection.
     */
    sigc::connection connectModified(sigc::slot<void ()> const &slot) { return _we_changed.connect(slot); }

    /**
     * @brief Checks whether a given key event triggers this action.
     * @param key - a pointer to a GdkEventKey struct containing key event data.
     * @return true if one of the keyboard shortcuts for the action is triggered by the passed event,
     *         false otherwise.
     */
    bool isTriggeredBy(GdkEventKey *key) const;
};

} // namespace Util
} // namespace Inkscape

#endif // ACTION_ACCEL_H_SEEN
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
