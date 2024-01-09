// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feMerge> implementation.
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "attributes.h"
#include "svg/svg.h"
#include "xml/repr.h"

#include "merge.h"
#include "mergenode.h"
#include "display/nr-filter.h"
#include "display/nr-filter-merge.h"

void SPFeMerge::modified(unsigned flags)
{
    auto const cflags = cascade_flags(flags);

    for (auto &c : children) {
        if (cflags || (c.mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            c.emitModified(cflags);
        }
    }
}

void SPFeMerge::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref)
{
    SPFilterPrimitive::child_added(child, ref);
    requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFeMerge::remove_child(Inkscape::XML::Node *child)
{
    SPFilterPrimitive::remove_child(child);
    requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFeMerge::order_changed(Inkscape::XML::Node *child, Inkscape::XML::Node *old_ref, Inkscape::XML::Node *new_ref)
{
    SPFilterPrimitive::order_changed(child, old_ref, new_ref);
    requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFeMerge::resolve_slots(SlotResolver &resolver)
{
    for (auto &input : children) {
        if (auto node = cast<SPFeMergeNode>(&input)) {
            node->resolve_slots(std::as_const(resolver));
        }
    }
    SPFilterPrimitive::resolve_slots(resolver);
}

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeMerge::build_renderer(Inkscape::DrawingItem*) const
{
    auto merge = std::make_unique<Inkscape::Filters::FilterMerge>();
    build_renderer_common(merge.get());

    int in_nr = 0;

    for (auto const &input : children) {
        if (auto node = cast<SPFeMergeNode>(&input)) {
            merge->set_input(in_nr, node->get_in());
            in_nr++;
        }
    }

    return merge;
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
