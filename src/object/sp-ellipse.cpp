// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <ellipse> and related implementations
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Mitsuru Oka
 *   bulia byak <buliabyak@users.sf.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2013 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm.h>
#include <glibmm/i18n.h>

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"

#include <2geom/angle.h>
#include <2geom/circle.h>
#include <2geom/ellipse.h>
#include <2geom/path-sink.h>

#include "attributes.h"
#include "display/curve.h"
#include "document.h"
#include "preferences.h"
#include "snap-candidate.h"
#include "sp-ellipse.h"
#include "style.h"
#include "svg/svg.h"
#include "svg/path-string.h"

#define SP_2PI (2 * M_PI)

SPGenericEllipse::SPGenericEllipse()
    : SPShape()
    , start(0)
    , end(SP_2PI)
    , type(SP_GENERIC_ELLIPSE_UNDEFINED)
    , arc_type(SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE)
{
}

SPGenericEllipse::~SPGenericEllipse()
= default;

/*
 * Ellipse and rect is the only SP object who's repr element tag name changes
 * during it's lifetime. During undo and redo these changes can cause
 * the SP object to become unstuck from the repr's true state.
 */
void SPGenericEllipse::tag_name_changed(gchar const* oldname, gchar const* newname)
{
    const std::string typeString = newname;
    if (typeString == "svg:circle") {
        type = SP_GENERIC_ELLIPSE_CIRCLE;
    } else if (typeString == "svg:ellipse") {
        type = SP_GENERIC_ELLIPSE_ELLIPSE;
    } else if (typeString == "svg:path") {
        type = SP_GENERIC_ELLIPSE_ARC;
    } else {
        type = SP_GENERIC_ELLIPSE_UNDEFINED;
    }
}

void SPGenericEllipse::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    // std::cout << "SPGenericEllipse::build: Entrance: " << this->type
    //           << "  ("  << g_quark_to_string(repr->code()) << ")" << std::endl;

    switch ( type ) {
        case SP_GENERIC_ELLIPSE_ARC:
            this->readAttr(SPAttr::SODIPODI_CX);
            this->readAttr(SPAttr::SODIPODI_CY);
            this->readAttr(SPAttr::SODIPODI_RX);
            this->readAttr(SPAttr::SODIPODI_RY);
            this->readAttr(SPAttr::SODIPODI_START);
            this->readAttr(SPAttr::SODIPODI_END);
            this->readAttr(SPAttr::SODIPODI_OPEN);
            this->readAttr(SPAttr::SODIPODI_ARC_TYPE);
            break;

        case SP_GENERIC_ELLIPSE_CIRCLE:
            this->readAttr(SPAttr::CX);
            this->readAttr(SPAttr::CY);
            this->readAttr(SPAttr::R);
            break;

        case SP_GENERIC_ELLIPSE_ELLIPSE:
            this->readAttr(SPAttr::CX);
            this->readAttr(SPAttr::CY);
            this->readAttr(SPAttr::RX);
            this->readAttr(SPAttr::RY);
            break;

        default:
            std::cerr << "SPGenericEllipse::build() unknown defined type." << std::endl;
    }

    // std::cout << "    cx: " << cx.write() << std::endl;
    // std::cout << "    cy: " << cy.write() << std::endl;
    // std::cout << "    rx: " << rx.write() << std::endl;
    // std::cout << "    ry: " << ry.write() << std::endl;
    SPShape::build(document, repr);
}

