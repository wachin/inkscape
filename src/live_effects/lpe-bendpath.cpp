// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 * Copyright (C) Steren Giannini 2008 <steren.giannini@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <vector>

#include "display/curve.h"
#include "live_effects/lpe-bendpath.h"
#include "ui/knot/knot-holder.h"
#include "ui/knot/knot-holder-entity.h"

// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

/* Theory in e-mail from J.F. Barraud
Let B be the skeleton path, and P the pattern (the path to be deformed).

P is a map t --> P(t) = ( x(t), y(t) ).
B is a map t --> B(t) = ( a(t), b(t) ).

The first step is to re-parametrize B by its arc length: this is the parametrization in which a point p on B is located by its distance s from start. One obtains a new map s --> U(s) = (a'(s),b'(s)), that still describes the same path B, but where the distance along B from start to
U(s) is s itself.

We also need a unit normal to the path. This can be obtained by computing a unit tangent vector, and rotate it by 90�. Call this normal vector N(s).

The basic deformation associated to B is then given by:

   (x,y) --> U(x)+y*N(x)

(i.e. we go for distance x along the path, and then for distance y along the normal)

Of course this formula needs some minor adaptations (as is it depends on the absolute position of P for instance, so a little translation is needed
first) but I think we can first forget about them.
*/

