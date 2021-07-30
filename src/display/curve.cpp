// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Johan Engelen
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2008 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/curve.h"

#include <glib.h>
#include <2geom/pathvector.h>
#include <2geom/sbasis-geometric.h>
#include <2geom/sbasis-to-bezier.h>
#include <2geom/point.h>

#include <utility>

/**
 * Routines for SPCurve and for its Geom::PathVector
 */

SPCurve::smart_pointer //
SPCurve::new_from_rect(Geom::Rect const &rect, bool all_four_sides)
{
    auto c = std::make_unique<SPCurve>();

    Geom::Point p = rect.corner(0);
    c->moveto(p);

    for (int i=3; i>=1; --i) {
        c->lineto(rect.corner(i));
    }

    if (all_four_sides) {
        // When _constrained_ snapping to a path, the 2geom::SimpleCrosser will be invoked which doesn't consider the closing segment.
        // of a path. Consequently, in case we want to snap to for example the page border, we must provide all four sides of the
        // rectangle explicitly
        c->lineto(rect.corner(0));
    } else {
        // ... instead of just three plus a closing segment
        c->closepath();
    }

    return c;
}

SPCurve::~SPCurve()
= default;

/* Methods */

void
SPCurve::set_pathvector(Geom::PathVector const & new_pathv)
{
    _pathv = new_pathv;
}

Geom::PathVector const &
SPCurve::get_pathvector() const
{
    return _pathv;
}

/*
 * Returns the number of segments of all paths summed
 * This count includes the closing line segment of a closed path.
 */
size_t
SPCurve::get_segment_count() const
{
    return _pathv.curveCount();
}

/**
 * Increase _refcount of curve.
 */
SPCurve::smart_pointer //
SPCurve::ref()
{
    _refcount += 1;

    return smart_pointer(this);
}

/**
 * Decrease refcount of curve, with possible destruction.
 */
void SPCurve::_unref()
{
    _refcount -= 1;

    if (_refcount < 1) {
        delete this;
    }
}

/**
 * Create new curve from this curve's pathvector array.
 */
SPCurve::smart_pointer //
SPCurve::copy() const
{
    return std::make_unique<SPCurve>(_pathv);
}

/**
 * Return a copy of `curve` or NULL if `curve` is NULL
 */
SPCurve::smart_pointer //
SPCurve::copy(SPCurve const *curve)
{
    return curve ? curve->copy() : nullptr;
}

/**
 * Returns a list of new curves corresponding to the subpaths in \a curve.
 * 2geomified
 */
std::list<SPCurve::smart_pointer> //
SPCurve::split() const
{
    std::list<smart_pointer> l;

    for (const auto & path_it : _pathv) {
        Geom::PathVector newpathv;
        newpathv.push_back(path_it);
        SPCurve * newcurve = new SPCurve(newpathv);
        l.emplace_back(newcurve);
    }

    return l;
}

/**
 * Transform all paths in curve using matrix.
 */
void
SPCurve::transform(Geom::Affine const &m)
{
    _pathv *= m;
}

/**
 * Set curve to empty curve.
 * In more detail: this clears the internal pathvector from all its paths.
 */
void
SPCurve::reset()
{
    _pathv.clear();
}

/** Several consecutive movetos are ALLOWED
 *  Ref: http://www.w3.org/TR/SVG11/implnote.html#PathElementImplementationNotes
 * (first subitem of the item about zero-length path segments) */

/**
 * Calls SPCurve::moveto() with point made of given coordinates.
 */
void
SPCurve::moveto(double x, double y)
{
    moveto(Geom::Point(x, y));
}
/**
 * Perform a moveto to a point, thus starting a new subpath.
 * Point p must be finite.
 */
void
SPCurve::moveto(Geom::Point const &p)
{
    Geom::Path path(p);
    path.setStitching(true);
    _pathv.push_back(path);
}

/**
 * Adds a line to the current subpath.
 * Point p must be finite.
 */
void
SPCurve::lineto(Geom::Point const &p)
{
    if (_pathv.empty())  g_message("SPCurve::lineto - path is empty!");
    else _pathv.back().appendNew<Geom::LineSegment>( p );
}
/**
 * Calls SPCurve::lineto( Geom::Point(x,y) )
 */
void
SPCurve::lineto(double x, double y)
{
    lineto(Geom::Point(x,y));
}

/**
 * Adds a quadratic bezier segment to the current subpath.
 * All points must be finite.
 */