void SPGenericEllipse::set(SPAttr key, gchar const *value)
{
    // There are multiple ways to set internal cx, cy, rx, and ry (via SVG attributes or Sodipodi
    // attributes) thus we don't want to unset them if a read fails (e.g., when we explicitly clear
    // an attribute by setting it to NULL).

    // We must update the SVGLengths immediately or nodes may be misplaced after they are moved.
    double const w = viewport.width();
    double const h = viewport.height();
    double const d = hypot(w, h) / sqrt(2); // diagonal
    double const em = style->font_size.computed;
    double const ex = em * 0.5;

    SVGLength t;
    switch (key) {
    case SPAttr::CX:
    case SPAttr::SODIPODI_CX:
        if( t.read(value) ) cx = t;
        cx.update( em, ex, w );
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::CY:
    case SPAttr::SODIPODI_CY:
        if( t.read(value) ) cy = t;
        cy.update( em, ex, h );
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::RX:
    case SPAttr::SODIPODI_RX:
        if( t.read(value) && t.value > 0.0 ) rx = t;
        rx.update( em, ex, w );
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::RY:
    case SPAttr::SODIPODI_RY:
        if( t.read(value) && t.value > 0.0 ) ry = t;
        ry.update( em, ex, h );
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::R:
        if( t.read(value) && t.value > 0.0 ) {
            this->ry = this->rx = t;
        }
        rx.update( em, ex, d );
        ry.update( em, ex, d );
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::SODIPODI_START:
        if (value) {
            sp_svg_number_read_d(value, &this->start);
        } else {
            this->start = 0;
        }
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::SODIPODI_END:
        if (value) {
            sp_svg_number_read_d(value, &this->end);
        } else {
            this->end = 2 * M_PI;
        }
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::SODIPODI_OPEN:
        // This is for reading in old files.
        if ((!value) || strcmp(value,"true")) {
            // We rely on this to reset arc_type when changing an arc to
            // an ellipse/circle, so it is drawn as a closed path.
            // A clone will not even change it's this->type
            this->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE;
        } else {
            this->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_ARC;
        }
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    case SPAttr::SODIPODI_ARC_TYPE:
        // To read in old files that use 'open', we need to not set if value is null.
        // We could also check inkscape version.
        if (value) {
            if (!strcmp(value,"arc")) {
                this->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_ARC;
            } else if (!strcmp(value,"chord")) {
                this->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_CHORD;
            } else {
                this->arc_type = SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE;
            }
        }
        this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        break;

    default:
        SPShape::set(key, value);
        break;
    }
}

void SPGenericEllipse::update(SPCtx *ctx, guint flags)
{
    // std::cout << "\nSPGenericEllipse::update: Entrance" << std::endl;
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        Geom::Rect const &viewbox = ((SPItemCtx const *) ctx)->viewport;

        double const dx = viewbox.width();
        double const dy = viewbox.height();
        double const dr = hypot(dx, dy) / sqrt(2);
        double const em = this->style->font_size.computed;
        double const ex = em * 0.5; // fixme: get from pango or libnrtype

        this->cx.update(em, ex, dx);
        this->cy.update(em, ex, dy);
        this->rx.update(em, ex, dr);
        this->ry.update(em, ex, dr);

        this->set_shape();
    }

    SPShape::update(ctx, flags);
    // std::cout << "SPGenericEllipse::update: Exit\n" << std::endl;
}

