// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "live_effects/lpe-bounding-box.h"

#include "display/curve.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>


namespace Inkscape {
namespace LivePathEffect {

LPEBoundingBox::LPEBoundingBox(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    linked_path(_("Linked path:"), _("Path from which to take the original path data"), "linkedpath", &wr, this),
    visual_bounds(_("Visual Bounds"), _("Uses the visual bounding box"), "visualbounds", &wr, this)
{
    registerParameter(&linked_path);
    registerParameter(&visual_bounds);
    //perceived_path = true;
    linked_path.setUpdating(true);
    linked_path.lookup = true;
}

LPEBoundingBox::~LPEBoundingBox() = default;

bool 
LPEBoundingBox::doOnOpen(SPLPEItem const *lpeitem)
{
    if (!is_load || is_applied) {
        return false;
    }
    linked_path.setUpdating(false);
    linked_path.start_listening(linked_path.getObject());
    linked_path.connect_selection_changed();
    return false;
}

void 
LPEBoundingBox::doOnApply(SPLPEItem const *lpeitem)
{
    lpeversion.param_setValue("1.3", true);
}

void 
LPEBoundingBox::doBeforeEffect (SPLPEItem const* lpeitem)
{
    if (is_load) {
        linked_path.setUpdating(false);
        linked_path.start_listening(linked_path.getObject());
        linked_path.connect_selection_changed();
        SPItem * item = nullptr;
        if (( item = cast<SPItem>(linked_path.getObject()) )) {
            item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        }
    }
}


void LPEBoundingBox::doEffect (SPCurve * curve)
{
    
    if (curve) {
        if ( linked_path.linksToItem() && linked_path.getObject() ) {
            auto item = cast<SPItem>(linked_path.getObject());
            Glib::ustring version = lpeversion.param_getSVGValue();
            Geom::OptRect bbox;
            if (version >= "1.3") {
                auto trans = item->getRelativeTransform(sp_lpe_item);
                bbox = visual_bounds.get_value() ? item->visualBounds(trans) : item->geometricBounds(trans);
            } else {
                bbox = visual_bounds.get_value() ? item->visualBounds() : item->geometricBounds();
            }
            Geom::Path p;
            Geom::PathVector out;
            if (bbox) {
                p = Geom::Path(*bbox);
                out.push_back(p);
            }
            curve->set_pathvector(out);
        }
    }
}

} // namespace LivePathEffect
} /* namespace Inkscape */

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
