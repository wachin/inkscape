// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Path utilities.
 *//*
 * Authors:
 * see git history
 *  Created by fred on Fri Dec 05 2003.
 *  tweaked endlessly by bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
#endif

#include <vector>

#include "path-util.h"

#include "text-editing.h"

#include "livarot/Path.h"
#include "livarot/Shape.h"

#include "object/sp-flowtext.h"
#include "object/sp-image.h"
#include "object/sp-marker.h"
#include "object/sp-path.h"
#include "object/sp-text.h"

#include "display/curve.h"

Path *
Path_for_pathvector(Geom::PathVector const &epathv)
{
    /*std::cout << "converting to Livarot path" << std::endl;

    Geom::SVGPathWriter wr;
    wr.feed(epathv);
    std::cout << wr.str() << std::endl;*/

    Path *dest = new Path;
    dest->LoadPathVector(epathv);
    return dest;
}

Path *
Path_for_item(SPItem *item, bool doTransformation, bool transformFull)
{
    auto curve = curve_for_item(item);

    if (!curve) {
        return nullptr;
    }

    Geom::PathVector *pathv =
        pathvector_for_curve(item, &*curve, doTransformation, transformFull, Geom::identity(), Geom::identity());

    /*std::cout << "converting to Livarot path" << std::endl;

    Geom::SVGPathWriter wr;
    if (pathv) {
        wr.feed(*pathv);
    }
    std::cout << wr.str() << std::endl;*/

    Path *dest = new Path;
    dest->LoadPathVector(*pathv);    
    delete pathv;

    /*gchar *str = dest->svg_dump_path();
    std::cout << "After conversion:\n" << str << std::endl;
    g_free(str);*/

    return dest;
}

Path *
Path_for_item_before_LPE(SPItem *item, bool doTransformation, bool transformFull)
{
    auto curve = curve_for_item_before_LPE(item);

    if (!curve) {
        return nullptr;
    }
    
    Geom::PathVector *pathv =
        pathvector_for_curve(item, &*curve, doTransformation, transformFull, Geom::identity(), Geom::identity());
    
    Path *dest = new Path;
    dest->LoadPathVector(*pathv);
    delete pathv;

    return dest;
}

Geom::PathVector*
pathvector_for_curve(SPItem *item, SPCurve *curve, bool doTransformation, bool transformFull, Geom::Affine extraPreAffine, Geom::Affine extraPostAffine)
{
    if (curve == nullptr)
        return nullptr;

    Geom::PathVector *dest = new Geom::PathVector;    
    *dest = curve->get_pathvector(); // Make a copy; must be freed by the caller!
    
    if (doTransformation) {
        if (transformFull) {
            *dest *= extraPreAffine * item->i2doc_affine() * extraPostAffine;
        } else {
            *dest *= extraPreAffine * (Geom::Affine)item->transform * extraPostAffine;
        }
    } else {
        *dest *= extraPreAffine * extraPostAffine;
    }

    return dest;
}

std::optional<SPCurve> curve_for_item(SPItem *item)
{
    if (!item) {
        return {};
    }
    
    if (auto path = cast<SPPath>(item)) {
        return SPCurve::ptr_to_opt(path->curveForEdit());
    } else if (auto shape = cast<SPShape>(item)) {
        return SPCurve::ptr_to_opt(shape->curve());
    } else if (is<SPText>(item) || is<SPFlowtext>(item)) {
        return te_get_layout(item)->convertToCurves();
    } else if (auto image = cast<SPImage>(item)) {
        return SPCurve::ptr_to_opt(image->get_curve());
    }
    
    return {};
}

std::optional<SPCurve> curve_for_item_before_LPE(SPItem *item)
{
    if (!item) {
        return {};
    }

    if (auto shape = cast<SPShape>(item)) {
        return SPCurve::ptr_to_opt(shape->curveForEdit());
    } else if (is<SPText>(item) || is<SPFlowtext>(item)) {
        return te_get_layout(item)->convertToCurves();
    } else if (auto image = cast<SPImage>(item)) {
        return SPCurve::ptr_to_opt(image->get_curve());
    }
    
    return {};
}

std::optional<Path::cut_position> get_nearest_position_on_Path(Path *path, Geom::Point p, unsigned seg)
{
    std::optional<Path::cut_position> result;
    if (!path) {
        return result; // returns empty std::optional
    }
    //get nearest position on path
    result = path->PointToCurvilignPosition(p, seg);
    return result;
}

Geom::Point get_point_on_Path(Path *path, int piece, double t)
{
    Geom::Point p;
    path->PointAt(piece, t, p);
    return p;
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