Inkscape::XML::Node *SPGenericEllipse::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags)
{
    // std::cout << "\nSPGenericEllipse::write: Entrance ("
    //           << (repr == NULL ? " NULL" : g_quark_to_string(repr->code()))
    //           << ")" << std::endl;

    GenericEllipseType new_type = SP_GENERIC_ELLIPSE_UNDEFINED;
    if (_isSlice() || hasPathEffectOnClipOrMaskRecursive(this) ) {
        new_type = SP_GENERIC_ELLIPSE_ARC;
    } else if ( rx.computed == ry.computed ) {
        new_type = SP_GENERIC_ELLIPSE_CIRCLE;
    } else {
        new_type = SP_GENERIC_ELLIPSE_ELLIPSE;
    }
    // std::cout << "  new_type: " << new_type << std::endl;

    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {

        switch ( new_type ) {

            case SP_GENERIC_ELLIPSE_ARC:
                repr = xml_doc->createElement("svg:path");
                break;
            case SP_GENERIC_ELLIPSE_CIRCLE:
                repr = xml_doc->createElement("svg:circle");
                break;
            case SP_GENERIC_ELLIPSE_ELLIPSE:
                repr = xml_doc->createElement("svg:ellipse");
                break;
            case SP_GENERIC_ELLIPSE_UNDEFINED:
            default:
                std::cerr << "SPGenericEllipse::write(): unknown type." << std::endl;
        }
    }

    if (type != new_type) {
        switch (new_type) {
            case SP_GENERIC_ELLIPSE_ARC:
                repr->setCodeUnsafe(g_quark_from_string("svg:path"));
                break;
            case SP_GENERIC_ELLIPSE_CIRCLE:
                repr->setCodeUnsafe(g_quark_from_string("svg:circle"));
                break;
            case SP_GENERIC_ELLIPSE_ELLIPSE:
                repr->setCodeUnsafe(g_quark_from_string("svg:ellipse"));
                break;
            default:
                std::cerr << "SPGenericEllipse::write(): unknown type." << std::endl;
        }
        type = new_type;
    }

    // std::cout << "  type: " << g_quark_to_string( repr->code() ) << std::endl;
    // std::cout << "  cx: " << cx.write() << " " << cx.computed
    //           << "  cy: " << cy.write() << " " << cy.computed
    //           << "  rx: " << rx.write() << " " << rx.computed
    //           << "  ry: " << ry.write() << " " << ry.computed << std::endl;

    switch ( type ) {
        case SP_GENERIC_ELLIPSE_UNDEFINED:
        case SP_GENERIC_ELLIPSE_ARC:

            repr->removeAttribute("cx");
            repr->removeAttribute("cy");
            repr->removeAttribute("rx");
            repr->removeAttribute("ry");
            repr->removeAttribute("r");

            if (flags & SP_OBJECT_WRITE_EXT) {

                repr->setAttribute("sodipodi:type", "arc");
                repr->setAttributeSvgLength("sodipodi:cx", cx);
                repr->setAttributeSvgLength("sodipodi:cy", cy);
                repr->setAttributeSvgLength("sodipodi:rx", rx);
                repr->setAttributeSvgLength("sodipodi:ry", ry);

                // write start and end only if they are non-trivial; otherwise remove
                if (_isSlice()) {
                    repr->setAttributeSvgDouble("sodipodi:start", start);
                    repr->setAttributeSvgDouble("sodipodi:end", end);

                    switch ( arc_type ) {
                        case SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE:
                            repr->removeAttribute("sodipodi:open"); // For backwards compat.
                            repr->setAttribute("sodipodi:arc-type", "slice");
                            break;
                        case SP_GENERIC_ELLIPSE_ARC_TYPE_CHORD:
                            // A chord's path isn't "open" but its fill most closely resembles an arc.
                            repr->setAttribute("sodipodi:open", "true"); // For backwards compat.
                            repr->setAttribute("sodipodi:arc-type", "chord");
                            break;
                        case SP_GENERIC_ELLIPSE_ARC_TYPE_ARC:
                            repr->setAttribute("sodipodi:open", "true"); // For backwards compat.
                            repr->setAttribute("sodipodi:arc-type", "arc");
                            break;
                        default:
                            std::cerr << "SPGenericEllipse::write: unknown arc-type." << std::endl;
                    }
                } else {
                    repr->removeAttribute("sodipodi:end");
                    repr->removeAttribute("sodipodi:start");
                    repr->removeAttribute("sodipodi:open");
                    repr->removeAttribute("sodipodi:arc-type");
                }
            }

            // write d=
            set_elliptical_path_attribute(repr);
            break;

        case SP_GENERIC_ELLIPSE_CIRCLE:
            repr->setAttributeSvgLength("cx", cx);
            repr->setAttributeSvgLength("cy", cy);
            repr->setAttributeSvgLength("r", rx);
            repr->removeAttribute("rx");
            repr->removeAttribute("ry");
            repr->removeAttribute("sodipodi:cx");
            repr->removeAttribute("sodipodi:cy");
            repr->removeAttribute("sodipodi:rx");
            repr->removeAttribute("sodipodi:ry");
            repr->removeAttribute("sodipodi:end");
            repr->removeAttribute("sodipodi:start");
            repr->removeAttribute("sodipodi:open");
            repr->removeAttribute("sodipodi:arc-type");
            repr->removeAttribute("sodipodi:type");
            repr->removeAttribute("d");
            break;

        case SP_GENERIC_ELLIPSE_ELLIPSE:
            repr->setAttributeSvgLength("cx", cx);
            repr->setAttributeSvgLength("cy", cy);
            repr->setAttributeSvgLength("rx", rx);
            repr->setAttributeSvgLength("ry", ry);
            repr->removeAttribute("r");
            repr->removeAttribute("sodipodi:cx");
            repr->removeAttribute("sodipodi:cy");
            repr->removeAttribute("sodipodi:rx");
            repr->removeAttribute("sodipodi:ry");
            repr->removeAttribute("sodipodi:end");
            repr->removeAttribute("sodipodi:start");
            repr->removeAttribute("sodipodi:open");
            repr->removeAttribute("sodipodi:arc-type");
            repr->removeAttribute("sodipodi:type");
            repr->removeAttribute("d");
            break;

        default:
            std::cerr << "SPGenericEllipse::write: unknown type." << std::endl;
    }

    set_shape(); // evaluate SPCurve

    SPShape::write(xml_doc, repr, flags);

    return repr;
}