void
SPCurve::quadto(Geom::Point const &p1, Geom::Point const &p2)
{
    if (_pathv.empty())  g_message("SPCurve::quadto - path is empty!");
    else _pathv.back().appendNew<Geom::QuadraticBezier>( p1, p2);
}
/**
 * Calls SPCurve::quadto( Geom::Point(x1,y1), Geom::Point(x2,y2) )
 * All coordinates must be finite.
 */
void
SPCurve::quadto(double x1, double y1, double x2, double y2)
{
    quadto( Geom::Point(x1,y1), Geom::Point(x2,y2) );
}

/**
 * Adds a bezier segment to the current subpath.
 * All points must be finite.
 */
void
SPCurve::curveto(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2)
{
    if (_pathv.empty())  g_message("SPCurve::curveto - path is empty!");
    else _pathv.back().appendNew<Geom::CubicBezier>( p0, p1, p2 );
}
/**
 * Calls SPCurve::curveto( Geom::Point(x0,y0), Geom::Point(x1,y1), Geom::Point(x2,y2) )
 * All coordinates must be finite.
 */
void
SPCurve::curveto(double x0, double y0, double x1, double y1, double x2, double y2)
{
    curveto( Geom::Point(x0,y0), Geom::Point(x1,y1), Geom::Point(x2,y2) );
}

/**
 * Close current subpath by possibly adding a line between start and end.
 */
void
SPCurve::closepath()
{
    _pathv.back().close(true);
}

/** Like SPCurve::closepath() but sets the end point of the last subpath
    to the subpath start point instead of adding a new lineto.

    Used for freehand drawing when the user draws back to the start point.
**/
void
SPCurve::closepath_current()
{
    if (_pathv.back().size() > 0 && dynamic_cast<Geom::LineSegment const *>(&_pathv.back().back_open())) {
        _pathv.back().erase_last();
    } else {
        _pathv.back().setFinal(_pathv.back().initialPoint());
    }
    _pathv.back().close(true);
}

/**
 * True if no paths are in curve. If it only contains a path with only a moveto, the path is considered NON-empty
 */
bool
SPCurve::is_empty() const
{
    return _pathv.empty();
}

/**
 * True if paths are in curve. If it only contains a path with only a moveto, the path is considered as unset FALSE
 */
bool
SPCurve::is_unset() const
{
    if (get_segment_count()) {
        return false;
    }
    return true;
}

/**
 * True iff all subpaths are closed.
 * Returns false if the curve is empty.
 */
bool
SPCurve::is_closed() const
{
    if (is_empty()) {
        return false;
    } 
    
    for (const auto & it : _pathv) {
        if ( ! it.closed() ) {
            return false;
        }
    }
    
    return true;    
}

/**
 * True if both curves are equal
 */
bool SPCurve::is_equal(SPCurve const *other) const
{
    if(other == nullptr) {
        return false;
    } 

    if(_pathv == other->get_pathvector()){
        return true;
    }
    
    return false;
}

/**
 * Return last pathsegment (possibly the closing path segment) of the last path in PathVector or NULL.
 * If the last path is empty (contains only a moveto), the function returns NULL
 */
Geom::Curve const *
SPCurve::last_segment() const
{
    if (is_empty()) {
        return nullptr;
    }
    if (_pathv.back().empty()) {
        return nullptr;
    }

    return &_pathv.back().back_default();
}

/**
 * Return last path in PathVector or NULL.
 */
Geom::Path const *
SPCurve::last_path() const
{
    if (is_empty()) {
        return nullptr;
    }

    return &_pathv.back();
}

/**
 * Return first pathsegment in PathVector or NULL.
 * equal in functionality to SPCurve::first_bpath()
 */
Geom::Curve const *
SPCurve::first_segment() const
{
    if (is_empty()) {
        return nullptr;
    }
    if (_pathv.front().empty()) {
        return nullptr;
    }

    return &_pathv.front().front();
}

/**
 * Return first path in PathVector or NULL.
 */
Geom::Path const *
SPCurve::first_path() const
{
    if (is_empty()) {
        return nullptr;
    }

    return &_pathv.front();
}

/**
 * Return first point of first subpath or nothing when the path is empty.
 */
std::optional<Geom::Point>
SPCurve::first_point() const
{
    std::optional<Geom::Point> retval;

    if (!is_empty()) {
        retval = _pathv.front().initialPoint();
    }

    return retval;
}

