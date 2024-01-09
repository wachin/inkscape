// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Bitmap tracing settings dialog
 */
/* Authors:
 *   Bob Jamison
 *   Others - see git history.
 *
 * Copyright (C) 2004-2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_TRACE_H
#define INKSCAPE_UI_DIALOG_TRACE_H

#include <memory>
#include "ui/dialog/dialog-base.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

class TraceDialog : public DialogBase
{
public:
    static std::unique_ptr<TraceDialog> create();

protected:
    TraceDialog() : DialogBase("/dialogs/trace", "Trace") {}
};

} //namespace Dialog
} //namespace UI
} //namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_TRACE_H

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
