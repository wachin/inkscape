// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for selection tied to the application and without GUI.
 *
 * Copyright (C) 2018 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_ACTIONS_HELPER_H
#define INK_ACTIONS_HELPER_H

#include <glibmm/ustring.h>

class InkscapeApplication;
class SPDocument;
namespace Inkscape {
  class Selection;
}

void active_window_start_helper();
void active_window_end_helper();
void show_output(Glib::ustring data, bool is_cerr = true);
bool get_document_and_selection(InkscapeApplication* app, SPDocument** document, Inkscape::Selection** selection);


#endif // INK_ACTIONS_HELPER_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
