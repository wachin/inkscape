// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Shape (styled path) belonging to an SVG drawing.
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm.h>
#include <2geom/curves.h>
#include <2geom/pathvector.h>
#include <2geom/path-sink.h>
#include <2geom/svg-path-parser.h>

#include "drawing-shape.h"

#include "preferences.h"
#include "style.h"

#include "display/cairo-utils.h"
#include "display/curve.h"
#include "display/drawing.h"
#include "display/drawing-context.h"
#include "display/drawing-group.h"
#include "display/control/canvas-item-drawing.h"

#include "helper/geom-curves.h"
#include "helper/geom.h"

#include "svg/svg.h"
#include "ui/widget/canvas.h" // Canvas area

namespace Inkscape {

DrawingShape::DrawingShape(Drawing &drawing)
    : DrawingItem(drawing)
    , _curve(nullptr)
    , _last_pick(nullptr)
    , _repick_after(0)
{}

DrawingShape::~DrawingShape()
{
}

void
DrawingShape::setPath(SPCurve *curve)
{
    _markForRendering();

    _curve = curve ? curve->ref() : nullptr;

    _markForUpdate(STATE_ALL, false);
}

void
DrawingShape::setStyle(SPStyle *style, SPStyle *context_style)
{
    DrawingItem::setStyle(style, context_style); // Must be first
    _nrstyle.set(_style, _context_style);
}

void
DrawingShape::setChildrenStyle(SPStyle* context_style)
{
    DrawingItem::setChildrenStyle( context_style );
    _nrstyle.set(_style, _context_style);
}

unsigned
DrawingShape::_updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    Geom::OptRect boundingbox;

    unsigned beststate = STATE_ALL;

    // update markers
    for (auto & i : _children) {
        i.update(area, ctx, flags, reset);
    }

    if (!(flags & STATE_RENDER)) {
        /* We do not have to create rendering structures */
        if (flags & STATE_BBOX) {
            if (_curve) {
                boundingbox = bounds_exact_transformed(_curve->get_pathvector(), ctx.ctm);
                if (boundingbox) {
                    _bbox = boundingbox->roundOutwards();
                } else {
                    _bbox = Geom::OptIntRect();
                }
            }
            if (beststate & STATE_BBOX) {
                for (auto & i : _children) {
                    _bbox.unionWith(i.geometricBounds());
                }
            }
        }
        return (flags | _state);
    }

    boundingbox = Geom::OptRect();
    bool outline = _drawing.outline();

    // clear Cairo data to force update
    _nrstyle.update();

    if (_curve) {
        boundingbox = bounds_exact_transformed(_curve->get_pathvector(), ctx.ctm);

        if (boundingbox && (_nrstyle.stroke.type != NRStyle::PAINT_NONE || outline)) {
            float width, scale;
            scale = ctx.ctm.descrim();
            width = std::max(0.125f, _nrstyle.stroke_width * scale);
            if ( fabs(_nrstyle.stroke_width * scale) > 0.01 ) { // FIXME: this is always true
                boundingbox->expandBy(width);
            }
            // those pesky miters, now
            float miterMax = width * _nrstyle.miter_limit;
            if ( miterMax > 0.01 ) {
                // grunt mode. we should compute the various miters instead
                // (one for each point on the curve)
                boundingbox->expandBy(miterMax);
            }
        }
    }

    _bbox = boundingbox ? boundingbox->roundOutwards() : Geom::OptIntRect();

    if (!_curve || 
        !_style ||
        _curve->is_empty())
    {
        return STATE_ALL;
    }

    if (beststate & STATE_BBOX) {
        for (auto & i : _children) {
            _bbox.unionWith(i.geometricBounds());
        }
    }
    return STATE_ALL;
}

void
DrawingShape::_renderFill(DrawingContext &dc)
{
    Inkscape::DrawingContext::Save save(dc);
    dc.transform(_ctm);

    bool has_fill =  _nrstyle.prepareFill(dc, _item_bbox, _fill_pattern);

    if( has_fill ) {
        dc.path(_curve->get_pathvector());
        _nrstyle.applyFill(dc);
        dc.fillPreserve();
        dc.newPath(); // clear path
    }
}

