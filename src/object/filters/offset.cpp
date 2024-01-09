// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feOffset> implementation.
 */
/*
 * Authors:
 *   hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Niko Kiirala <niko@kiirala.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006,2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/transforms.h>

#include "offset.h"
#include "attributes.h"

#include "display/nr-filter.h"
#include "display/nr-filter-offset.h"

#include "util/numeric/converters.h"
#include "svg/svg.h"
#include "xml/repr.h"

void SPFeOffset::build(SPDocument *document, Inkscape::XML::Node *repr)
{
	SPFilterPrimitive::build(document, repr);

    readAttr(SPAttr::DX);
    readAttr(SPAttr::DY);
}

void SPFeOffset::set(SPAttr key, char const *value)
{
    switch(key) {
        case SPAttr::DX: {
            double read_num = value ? Inkscape::Util::read_number(value) : 0.0;
            if (read_num != dx) {
                dx = read_num;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::DY:
        {
            double read_num = value ? Inkscape::Util::read_number(value) : 0.0;
            if (read_num != dy) {
                dy = read_num;
                requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        default:
        	SPFilterPrimitive::set(key, value);
            break;
    }
}

std::unique_ptr<Inkscape::Filters::FilterPrimitive> SPFeOffset::build_renderer(Inkscape::DrawingItem*) const
{
    auto offset = std::make_unique<Inkscape::Filters::FilterOffset>();
    build_renderer_common(offset.get());

    offset->set_dx(dx);
    offset->set_dy(dy);

    return offset;
}

/**
 * Calculate the region taken up by an offset
 *
 * @param region The original shape's region or previous primitive's region output.
 */
Geom::Rect SPFeOffset::calculate_region(Geom::Rect const &region) const
{
    // Because blur calculates its drawing space based on the resulting region.
    // An offset will actually harm blur's ability to draw, even though it shouldn't
    // A future fix would require the blur to figure out its region minus any downstream
    // offset (this affects drop-shadows).
    // TODO: region *= Geom::Translate(dx, dy);
    auto r = region;
    r.unionWith(r * Geom::Translate(dx, dy));
    return r;
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
