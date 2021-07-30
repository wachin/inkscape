// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_PATTERN_ALONG_PATH_H
#define INKSCAPE_LPE_PATTERN_ALONG_PATH_H

/*
 * Inkscape::LPEPatternAlongPath
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/enum.h"
#include "live_effects/effect.h"
#include "live_effects/parameter/path.h"
#include "live_effects/parameter/bool.h"
#include "live_effects/parameter/point.h"

namespace Inkscape {
namespace UI {
namespace Toolbar {
class PencilToolbar;
}
} // namespace UI
namespace LivePathEffect {

namespace WPAP {
class KnotHolderEntityWidthPatternAlongPath;
}

enum PAPCopyType {
    PAPCT_SINGLE = 0,
    PAPCT_SINGLE_STRETCHED,
    PAPCT_REPEATED,
    PAPCT_REPEATED_STRETCHED,
    PAPCT_END // This must be last
};

class LPEPatternAlongPath : public Effect {
public:
    LPEPatternAlongPath(LivePathEffectObject *lpeobject);
    ~LPEPatternAlongPath() override;

    void doBeforeEffect (SPLPEItem const* lpeitem) override;

    Geom::Piecewise<Geom::D2<Geom::SBasis> > doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in) override;

    void transform_multiply(Geom::Affine const &postmul, bool set) override;

    void addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec) override;
    
    void addKnotHolderEntities(KnotHolder * knotholder, SPItem * item) override;

    PathParam  pattern;
    friend class WPAP::KnotHolderEntityWidthPatternAlongPath;
    friend class Inkscape::UI::Toolbar::PencilToolbar;

protected:
    double original_height;
    ScalarParam prop_scale;
private:
    EnumParam<PAPCopyType> copytype;
    BoolParam scale_y_rel;
    ScalarParam  spacing;
    ScalarParam  normal_offset;
    ScalarParam  tang_offset;
    BoolParam    prop_units;
    BoolParam    vertical_pattern;
    BoolParam    hide_knot;
    ScalarParam  fuse_tolerance;
    KnotHolderEntity * _knot_entity;
    Geom::PathVector helper_path;
    void on_pattern_pasted();

    LPEPatternAlongPath(const LPEPatternAlongPath&);
    LPEPatternAlongPath& operator=(const LPEPatternAlongPath&);
};

}; //namespace LivePathEffect
}; //namespace Inkscape

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
