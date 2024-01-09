// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LPE_FILL_BETWEEN_MANY_H
#define INKSCAPE_LPE_FILL_BETWEEN_MANY_H

/*
 * Inkscape::LPEFillBetweenStrokes
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/enum.h"
#include "live_effects/parameter/hidden.h"
#include "live_effects/parameter/patharray.h"

namespace Inkscape {
namespace LivePathEffect {

enum Filllpemethod {
    FLM_ORIGINALD,
    FLM_BSPLINESPIRO,
    FLM_D,
    FLM_END
};

class LPEFillBetweenMany : public Effect {
public:
    LPEFillBetweenMany(LivePathEffectObject *lpeobject);
    ~LPEFillBetweenMany() override;
    void doEffect (SPCurve * curve) override;
    bool doOnOpen(SPLPEItem const *lpeitem) override;
    void doBeforeEffect (SPLPEItem const* lpeitem) override;
    void doOnApply (SPLPEItem const* lpeitem) override;
    void transform_multiply_nested(Geom::Affine const &postmul);
private:
    PathArrayParam linked_paths;
    EnumParam<Filllpemethod> method;
    BoolParam join;
    BoolParam close;
    BoolParam autoreverse;
    bool legacytest = false;
    bool fixreverseend = false;
    Geom::Affine prevaffine = Geom::identity();
    Filllpemethod previous_method;
    LPEFillBetweenMany(const LPEFillBetweenMany&) = delete;
    LPEFillBetweenMany& operator=(const LPEFillBetweenMany&) = delete;
};

}; //namespace LivePathEffect
}; //namespace Inkscape

#endif
