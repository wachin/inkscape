// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SubItem controls each fractured piece and links it to its original items.
 *
 *//*
 * Authors:
 *   Martin Owens
 *   PBS
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <numeric>
#include <utility>
#include <random>

#include <boost/range/adaptor/reversed.hpp>

#include "booleans-subitems.h"
#include "helper/geom-pathstroke.h"
#include "livarot/LivarotDefs.h"
#include "livarot/Shape.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"
#include "object/sp-use.h"
#include "object/sp-image.h"
#include "path/path-boolop.h"
#include "style.h"

namespace Inkscape {

// Todo: (Wishlist) Remove this function when no longer necessary to remove boolops artifacts.
static Geom::PathVector clean_pathvector(Geom::PathVector &&pathv)
{
    Geom::PathVector result;

    for (auto &path : pathv) {
        if (path.closed() && !is_path_empty(path)) {
            result.push_back(std::move(path));
        }
    }

    return result;
}

/**
 * Union operator, merges two subitems when requested by the user
 * The left hand side will retain priority for the resulting style
 * so you should be mindful of how you merge these shapes.
 */
SubItem &SubItem::operator+=(SubItem const &other)
{
    _paths = sp_pathvector_boolop(_paths, other._paths, bool_op_union, fill_nonZero, fill_nonZero, true);
    sp_flatten(_paths, fill_nonZero);
    _paths = clean_pathvector(std::move(_paths));
    return *this;
}

using ExtractPathvectorsResult = std::vector<std::pair<Geom::PathVector, SPStyle*>>;

static void extract_pathvectors_recursive(SPItem *item, ExtractPathvectorsResult &result, Geom::Affine const &transform)
{
    if (is<SPGroup>(item)) {
        for (auto &child : item->children | boost::adaptors::reversed) {
            if (auto child_item = cast<SPItem>(&child)) {
                extract_pathvectors_recursive(child_item, result, child_item->transform * transform);
            }
        }
    } else if (auto img = cast<SPImage>(item)) {
        result.emplace_back(img->get_curve()->get_pathvector() * transform, item->style);
    } else if (auto shape = cast<SPShape>(item)) {
        if (auto curve = shape->curve()) {
            result.emplace_back(curve->get_pathvector() * transform, item->style);
        }
    } else if (auto text = cast<SPText>(item)) {
        result.emplace_back(text->getNormalizedBpath().get_pathvector() * transform, item->style);
    } else if (auto use = cast<SPUse>(item)) {
        if (use->child) {
            extract_pathvectors_recursive(use->child, result, use->child->transform * Geom::Translate(use->x.computed, use->y.computed) * transform);
        }
    }
}

// Return all pathvectors found within an item, along with their styles, sorted top-to-bottom.
static ExtractPathvectorsResult extract_pathvectors(SPItem *item)
{
    ExtractPathvectorsResult result;
    extract_pathvectors_recursive(item, result, item->i2dt_affine());
    return result;
}

static FillRule sp_to_livarot(SPWindRule fillrule)
{
    return fillrule == SP_WIND_RULE_NONZERO ? fill_nonZero : fill_oddEven;
}

static double diameter(Geom::PathVector const &path)
{
    auto rect = path.boundsExact();
    if (!rect) {
        return 1;
    }
    return std::hypot(rect->width(), rect->height());
}

// Cut the given pathvector along the lines into several smaller pathvectors.
static std::vector<Geom::PathVector> improved_cut(Geom::PathVector const &pathv, Geom::PathVector const &lines)
{
    Path patha;
    patha.LoadPathVector(pathv);
    patha.ConvertWithBackData(diameter(pathv) * 1e-3);

    Path pathb;
    pathb.LoadPathVector(lines);
    pathb.ConvertWithBackData(diameter(lines) * 1e-3);

    Shape shapea;
    {
        Shape tmp;
        patha.Fill(&tmp, 0);
        shapea.ConvertToShape(&tmp);
    }

    Shape shapeb;
    {
        Shape tmp;
        bool isline = pathb.pts.size() == 2 && pathb.pts[0].isMoveTo && !pathb.pts[1].isMoveTo;
        pathb.Fill(&tmp, 1, false, isline);
        shapeb.ConvertToShape(&tmp, fill_justDont);
    }

    Shape shape;
    shape.Booleen(&shapeb, &shapea, bool_op_cut, 1);

    Path path;
    int num_nesting = 0;
    int *nesting = nullptr;
    int *conts = nullptr;
    {
        path.SetBackData(false);
        Path *paths[2] = { &patha, &pathb };
        shape.ConvertToFormeNested(&path, 2, paths, 1, num_nesting, nesting, conts, false, true);
    }

    int num_paths;
    auto paths = path.SubPathsWithNesting(num_paths, false, num_nesting, nesting, conts);

    std::vector<Geom::PathVector> result;

    for (int i = 0; i < num_paths; i++) {
        result.emplace_back(paths[i]->MakePathVector());
    }

    g_free(paths);
    g_free(conts);
    g_free(nesting);

    return result;
}

/**
 * Take a list of items and fracture into a list of SubItems ready for
 * use inside the booleans interactive tool.
 */
WorkItems SubItem::build_mosaic(std::vector<SPItem*> &&items)
{
    // Sort so that topmost items come first.
    std::sort(items.begin(), items.end(), [] (auto a, auto b) {
        return sp_object_compare_position_bool(b, a);
    });

    // Extract all individual pathvectors within the collection of items,
    // keeping track of their associated item and style, again sorted topmost-first.
    using AugmentedItem = std::tuple<Geom::PathVector, SPItem*, SPStyle*>;
    std::vector<AugmentedItem> augmented;

    for (auto item : items) {
        // Get the correctly-transformed pathvectors, together with their corresponding styles.
        auto extracted = extract_pathvectors(item);

        // Append to the list of augmented items.
        for (auto &[pathv, style] : extracted) {
            augmented.emplace_back(std::move(pathv), item, style);
        }
    }

    // Compute a slightly expanded bounding box, collect together all lines, and cut the former by the latter.
    Geom::OptRect bounds;
    Geom::PathVector lines;

    for (auto &[pathv, item, style] : augmented) {
        bounds |= pathv.boundsExact();
        for (auto &path : pathv) {
            lines.push_back(path);
        }
    }

    if (!bounds) {
        return {};
    }

    constexpr double expansion = 10.0;
    bounds->expandBy(expansion);

    auto bounds_pathv = Geom::PathVector(Geom::Path(*bounds));
    auto pieces = improved_cut(bounds_pathv, lines);

    // Construct the SubItems, attempting to guess the corresponding augmented item for each piece.
    WorkItems result;

    auto gen = std::default_random_engine(std::random_device()());
    auto ranf = [&] { return std::uniform_real_distribution()(gen); };
    auto randpt = [&] { return Geom::Point(ranf(), ranf()); };

    for (auto &piece : pieces) {
        // Skip the big enclosing piece that is touching the outer boundary.
        if (auto rect = piece.boundsExact()) {
            if (   Geom::are_near(rect->top(), bounds->top(), expansion / 2)
                || Geom::are_near(rect->bottom(), bounds->bottom(), expansion / 2)
                || Geom::are_near(rect->left(), bounds->left(), expansion / 2)
                || Geom::are_near(rect->right(), bounds->right(), expansion / 2))
            {
                continue;
            }
        }

        // Remove junk paths that are open and/or tiny.
        for (auto it = piece.begin(); it != piece.end(); ) {
            if (!it->closed() || is_path_empty(*it)) {
                it = piece.erase(it);
            } else {
                ++it;
            }
        }

        // Skip empty pathvectors.
        if (piece.empty()) {
            continue;
        }

        // Determine the corresponding augmented item.
        // Fixme: (Wishlist) This is done unreliably and hackily, but livarot/2geom seemingly offer no alternative.
        std::unordered_map<AugmentedItem*, int> hits;

        auto rect = piece.boundsExact();

        auto add_hit = [&] (Geom::Point const &pt) {
            // Find an augmented item containing the point.
            for (auto &aug : augmented) {
                auto &[pathv, item, style] = aug;
                auto fill_rule = style->fill_rule.computed;
                auto winding = pathv.winding(pt);
                if (fill_rule == SP_WIND_RULE_NONZERO ? winding : winding % 2) {
                    hits[&aug]++;
                    return;
                }
            }

            // If none exists, register a background hit.
            hits[nullptr]++;
        };

        for (int total_hits = 0, patience = 1000; total_hits < 20 && patience > 0; patience--) {
            // Attempt to generate a point strictly inside the piece.
            auto pt = rect->min() + randpt() * rect->dimensions();
            if (piece.winding(pt)) {
                add_hit(pt);
                total_hits++;
            }
        }

        // Pick the augmented item with the most hits.
        AugmentedItem *found = nullptr;
        int max_hits = 0;

        for (auto &[a, h] : hits) {
            if (h > max_hits) {
                max_hits = h;
                found = a;
            }
        }

        // Add the SubItem.
        auto item = found ? std::get<1>(*found) : nullptr;
        auto style = found ? std::get<2>(*found) : nullptr;
        result.emplace_back(std::make_shared<SubItem>(std::move(piece), item, style));
    }

    return result;
}

/**
 * Take a list of items and flatten into a list of SubItems.
 */
WorkItems SubItem::build_flatten(std::vector<SPItem*> &&items)
{
    // Sort so that topmost items come first.
    std::sort(items.begin(), items.end(), [] (auto a, auto b) {
        return sp_object_compare_position_bool(b, a);
    });

    WorkItems result;
    Geom::PathVector unioned;

    for (auto item : items) {
        // Get the correctly-transformed pathvectors, together with their corresponding styles.
        auto extracted = extract_pathvectors(item);

        for (auto &[pathv, style] : extracted) {
            // Remove lines.
            for (auto it = pathv.begin(); it != pathv.end(); ) {
                if (!it->closed()) {
                    it = pathv.erase(it);
                } else {
                    ++it;
                }
            }

            // Skip pathvectors that are just lines.
            if (pathv.empty()) {
                continue;
            }

            // Flatten the remaining pathvector according to its fill rule.
            auto fillrule = style->fill_rule.computed;
            sp_flatten(pathv, sp_to_livarot(fillrule));

            // Remove the union so far from the shape, then add the shape to the union so far.
            Geom::PathVector uniq;

            if (unioned.empty()) {
                uniq = pathv;
                unioned = std::move(pathv);
            } else {
                uniq = sp_pathvector_boolop(unioned, pathv, bool_op_diff, fill_nonZero, fill_nonZero, true);
                unioned = sp_pathvector_boolop(unioned, pathv, bool_op_union, fill_nonZero, fill_nonZero, true);
            }

            // Add the new SubItem.
            result.emplace_back(std::make_shared<SubItem>(std::move(uniq), item, style));
        }
    }

    return result;
}

/**
 * Return true if this subitem contains the give point.
 */
bool SubItem::contains(Geom::Point const &pt) const
{
    return _paths.winding(pt) % 2 != 0;
}

} // namespace Inkscape
