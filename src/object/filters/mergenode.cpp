// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * feMergeNode implementation. A feMergeNode contains the name of one
 * input image for feMerge.
 */
/*
 * Authors:
 *   Kees Cook <kees@outflux.net>
 *   Niko Kiirala <niko@kiirala.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2004,2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "mergenode.h"
#include "merge.h"
#include "object/sp-filter.h"

#include "attributes.h"
#include "xml/repr.h"
#include "slot-resolver.h"
#include "util/optstr.h"

void SPFeMergeNode::build(SPDocument */*document*/, Inkscape::XML::Node */*repr*/)
{
    readAttr(SPAttr::IN_);
}

void SPFeMergeNode::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::IN_:
            if (Inkscape::Util::assign(in_name, value)) {
                requestModified(SP_OBJECT_MODIFIED_FLAG);
                invalidate_parent_slots();
            }
            break;
        default:
            SPObject::set(key, value);
            break;
    }
}

void SPFeMergeNode::invalidate_parent_slots()
{
    if (auto merge = cast<SPFeMerge>(parent)) {
        merge->invalidate_parent_slots();
    }
}

void SPFeMergeNode::resolve_slots(SlotResolver const &resolver)
{
    in_slot = resolver.read(in_name);
}

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