namespace Inkscape {
namespace LivePathEffect {

namespace BeP {
class KnotHolderEntityWidthBendPath : public LPEKnotHolderEntity {
    public:
        KnotHolderEntityWidthBendPath(LPEBendPath * effect) : LPEKnotHolderEntity(effect) {}
        ~KnotHolderEntityWidthBendPath() override
        {
            if (auto const lpe = dynamic_cast<LPEBendPath *> (_effect)) {
                lpe->_knotholder = nullptr;
            }
        }
        void unset_effect() { _effect = nullptr; }
        void knot_set(Geom::Point const &p, Geom::Point const &origin, guint state) override;
        Geom::Point knot_get() const override;
    };
} // BeP

LPEBendPath::LPEBendPath(LivePathEffectObject *lpeobject) :
    Effect(lpeobject),
    bend_path(_("Bend path:"), _("Path along which to bend the original path"), "bendpath", &wr, this, "M0,0 L1,0"),
    original_height(0.0),
    prop_scale(_("_Width:"), _("Width of the path"), "prop_scale", &wr, this, 1.0),
    scale_y_rel(_("W_idth in units of length"), _("Scale the width of the path in units of its length"), "scale_y_rel", &wr, this, false),
    vertical_pattern(_("_Original path is vertical"), _("Rotates the original 90 degrees, before bending it along the bend path"), "vertical", &wr, this, false),
    hide_knot(_("Hide width knot"), _("Hide width knot"),"hide_knot", &wr, this, false)
{
    registerParameter( &bend_path );
    registerParameter( &prop_scale);
    registerParameter( &scale_y_rel);
    registerParameter( &vertical_pattern);
    registerParameter(&hide_knot);

    prop_scale.param_set_digits(3);
    prop_scale.param_set_increments(0.01, 0.10);
    
    _knotholder = nullptr;
    _provides_knotholder_entities = true;
    apply_to_clippath_and_mask = true;
    concatenate_before_pwd2 = true;
}

LPEBendPath::~LPEBendPath()
{
        if (_knotholder) {
        _knotholder->clear();
        _knotholder = nullptr;
    }

}


bool 
LPEBendPath::doOnOpen(SPLPEItem const *lpeitem)
{
    if (!is_load || is_applied) {
        return false;
    }
    bend_path.reload();
    return false;
}


void
LPEBendPath::doBeforeEffect (SPLPEItem const* lpeitem)
{
    // get the item bounding box
    original_bbox(lpeitem, false, true);
    original_height = boundingbox_Y.max() - boundingbox_Y.min();
    if (is_load) {
        bend_path.reload();
    }
    if (_knotholder) {
        if (hide_knot) {
            helper_path.clear();
            _knotholder->entity.front()->knot->hide();
        } else {
            _knotholder->entity.front()->knot->show();
        }
        _knotholder->update_knots();
    }
}

void LPEBendPath::transform_multiply(Geom::Affine const &postmul, bool /*set*/)
{   
    Inkscape::Selection * selection = nullptr;
    SPItem *linked = nullptr;
    if (SP_ACTIVE_DESKTOP) {
        selection = SP_ACTIVE_DESKTOP->getSelection();
        linked = cast<SPItem>(bend_path.getObject());
    }
    if (linked) {
        linked->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
        return;
    }
    if (sp_lpe_item && sp_lpe_item->pathEffectsEnabled() && sp_lpe_item->optimizeTransforms()) {
        bend_path.param_transform_multiply(postmul, false);
    } else if( //this allow transform user spected way when lpeitem and pathparamenter are both selected
        sp_lpe_item && 
        sp_lpe_item->pathEffectsEnabled() &&
        linked &&
        (selection->includes(linked))) 
    {
        Geom::Affine transformlpeitem = sp_item_transform_repr(sp_lpe_item).inverse() * postmul;
        sp_lpe_item->transform *= transformlpeitem.inverse();
        sp_lpe_item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
    }
}

Geom::Piecewise<Geom::D2<Geom::SBasis> >
LPEBendPath::doEffect_pwd2 (Geom::Piecewise<Geom::D2<Geom::SBasis> > const & pwd2_in)
{
    using namespace Geom;

    /* Much credit should go to jfb and mgsloan of lib2geom development for the code below! */
    Geom::Affine affine = bend_path.get_relative_affine();

    if (bend_path.changed) {
        uskeleton = arc_length_parametrization(Piecewise<D2<SBasis> >(bend_path.get_pwd2() * affine),2,.1);
        uskeleton = remove_short_cuts(uskeleton,.01);
        n = rot90(derivative(uskeleton));
        n = force_continuity(remove_short_cuts(n,.01));

        bend_path.changed = false;
    }

    if (uskeleton.empty()) {
        return pwd2_in;  /// \todo or throw an exception instead? might be better to throw an exception so that the UI can display an error message or smth
    }

    D2<Piecewise<SBasis> > patternd2 = make_cuts_independent(pwd2_in);
    Piecewise<SBasis> x = vertical_pattern.get_value() ? Piecewise<SBasis>(patternd2[1]) : Piecewise<SBasis>(patternd2[0]);
    Piecewise<SBasis> y = vertical_pattern.get_value() ? Piecewise<SBasis>(patternd2[0]) : Piecewise<SBasis>(patternd2[1]);

    Interval bboxHorizontal = vertical_pattern.get_value() ? boundingbox_Y : boundingbox_X;
    Interval bboxVertical = vertical_pattern.get_value() ? boundingbox_X : boundingbox_Y;
    
    //+0.1 in x fix bug #1658855
    //We use the group bounding box size or the path bbox size to translate well x and y
    x-= bboxHorizontal.min() + 0.1;
    y-= bboxVertical.middle();

    double scaling = uskeleton.cuts.back()/bboxHorizontal.extent();

    if (scaling != 1.0) {
        x*=scaling;
    }

    if ( scale_y_rel.get_value() ) {
        y*=(scaling*prop_scale);
    } else {
        if (prop_scale != 1.0) y *= prop_scale;
    }

    Piecewise<D2<SBasis> > output = compose(uskeleton,x) + y*compose(n,x);
    return output;
}

void
LPEBendPath::resetDefaults(SPItem const* item)
{
    Effect::resetDefaults(item);
    original_bbox(cast<SPLPEItem>(item), false, true);

    Geom::Point start(boundingbox_X.min(), (boundingbox_Y.max()+boundingbox_Y.min())/2);
    Geom::Point end(boundingbox_X.max(), (boundingbox_Y.max()+boundingbox_Y.min())/2);

    if ( Geom::are_near(start,end) ) {
        end += Geom::Point(1.,0.);
    }
     
    Geom::Path path;
    path.start( start );
    path.appendNew<Geom::LineSegment>( end );
    bend_path.set_new_value( path.toPwSb(), true );
}

void
LPEBendPath::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    hp_vec.push_back(helper_path);
}

void 
LPEBendPath::addKnotHolderEntities(KnotHolder *knotholder, SPItem *item)
{
    _knotholder = knotholder;
    KnotHolderEntity *knot_entity = new BeP::KnotHolderEntityWidthBendPath(this);
    knot_entity->create(nullptr, item, _knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:WidthBend",
                         _("Change the width"));
    _knotholder->add(knot_entity);
    if (hide_knot) {
        knot_entity->knot->hide();
        knot_entity->update_knot();
    }
}

namespace BeP {

void 
KnotHolderEntityWidthBendPath::knot_set(Geom::Point const &p, Geom::Point const& /*origin*/, guint state)
{
    LPEBendPath *lpe = dynamic_cast<LPEBendPath *> (_effect);
    if (!lpe)
        return;

    Geom::Point const s = snap_knot_position(p, state);
    Geom::Path path_in = lpe->bend_path.get_pathvector().pathAt(Geom::PathVectorTime(0, 0, 0.0));
    Geom::Point ptA = path_in.pointAt(Geom::PathTime(0, 0.0));
    Geom::Point B = path_in.pointAt(Geom::PathTime(1, 0.0));
    Geom::Curve const *first_curve = &path_in.curveAt(Geom::PathTime(0, 0.0));
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*first_curve);
    Geom::Ray ray(ptA, B);
    if (cubic) {
        ray.setPoints(ptA, (*cubic)[1]);
    }
    ray.setAngle(ray.angle() + Geom::rad_from_deg(90));
    Geom::Point knot_pos = this->knot->pos * item->i2dt_affine().inverse();
    Geom::Coord nearest_to_ray = ray.nearestTime(knot_pos);
    if(nearest_to_ray == 0){
        lpe->prop_scale.param_set_value(-Geom::distance(s , ptA)/(lpe->original_height/2.0));
    } else {
        lpe->prop_scale.param_set_value(Geom::distance(s , ptA)/(lpe->original_height/2.0));
    }
    if (!lpe->original_height) {
        lpe->prop_scale.param_set_value(0);
    }
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setDouble("/live_effects/bend_path/width", lpe->prop_scale);

    sp_lpe_item_update_patheffect (cast<SPLPEItem>(item), false, true);
}

Geom::Point 
KnotHolderEntityWidthBendPath::knot_get() const
{
    LPEBendPath *lpe = dynamic_cast<LPEBendPath *> (_effect);
    if (!lpe)
        return Geom::Point(0, 0);

    Geom::Path path_in = lpe->bend_path.get_pathvector().pathAt(Geom::PathVectorTime(0, 0, 0.0));
    Geom::Point ptA = path_in.pointAt(Geom::PathTime(0, 0.0));
    Geom::Point B = path_in.pointAt(Geom::PathTime(1, 0.0));
    Geom::Curve const *first_curve = &path_in.curveAt(Geom::PathTime(0, 0.0));
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*first_curve);
    Geom::Ray ray(ptA, B);
    if (cubic) {
        ray.setPoints(ptA, (*cubic)[1]);
    }
    ray.setAngle(ray.angle() + Geom::rad_from_deg(90));
    Geom::Point result_point = Geom::Point::polar(ray.angle(), (lpe->original_height/2.0) * lpe->prop_scale) + ptA;
    lpe->helper_path.clear();
    if (!lpe->hide_knot) {
        Geom::Path hp(result_point);
        hp.appendNew<Geom::LineSegment>(ptA);
        lpe->helper_path.push_back(hp);
        hp.clear();
    }
    return result_point;
}
} // namespace BeP
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
