// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * ActionAccel class implementation
 *
 * Authors:
 *   Rafael Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2022 the Authors.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gdk/gdk.h>
#include <glibmm/ustring.h>
#include <gtkmm.h>
#include <set>
#include <sigc++/sigc++.h>

#include "inkscape-application.h"
#include "ui/shortcuts.h"
#include "util/action-accel.h"

namespace Inkscape {
namespace Util {

ActionAccel::ActionAccel(Glib::ustring action_name)
    : _action{std::move(action_name)}
{
    auto &shortcuts = Shortcuts::getInstance();
    _query();
    _prefs_changed = shortcuts.connect_changed([this]() { _onShortcutsModified(); });
}

ActionAccel::~ActionAccel()
{
    _prefs_changed.disconnect();
}

void ActionAccel::_onShortcutsModified()
{
    if (_query()) {
        _we_changed.emit();
    }
}

bool ActionAccel::_query()
{
    auto *app = InkscapeApplication::instance();
    if (!app) {
        g_warn_message("Inkscape", __FILE__, __LINE__, __func__,
                       "Attempt to read keyboard shortcuts while running without an InkscapeApplication!");
        return false;
    }
    auto *gtk_app = app->gtk_app();
    if (!gtk_app) {
        g_warn_message("Inkscape", __FILE__, __LINE__, __func__,
                       "Attempt to read keyboard shortcuts while running without a GUI!");
        return false;
    }
    auto accel_strings = gtk_app->get_accels_for_action(_action);
    std::set<AcceleratorKey> new_keys;
    for (auto &&name : accel_strings) {
        new_keys.emplace(std::move(name));
    }

    if (new_keys != _accels)
    {
        _accels = std::move(new_keys);
        return true;
    }
    return false;
}

bool ActionAccel::isTriggeredBy(GdkEventKey *key) const
{
    auto &shortcuts = Shortcuts::getInstance();
    AcceleratorKey accelerator = shortcuts.get_from_event(key);
    return _accels.find(accelerator) != _accels.end();
}

} // namespace Util
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