const char *SPGenericEllipse::typeName() const
{
    switch (type) {
        case SP_GENERIC_ELLIPSE_UNDEFINED:
        case SP_GENERIC_ELLIPSE_ARC:
            return "arc";
        case SP_GENERIC_ELLIPSE_CIRCLE:
        case SP_GENERIC_ELLIPSE_ELLIPSE:
        default:
            return "circle"; //
    }
}

const char *SPGenericEllipse::displayName() const
{
    switch ( type ) {
        case SP_GENERIC_ELLIPSE_UNDEFINED:
        case SP_GENERIC_ELLIPSE_ARC:
            if (_isSlice()) {
                switch ( arc_type ) {
                    case SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE:
                        return _("Slice");
                        break;
                    case SP_GENERIC_ELLIPSE_ARC_TYPE_CHORD:
                        return _("Chord");
                        break;
                    case SP_GENERIC_ELLIPSE_ARC_TYPE_ARC:
                        return _("Arc");
                        break;
                }
            } // fallback to ellipse
        case SP_GENERIC_ELLIPSE_ELLIPSE:
            return _("Ellipse");
        case SP_GENERIC_ELLIPSE_CIRCLE:
            return _("Circle");
        default:
            return "Unknown ellipse: ERROR";
    }
}

// Create path for rendering shape on screen
void SPGenericEllipse::set_shape()
{
    // std::cout << "SPGenericEllipse::set_shape: Entrance" << std::endl;
    if (checkBrokenPathEffect()) {
        return;
    }
    if (Geom::are_near(this->rx.computed, 0) || Geom::are_near(this->ry.computed, 0)) {
        return;
    }

    this->normalize();

    // For simplicity, we use a circle with center (0, 0) and radius 1 for our calculations.
    Geom::Circle circle(0, 0, 1);

    if (!this->_isSlice()) {
        start = 0.0;
        end = 2.0*M_PI;
    }
    double incr = end - start; // arc angle
    if (incr < 0.0) incr += 2.0*M_PI;    

    int numsegs = 1 + int(incr*2.0/M_PI); // number of arc segments
    if (numsegs > 4) numsegs = 4;

    incr = incr/numsegs; // limit arc angle to less than 90 degrees
    Geom::Path path(Geom::Point::polar(start));
    Geom::EllipticalArc* arc;
    for (int seg = 0; seg < numsegs; seg++) {
        arc = circle.arc(Geom::Point::polar(start + seg*incr), Geom::Point::polar(start + (seg + 0.5)*incr), Geom::Point::polar(start + (seg + 1.0)*incr));
        path.append(*arc);
        delete arc;
    }
    Geom::PathBuilder pb;
    pb.append(path);
    if (this->_isSlice() && this->arc_type == SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE) {
        pb.lineTo(Geom::Point(0, 0));
    }

    if (this->arc_type != SP_GENERIC_ELLIPSE_ARC_TYPE_ARC) {
        pb.closePath();
    } else {
        pb.flush();
    }

    auto c = SPCurve(pb.peek());

    // gchar *str = sp_svg_write_path(curve->get_pathvector());
    // std::cout << "  path: " << str << std::endl;
    // g_free(str);

    // Stretching / moving the calculated shape to fit the actual dimensions.
    Geom::Affine aff = Geom::Scale(rx.computed, ry.computed) * Geom::Translate(cx.computed, cy.computed);
    c.transform(aff);
    prepareShapeForLPE(&c);
}