/**
 * Return the second point of first subpath or _movePos if curve too short.
 * If the pathvector is empty, this returns nothing. If the first path is only a moveto, this method
 * returns the first point of the second path, if it exists. If there is no 2nd path, it returns the
 * first point of the first path.
 */
std::optional<Geom::Point>
SPCurve::second_point() const
{
    std::optional<Geom::Point> retval;
    if (!is_empty()) {
        if (_pathv.front().empty()) {
            // first path is only a moveto
            // check if there is second path
            if (_pathv.size() > 1) {
                retval = _pathv[1].initialPoint();
            } else {
                retval = _pathv[0].initialPoint();
            }
        } else {
            retval = _pathv.front()[0].finalPoint();
        }
    }

    return retval;
}

/**
 * Return the second-last point of last subpath or first point when that last subpath has only a moveto.
 */
std::optional<Geom::Point>
SPCurve::penultimate_point() const
{
    std::optional<Geom::Point> retval;
    if (!is_empty()) {
        Geom::Path const &lastpath = _pathv.back();
        if (!lastpath.empty()) {
            Geom::Curve const &back = lastpath.back_default();
            retval = back.initialPoint();
        } else {
            retval = lastpath.initialPoint();
        }
    }

    return retval;
}

/**
 * Return last point of last subpath or nothing when the curve is empty.
 * If the last path is only a moveto, then return that point.
 */
std::optional<Geom::Point>
SPCurve::last_point() const
{
    std::optional<Geom::Point> retval;

    if (!is_empty()) {
        retval = _pathv.back().finalPoint();
    }

    return retval;
}

/**
 * Returns a *new* \a curve but drawn in the opposite direction.
 * Should result in the same shape, but
 * with all its markers drawn facing the other direction.
 * Reverses the order of subpaths as well
 **/
SPCurve::smart_pointer //
SPCurve::create_reverse() const
{
    return std::make_unique<SPCurve>(_pathv.reversed());
}

/**
 * Append \a curve2 to \a this.
 * If \a use_lineto is false, simply add all paths in \a curve2 to \a this;
 * if \a use_lineto is true, combine \a this's last path and \a curve2's first path and add the rest of the paths in \a curve2 to \a this.
 */
void
SPCurve::append(SPCurve const &curve2,
                bool use_lineto)
{
    append(curve2._pathv, use_lineto);
}
void SPCurve::append(Geom::PathVector const &pathv, bool use_lineto)
{
    if (pathv.empty())
        return;

    if (use_lineto) {
        Geom::PathVector::const_iterator it = pathv.begin();
        if ( ! _pathv.empty() ) {
            Geom::Path & lastpath = _pathv.back();
            lastpath.appendNew<Geom::LineSegment>( (*it).initialPoint() );
            lastpath.append( (*it) );
        } else {
            _pathv.push_back( (*it) );
        }

        for (++it; it != pathv.end(); ++it) {
            _pathv.push_back( (*it) );
        }
    } else {
        for (const auto &it : pathv) {
            _pathv.push_back( it );
        }
    }
}

/**
 * Append \a c1 to \a this with possible fusing of close endpoints. If the end of this curve and the start of c1 are within tolerance distance,
 * then the startpoint of c1 is moved to the end of this curve and the first subpath of c1 is appended to the last subpath of this curve.
 * When one of the curves is empty, this curves path becomes the non-empty path.
 *
 * @param tolerance Tolerance for endpoint fusion (applied to x and y separately)
 * @return False if one of the curves (this curve or the argument curve) is closed, true otherwise.
 */
bool SPCurve::append_continuous(SPCurve const &c1, double tolerance)
{
    if (is_closed() || c1.is_closed()) {
        return false;
    }

    if (c1.is_empty()) {
        return true;
    }

    if (this->is_empty()) {
        _pathv = c1._pathv;
        return true;
    }

    if ((fabs(last_point()->x() - c1.first_point()->x()) <= tolerance) &&
        (fabs(last_point()->y() - c1.first_point()->y()) <= tolerance)) {
    // c1's first subpath can be appended to this curve's last subpath
        Geom::PathVector::const_iterator path_it = c1._pathv.begin();
        Geom::Path & lastpath = _pathv.back();

        Geom::Path newfirstpath(*path_it);
        newfirstpath.setInitial(lastpath.finalPoint());
        lastpath.append( newfirstpath );

        for (++path_it; path_it != c1._pathv.end(); ++path_it) {
            _pathv.push_back( (*path_it) );
        }

    } else {
        append(c1, true);
    }

    return true;
}

