// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * LPE "Ellipse through 5 points" implementation.
 */
#ifndef SEEN_LPE_ELLIPSE_5PTS_H
#define SEEN_LPE_ELLIPSE_5PTS_H

/*
 * Authors:
 *   Theodore Janeczko
 *
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "message.h"

namespace Inkscape::LivePathEffect {

class LPEEllipse5Pts : public Effect
{
public:
    LPEEllipse5Pts(LivePathEffectObject *lpeobject);
    ~LPEEllipse5Pts() override { _clearWarning(); }

    Geom::PathVector doEffect_path(Geom::PathVector const &path_in) override;

private:
    LPEEllipse5Pts(LPEEllipse5Pts const &) = delete;
    LPEEllipse5Pts& operator=(LPEEllipse5Pts const &) = delete;

    void _flashWarning(char const *message);
    void _clearWarning();

    inline static MessageId const INVALID = 0x00'DEADBEEF'00;
    MessageId _error = INVALID;
    Geom::PathVector const _unit_circle;
};

} // namespace Inkscape::LivePathEffect

#endif // SEEN_LPE_ELLIPSE_5PTS_H

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