Geom::Affine SPGenericEllipse::set_transform(Geom::Affine const &xform)
{
    if (pathEffectsEnabled() && !optimizeTransforms()) {
        return xform;
    }

    /* Calculate ellipse start in parent coords. */
    Geom::Point pos(Geom::Point(this->cx.computed, this->cy.computed) * xform);

    /* This function takes care of translation and scaling, we return whatever parts we can't
       handle. */
    Geom::Affine ret(Geom::Affine(xform).withoutTranslation());
    gdouble const sw = hypot(ret[0], ret[1]);
    gdouble const sh = hypot(ret[2], ret[3]);

    if (sw > 1e-9) {
        ret[0] /= sw;
        ret[1] /= sw;
    } else {
        ret[0] = 1.0;
        ret[1] = 0.0;
    }

    if (sh > 1e-9) {
        ret[2] /= sh;
        ret[3] /= sh;
    } else {
        ret[2] = 0.0;
        ret[3] = 1.0;
    }

    if (this->rx._set) {
        this->rx.scale( sw );
    }

    if (this->ry._set) {
        this->ry.scale( sh );
    }

    /* Find start in item coords */
    pos = pos * ret.inverse();
    this->cx = pos[Geom::X];
    this->cy = pos[Geom::Y];

    this->set_shape();

    // Adjust stroke width
    if (!g_strcmp0(getAttribute("sodipodi:arc-type"), "slice") || 
        !g_strcmp0(getAttribute("sodipodi:arc-type"), "chord") ||
        !g_strcmp0(getAttribute("sodipodi:arc-type"), "arc")) 
    {
        double const expansion = transform.descrim();
        adjust_stroke_width_recursive(expansion);
    }
    this->adjust_stroke(sqrt(fabs(sw * sh)));

    // Adjust pattern fill
    this->adjust_pattern(xform * ret.inverse());

    // Adjust gradient fill
    this->adjust_gradient(xform * ret.inverse());

    return ret;
}

void SPGenericEllipse::snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) const
{
	// CPPIFY: is this call necessary?
	const_cast<SPGenericEllipse*>(this)->normalize();

    Geom::Affine const i2dt = this->i2dt_affine();

    // Snap to the 4 quadrant points of the ellipse, but only if the arc
    // spans far enough to include them
    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_ELLIPSE_QUADRANT_POINT)) {
        for (double angle = 0; angle < SP_2PI; angle += M_PI_2) {
            if (Geom::AngleInterval(this->start, this->end, true).contains(angle)) {
                Geom::Point pt = this->getPointAtAngle(angle) * i2dt;
                p.emplace_back(pt, Inkscape::SNAPSOURCE_ELLIPSE_QUADRANT_POINT, Inkscape::SNAPTARGET_ELLIPSE_QUADRANT_POINT);
            }
        }
    }

    double cx = this->cx.computed;
    double cy = this->cy.computed;
    

    bool slice = this->_isSlice();

    // Add the centre, if we have a closed slice or when explicitly asked for
    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_NODE_CUSP) && slice &&
        this->arc_type == SP_GENERIC_ELLIPSE_ARC_TYPE_SLICE) {
        Geom::Point pt = Geom::Point(cx, cy) * i2dt;
        p.emplace_back(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP);
    }

    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_OBJECT_MIDPOINT)) {
        Geom::Point pt = Geom::Point(cx, cy) * i2dt;
        p.emplace_back(pt, Inkscape::SNAPSOURCE_OBJECT_MIDPOINT, Inkscape::SNAPTARGET_OBJECT_MIDPOINT);
    }

    // And if we have a slice, also snap to the endpoints
    if (snapprefs->isTargetSnappable(Inkscape::SNAPTARGET_NODE_CUSP) && slice) {
        // Add the start point, if it's not coincident with a quadrant point
        if (!Geom::are_near(std::fmod(this->start, M_PI_2), 0)) {
            Geom::Point pt = this->getPointAtAngle(this->start) * i2dt;
            p.emplace_back(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP);
        }

        // Add the end point, if it's not coincident with a quadrant point
        if (!Geom::are_near(std::fmod(this->end, M_PI_2), 0)) {
            Geom::Point pt = this->getPointAtAngle(this->end) * i2dt;
            p.emplace_back(pt, Inkscape::SNAPSOURCE_NODE_CUSP, Inkscape::SNAPTARGET_NODE_CUSP);
        }
    }
}

void SPGenericEllipse::modified(guint flags)
{
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        this->set_shape();
    }

    SPShape::modified(flags);
}

void SPGenericEllipse::update_patheffect(bool write) {
    SPShape::update_patheffect(write);
}