/**
 * Remove last segment of curve.
 * (Only used once in /src/pen-context.cpp)
 */
void
SPCurve::backspace()
{
    if ( is_empty() )
        return;

    if ( !_pathv.back().empty() ) {
        _pathv.back().erase_last();
        _pathv.back().close(false);
    }
}

/**
 * TODO: add comments about what this method does and what assumptions are made and requirements are put on SPCurve
 (2:08:18 AM) Johan: basically, i convert the path to pw<d2>
(2:08:27 AM) Johan: then i calculate an offset path
(2:08:29 AM) Johan: to move the knots
(2:08:36 AM) Johan: then i add it
(2:08:40 AM) Johan: then convert back to path
If I remember correctly, this moves the firstpoint to new_p0, and the lastpoint to new_p1, and moves all nodes in between according to their arclength (interpolates the movement amount)
 */
void
SPCurve::stretch_endpoints(Geom::Point const &new_p0, Geom::Point const &new_p1)
{
    if (is_empty()) {
        return;
    }

    Geom::Point const offset0( new_p0 - *first_point() );
    Geom::Point const offset1( new_p1 - *last_point() );

    Geom::Piecewise<Geom::D2<Geom::SBasis> > pwd2 = _pathv.front().toPwSb();
    Geom::Piecewise<Geom::SBasis> arclength = Geom::arcLengthSb(pwd2);
    if ( arclength.lastValue() <= 0 ) {
        g_error("SPCurve::stretch_endpoints - arclength <= 0");
        throw;
    }
    arclength *= 1./arclength.lastValue();
    Geom::Point const A( offset0 );
    Geom::Point const B( offset1 );
    Geom::Piecewise<Geom::SBasis> offsetx = (arclength*-1.+1)*A[0] + arclength*B[0];
    Geom::Piecewise<Geom::SBasis> offsety = (arclength*-1.+1)*A[1] + arclength*B[1];
    Geom::Piecewise<Geom::D2<Geom::SBasis> > offsetpath = Geom::sectionize( Geom::D2<Geom::Piecewise<Geom::SBasis> >(offsetx, offsety) );
    pwd2 += offsetpath;
    _pathv = Geom::path_from_piecewise( pwd2, 0.001 );
}

/**
 *  sets start of first path to new_p0, and end of first path to  new_p1
 */
void
SPCurve::move_endpoints(Geom::Point const &new_p0, Geom::Point const &new_p1)
{
    if (is_empty()) {
        return;
    }
    _pathv.front().setInitial(new_p0);
    _pathv.front().setFinal(new_p1);
}

/**
 * returns the number of nodes in a path, used for statusbar text when selecting an spcurve.
 * Sum of nodes in all the paths. When a path is closed, and its closing line segment is of zero-length,
 * this function will not count the closing knot double (so basically ignores the closing line segment when it has zero length)
 */
size_t
SPCurve::nodes_in_path() const
{
    size_t nr = 0;
    for(const auto & it : _pathv) {
        // if the path does not have any segments, it is a naked moveto,
        // and therefore any path has at least one valid node
        size_t psize = std::max<size_t>(1, it.size_closed());
        nr += psize;
        if (it.closed() && it.size_closed() > 0) {
            const Geom::Curve &closingline = it.back_closed();
            // the closing line segment is always of type
            // Geom::LineSegment.
            if (are_near(closingline.initialPoint(), closingline.finalPoint())) {
                // closingline.isDegenerate() did not work, because it only checks for
                // *exact* zero length, which goes wrong for relative coordinates and
                // rounding errors...
                // the closing line segment has zero-length. So stop before that one!
                nr -= 1;
            }
        }
    }

    return nr;
}

/**
 *  Adds p to the last point (and last handle if present) of the last path
 */
void
SPCurve::last_point_additive_move(Geom::Point const & p)
{
    if (is_empty()) {
        return;
    }

    _pathv.back().setFinal( _pathv.back().finalPoint() + p );

    // Move handle as well when the last segment is a cubic bezier segment:
    // TODO: what to do for quadratic beziers?
    if ( Geom::CubicBezier const *lastcube = dynamic_cast<Geom::CubicBezier const *>(&_pathv.back().back()) ) {
        Geom::CubicBezier newcube( *lastcube );
        newcube.setPoint(2, newcube[2] + p);
        _pathv.back().replace( --_pathv.back().end(), newcube );
    }
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:
