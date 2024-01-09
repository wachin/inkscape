// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Header file that defines the singleton DocumentFonts class.
 * This is a singleton class.
 *
 * Authors:
 *   Vaibhav Malik <vaibhavmalik2018@gmail.com>
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INK_DOCUMENT_FONTS_H
#define INK_DOCUMENT_FONTS_H

#include <vector>
#include <map>
#include <set>
#include <sigc++/sigc++.h>

#include "io/resource.h"
#include "io/dir-util.h"

namespace Inkscape {

class DocumentFonts {

public:
    enum What {
        All,
        System,
        User
    };

    static DocumentFonts* get();
    ~DocumentFonts() = default;

    void clear();
    // void print_document_fonts();
    void update_document_fonts(const std::map<Glib::ustring, std::set<Glib::ustring>>& font_data);
    const std::set <Glib::ustring> get_fonts();

    // Signals
    sigc::connection connectUpdate(sigc::slot <void ()> slot) {
        return update_signal.connect(slot);
    }

private:
    DocumentFonts();
    std::set <Glib::ustring> _document_fonts;

    // Signals
    sigc::signal <void ()> update_signal;
};

} // Namespace Inkscape

#endif // INK_DOCUMENT_FONTS_H

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