void SPGenericEllipse::normalize()
{
    Geom::AngleInterval a(this->start, this->end, true);

    this->start = a.initialAngle().radians0();
    this->end = a.finalAngle().radians0();
}

Geom::Point SPGenericEllipse::getPointAtAngle(double arg) const
{
    return Geom::Point::polar(arg) * Geom::Scale(rx.computed, ry.computed) * Geom::Translate(cx.computed, cy.computed);
}

/*
 * set_elliptical_path_attribute:
 *
 * Convert center to endpoint parameterization and set it to repr.
 *
 * See SVG 1.0 Specification W3C Recommendation
 * ``F.6 Elliptical arc implementation notes'' for more detail.
 */
bool SPGenericEllipse::set_elliptical_path_attribute(Inkscape::XML::Node *repr)
{
    // Make sure our pathvector is up to date.
    this->set_shape();

    if (_curve) {
        repr->setAttribute("d", sp_svg_write_path(_curve->get_pathvector()));
    } else {
        repr->removeAttribute("d");
    }

    return true;
}

void SPGenericEllipse::position_set(gdouble x, gdouble y, gdouble rx, gdouble ry)
{
    this->cx = x;
    this->cy = y;
    this->rx = rx;
    this->ry = ry;

    Inkscape::Preferences * prefs = Inkscape::Preferences::get();

    // those pref values are in degrees, while we want radians
    if (prefs->getDouble("/tools/shapes/arc/start", 0.0) != 0) {
        this->start = Geom::Angle::from_degrees(prefs->getDouble("/tools/shapes/arc/start", 0.0)).radians0();
    }

    if (prefs->getDouble("/tools/shapes/arc/end", 0.0) != 0) {
        this->end = Geom::Angle::from_degrees(prefs->getDouble("/tools/shapes/arc/end", 0.0)).radians0();
    }

    this->arc_type = (GenericEllipseArcType)prefs->getInt("/tools/shapes/arc/arc_type", 0);
    if (this->type != SP_GENERIC_ELLIPSE_ARC && _isSlice()) {
        // force an update while creating shapes, so correct rendering is shown initially
        updateRepr();
    }

    this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

bool SPGenericEllipse::_isSlice() const
{
    Geom::AngleInterval a(this->start, this->end, true);

    return !(Geom::are_near(a.extent(), 0) || Geom::are_near(a.extent(), SP_2PI));
}

/**
Returns the ratio in which the vector from p0 to p1 is stretched by transform
 */
gdouble SPGenericEllipse::vectorStretch(Geom::Point p0, Geom::Point p1, Geom::Affine xform) {
    if (p0 == p1) {
        return 0;
    }

    return (Geom::distance(p0 * xform, p1 * xform) / Geom::distance(p0, p1));
}

void SPGenericEllipse::setVisibleRx(gdouble rx) {
    if (rx == 0) {
        this->rx.unset();
    } else {
        this->rx = rx / SPGenericEllipse::vectorStretch(
            Geom::Point(this->cx.computed + 1, this->cy.computed),
            Geom::Point(this->cx.computed, this->cy.computed),
            this->i2doc_affine());
    }

    this->updateRepr();
}

void SPGenericEllipse::setVisibleRy(gdouble ry) {
    if (ry == 0) {
        this->ry.unset();
    } else {
        this->ry = ry / SPGenericEllipse::vectorStretch(
            Geom::Point(this->cx.computed, this->cy.computed + 1),
            Geom::Point(this->cx.computed, this->cy.computed),
            this->i2doc_affine());
    }

    this->updateRepr();
}

gdouble SPGenericEllipse::getVisibleRx() const {
    if (!this->rx._set) {
        return 0;
    }

    return this->rx.computed * SPGenericEllipse::vectorStretch(
        Geom::Point(this->cx.computed + 1, this->cy.computed),
        Geom::Point(this->cx.computed, this->cy.computed),
        this->i2doc_affine());
}

gdouble SPGenericEllipse::getVisibleRy() const {
    if (!this->ry._set) {
        return 0;
    }

    return this->ry.computed * SPGenericEllipse::vectorStretch(
        Geom::Point(this->cx.computed, this->cy.computed + 1),
        Geom::Point(this->cx.computed, this->cy.computed),
        this->i2doc_affine());
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
