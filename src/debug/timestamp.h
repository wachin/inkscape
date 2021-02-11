// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::Debug::timestamp - timestamp strings
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2007 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DEBUG_TIMESTAMP_H
#define SEEN_INKSCAPE_DEBUG_TIMESTAMP_H

#include <memory>
#include <string>

namespace Inkscape {

namespace Debug {

std::shared_ptr<std::string> timestamp();

}

}

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