void
DrawingShape::_renderStroke(DrawingContext &dc)
{
    Inkscape::DrawingContext::Save save(dc);
    dc.transform(_ctm);

    bool has_stroke = _nrstyle.prepareStroke(dc, _item_bbox, _stroke_pattern);
    if (!_style->stroke_extensions.hairline) {
        has_stroke &= (_nrstyle.stroke_width != 0);
    }

    if( has_stroke ) {
        // TODO: remove segments outside of bbox when no dashes present
        dc.path(_curve->get_pathvector());
        if (_style && _style->vector_effect.stroke) {
            dc.restore();
            dc.save();
        }
        _nrstyle.applyStroke(dc);

        // If the stroke is a hairline, set it to exactly 1px on screen.
        // If visible hairline mode is on, make sure the line is at least 1px.
        if (_drawing.visibleHairlines() || _style->stroke_extensions.hairline) {
            double pixel_size_x = 1.0, pixel_size_y = 1.0;
            dc.device_to_user_distance(pixel_size_x, pixel_size_y);
            if (_style->stroke_extensions.hairline || _nrstyle.stroke_width < std::min(pixel_size_x, pixel_size_y)) {
                dc.setHairline();
            }
        }

        dc.strokePreserve();
        dc.newPath(); // clear path
    }
}

void
DrawingShape::_renderMarkers(DrawingContext &dc, Geom::IntRect const &area, unsigned flags, DrawingItem *stop_at)
{
    // marker rendering
    for (auto & i : _children) {
        i.render(dc, area, flags, stop_at);
    }
}

unsigned
DrawingShape::_renderItem(DrawingContext &dc, Geom::IntRect const &area, unsigned flags, DrawingItem *stop_at)
{
    if (!_curve || !_style) return RENDER_OK;
    if (!area.intersects(_bbox)) return RENDER_OK; // skip if not within bounding box

    bool outline = _drawing.outline();

    if (outline) {
        guint32 rgba = _drawing.outlinecolor;

        // paint-order doesn't matter
        {   Inkscape::DrawingContext::Save save(dc);
            dc.transform(_ctm);
            dc.path(_curve->get_pathvector());
        }
        {   Inkscape::DrawingContext::Save save(dc);
            dc.setSource(rgba);
            dc.setLineWidth(0.5);
            dc.setTolerance(0.5);
            dc.stroke();
        }

        _renderMarkers(dc, area, flags, stop_at);
        return RENDER_OK;

    }

    if( _nrstyle.paint_order_layer[0] == NRStyle::PAINT_ORDER_NORMAL ) {
        // This is the most common case, special case so we don't call get_pathvector(), etc. twice

        {
            // we assume the context has no path
            Inkscape::DrawingContext::Save save(dc);
            dc.transform(_ctm);


            // update fill and stroke paints.
            // this cannot be done during nr_arena_shape_update, because we need a Cairo context
            // to render svg:pattern
            bool has_fill   = _nrstyle.prepareFill(dc, _item_bbox, _fill_pattern);
            bool has_stroke = _nrstyle.prepareStroke(dc, _item_bbox, _stroke_pattern);
            has_stroke &= (_nrstyle.stroke_width != 0 || _nrstyle.hairline == true);
            if (has_fill || has_stroke) {
                dc.path(_curve->get_pathvector());
                // TODO: remove segments outside of bbox when no dashes present
                if (has_fill) {
                    _nrstyle.applyFill(dc);
                    dc.fillPreserve();
                }
                if (_style && _style->vector_effect.stroke) {
                    dc.restore();
                    dc.save();
                }
                if (has_stroke) {
                    _nrstyle.applyStroke(dc);

                    // If the draw mode is set to visible hairlines, don't let anything get smaller
                    // than half a pixel.
                    if (_drawing.visibleHairlines()) {
                        double half_pixel_size = 0.5, trash = 0.5;
                        dc.device_to_user_distance(half_pixel_size, trash);
                        if (_nrstyle.stroke_width < half_pixel_size) {
                            dc.setLineWidth(half_pixel_size);
                        }
                    }

                    dc.strokePreserve();
                }
                dc.newPath(); // clear path
            } // has fill or stroke pattern
        }
        _renderMarkers(dc, area, flags, stop_at);
        return RENDER_OK;

    }

    // Handle different paint orders
    for (auto & i : _nrstyle.paint_order_layer) {
        switch (i) {
            case NRStyle::PAINT_ORDER_FILL:
                _renderFill(dc);
                break;
            case NRStyle::PAINT_ORDER_STROKE:
                _renderStroke(dc);
                break;
            case NRStyle::PAINT_ORDER_MARKER:
                _renderMarkers(dc, area, flags, stop_at);
                break;
            default:
                // PAINT_ORDER_AUTO Should not happen
                break;
        }
    }
    return RENDER_OK;
}

void DrawingShape::_clipItem(DrawingContext &dc, Geom::IntRect const & /*area*/)
{
    if (!_curve) return;

    Inkscape::DrawingContext::Save save(dc);
    // handle clip-rule
    if (_style) {
        if (_style->clip_rule.computed == SP_WIND_RULE_EVENODD) {
            dc.setFillRule(CAIRO_FILL_RULE_EVEN_ODD);
        } else {
            dc.setFillRule(CAIRO_FILL_RULE_WINDING);
        }
    }
    dc.transform(_ctm);
    dc.path(_curve->get_pathvector());
    dc.fill();
}

DrawingItem *
DrawingShape::_pickItem(Geom::Point const &p, double delta, unsigned flags)
{
    if (_repick_after > 0)
        --_repick_after;

    if (_repick_after > 0) { // we are a slow, huge path
        return _last_pick;   // skip this pick, returning what was returned last time
    }

    if (!_curve) return nullptr;
    if (!_style) return nullptr;
    bool outline = _drawing.outline() || _drawing.outlineOverlay() || _drawing.getOutlineSensitive();
    bool pick_as_clip = flags & PICK_AS_CLIP;

    if (SP_SCALE24_TO_FLOAT(_style->opacity.value) == 0 && !outline && !pick_as_clip) {
        // fully transparent, no pick unless outline mode
        return nullptr;
    }

    gint64 tstart = g_get_monotonic_time();

    double width;
    if (pick_as_clip) {
        width = 0; // no width should be applied to clip picking
                   // this overrides display mode and stroke style considerations
    } else if (outline) {
        width = 0.5; // in outline mode, everything is stroked with the same 0.5px line width
    } else if (_nrstyle.stroke.type != NRStyle::PAINT_NONE && _nrstyle.stroke.opacity > 1e-3) {
        // for normal picking calculate the distance corresponding top the stroke width
        // FIXME BUG: this is incorrect for transformed strokes
        float const scale = _ctm.descrim();
        width = std::max(0.125f, _nrstyle.stroke_width * scale) / 2;
    } else {
        width = 0;
    }

    double dist = Geom::infinity();
    int wind = 0;
    bool needfill = pick_as_clip || (_nrstyle.fill.type != NRStyle::PAINT_NONE &&
        _nrstyle.fill.opacity > 1e-3 && !outline);
    bool wind_evenodd = pick_as_clip ? (_style->clip_rule.computed == SP_WIND_RULE_EVENODD) :
        (_style->fill_rule.computed == SP_WIND_RULE_EVENODD);

    // actual shape picking
    if (_drawing.getCanvasItemDrawing()) {
        Geom::Rect viewbox = _drawing.getCanvasItemDrawing()->get_canvas()->get_area_world();
        viewbox.expandBy (width);
        pathv_matrix_point_bbox_wind_distance(_curve->get_pathvector(), _ctm, p, nullptr, needfill? &wind : nullptr, &dist, 0.5, &viewbox);
    } else {
        pathv_matrix_point_bbox_wind_distance(_curve->get_pathvector(), _ctm, p, nullptr, needfill? &wind : nullptr, &dist, 0.5, nullptr);
    }

    gint64 tfinish = g_get_monotonic_time();
    gint64 this_pick = tfinish - tstart;
    //g_print ("pick time %lu\n", this_pick);
    if (this_pick > 10000) { // slow picking, remember to skip several new picks
        _repick_after = this_pick / 5000;
    }

    // covered by fill?
    if (needfill) {
        if (wind_evenodd) {
            if (wind & 0x1) {
                _last_pick = this;
                return this;
            }
        } else {
            if (wind != 0) {
                _last_pick = this;
                return this;
            }
        }
    }

    // close to the edge, as defined by strokewidth and delta?
    // this ignores dashing (as if the stroke is solid) and always works as if caps are round
    if (needfill || width > 0) { // if either fill or stroke visible,
        if ((dist - width) < delta) {
            _last_pick = this;
            return this;
        }
    }

    // if not picked on the shape itself, try its markers
    for (auto & i : _children) {
        DrawingItem *ret = i.pick(p, delta, flags & ~PICK_STICKY);
        if (ret) {
            _last_pick = this;
            return this;
        }
    }

    _last_pick = nullptr;
    return nullptr;
}

bool
DrawingShape::_canClip()
{
    return true;
}

} // end namespace Inkscape

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
