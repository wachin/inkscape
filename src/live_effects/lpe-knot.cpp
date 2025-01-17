// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LPE knot effect implementation.
 */
/* Authors:
 *   Jean-Francois Barraud <jf.barraud@gmail.com>
 *   Abhishek Sharma
 *   Johan Engelen
 *
 * Copyright (C) 2007-2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gdk/gdk.h>
#include <optional>

#include <2geom/sbasis-to-bezier.h>
#include <2geom/bezier-to-sbasis.h>
#include <2geom/basic-intersection.h>

#include "lpe-knot.h"

// for change crossing undo
#include "document.h"
#include "document-undo.h"

#include "style.h"

#include "display/curve.h"

#include "helper/geom.h"

#include "object/sp-path.h"
#include "object/sp-shape.h"

#include "ui/icon-names.h"
#include "ui/knot/knot-holder.h"
#include "ui/knot/knot-holder-entity.h"


// TODO due to internal breakage in glibmm headers, this must be last:
#include <glibmm/i18n.h>

namespace Inkscape {
namespace LivePathEffect {

class KnotHolderEntityCrossingSwitcher : public LPEKnotHolderEntity {
public:
    KnotHolderEntityCrossingSwitcher(LPEKnot *effect) : LPEKnotHolderEntity(effect) {};
    void knot_set(Geom::Point const &p, Geom::Point const &origin, guint state) override;
    Geom::Point knot_get() const override;
    void knot_click(guint state) override;
};


static Geom::Path::size_type size_nondegenerate(Geom::Path const &path) {
    Geom::Path::size_type retval = path.size_default();
    const Geom::Curve &closingline = path.back_closed();
    // the closing line segment is always of type
    // Geom::LineSegment.
    if (are_near(closingline.initialPoint(), closingline.finalPoint())) {
        // closingline.isDegenerate() did not work, because it only checks for
        // *exact* zero length, which goes wrong for relative coordinates and
        // rounding errors...
        // the closing line segment has zero-length. So stop before that one!
        retval = path.size_open();
    }
    return retval;
}

//---------------------------------------------------------------------------
//LPEKnot specific Interval manipulation.
//---------------------------------------------------------------------------

//remove an interval from an union of intervals.
//TODO: is it worth moving it to 2Geom?
static
std::vector<Geom::Interval> complementOf(Geom::Interval I, std::vector<Geom::Interval> domain){
    std::vector<Geom::Interval> ret;
    if (!domain.empty()) {
        double min = domain.front().min();
        double max = domain.back().max();
        Geom::Interval I1 = Geom::Interval(min,I.min());
        Geom::Interval I2 = Geom::Interval(I.max(),max);

        for (auto i : domain){
            std::optional<Geom::Interval> I1i = intersect(i,I1);
            if (I1i && !I1i->isSingular()) ret.push_back(*I1i);
            std::optional<Geom::Interval> I2i = intersect(i,I2);
            if (I2i && !I2i->isSingular()) ret.push_back(*I2i);
        }
    }
    return ret;
}

//find the time interval during which patha is hidden by pathb near a given crossing.
// Warning: not accurate!
static
Geom::Interval
findShadowedTime(Geom::Path const &patha, std::vector<Geom::Point> const &pt_and_dir,
                 double const ta, double const width){
    using namespace Geom;
    Point T = unit_vector(pt_and_dir[1]);
    Point N = T.cw();
    //Point A = pt_and_dir[0] - 3 * width * T;
    //Point B = A+6*width*T;

    Affine mat = from_basis( T, N, pt_and_dir[0] );
    mat = mat.inverse();
    Geom::Path p = patha * mat;
    
    std::vector<double> times;
    
    //TODO: explore the path fwd/backward from ta (worth?)
    for (unsigned i = 0; i < size_nondegenerate(patha); i++){
        D2<SBasis> f = p[i].toSBasis();
        std::vector<double> times_i, temptimes;
        temptimes = roots(f[Y]-width);
        times_i.insert(times_i.end(), temptimes.begin(), temptimes.end() ); 
        temptimes = roots(f[Y]+width);
        times_i.insert(times_i.end(), temptimes.begin(), temptimes.end() ); 
        temptimes = roots(f[X]-3*(width?width:EPSILON/2.0));
        times_i.insert(times_i.end(), temptimes.begin(), temptimes.end() ); 
        temptimes = roots(f[X]+3*(width?width:EPSILON/2.0));
        times_i.insert(times_i.end(), temptimes.begin(), temptimes.end() );
        for (double & k : times_i){
            k+=i;
        }
        times.insert(times.end(), times_i.begin(), times_i.end() );
    }
    std::sort( times.begin(),  times.end() );
    std::vector<double>::iterator new_end = std::unique( times.begin(),  times.end() );
    times.resize( new_end - times.begin() );

    double tmin = 0, tmax = size_nondegenerate(patha);
    double period = size_nondegenerate(patha);
    if (!times.empty()){
        unsigned rk = upper_bound( times.begin(),  times.end(), ta ) - times.begin();
        if ( rk < times.size() ) 
            tmax = times[rk];
        else if ( patha.closed() ) 
            tmax = times[0]+period;

        if ( rk > 0 ) 
            tmin = times[rk-1];
        else if ( patha.closed() ) 
            tmin = times.back()-period;
    }
    return Interval(tmin,tmax);
}

//---------------------------------------------------------------------------
//LPEKnot specific Crossing Data manipulation.
//---------------------------------------------------------------------------

//Yet another crossing data representation.
// an CrossingPoint stores
//    -an intersection point
//    -the involved path components
//    -for each component, the time at which this crossing occurs + the order of this crossing along the component (when starting from 0).

namespace LPEKnotNS {//just in case...
CrossingPoints::CrossingPoints(Geom::PathVector const &paths) : std::vector<CrossingPoint>(){
//    std::cout<<"\nCrossingPoints creation from path vector\n";
    for( unsigned i=0; i<paths.size(); i++){
        for( unsigned ii=0; ii < size_nondegenerate(paths[i]); ii++){
            for( unsigned j=i; j<paths.size(); j++){
                for( unsigned jj=(i==j?ii:0); jj < size_nondegenerate(paths[j]); jj++){
                    std::vector<std::pair<double,double> > times;
                    if ( (i==j) && (ii==jj) ) {

//                         std::cout<<"--(self int)\n";
//                         std::cout << paths[i][ii].toSBasis()[Geom::X] <<"\n";
//                         std::cout << paths[i][ii].toSBasis()[Geom::Y] <<"\n";

                        find_self_intersections( times, paths[i][ii].toSBasis() );
                    } else {
//                         std::cout<<"--(pair int)\n";
//                         std::cout << paths[i][ii].toSBasis()[Geom::X] <<"\n";
//                         std::cout << paths[i][ii].toSBasis()[Geom::Y] <<"\n";
//                         std::cout<<"with\n";
//                         std::cout << paths[j][jj].toSBasis()[Geom::X] <<"\n";
//                         std::cout << paths[j][jj].toSBasis()[Geom::Y] <<"\n";

                        find_intersections( times, paths[i][ii].toSBasis(), paths[j][jj].toSBasis() );
                    }
                    for (auto & time : times){
                        //std::cout<<"intersection "<<i<<"["<<ii<<"]("<<times[k].first<<")= "<<j<<"["<<jj<<"]("<<times[k].second<<")\n";
                        if ( !std::isnan(time.first) && !std::isnan(time.second) ){
                            double zero = 1e-4;
                            if ( (i==j) && (fabs(time.first+ii - time.second-jj) <= zero) )
                            { //this is just end=start of successive curves in a path.
                                continue;
                            }
                            if ( (i==j) && (ii == 0) && (jj == size_nondegenerate(paths[i])-1)
                                 && paths[i].closed()
                                 && (fabs(time.first) <= zero)
                                 && (fabs(time.second - 1) <= zero) )
                            {//this is just end=start of a closed path.
                                continue;
                            }
                            CrossingPoint cp;
                            cp.pt = paths[i][ii].pointAt(time.first);
                            cp.sign = 1;
                            cp.i = i;
                            cp.j = j;
                            cp.ni = 0; cp.nj=0;//not set yet
                            cp.ti = time.first + ii;
                            cp.tj = time.second + jj;
                            push_back(cp);
                        }else{
                            std::cerr<<"ooops: find_(self)_intersections returned NaN:" << std::endl;
                            //std::cout<<"intersection "<<i<<"["<<ii<<"](NaN)= "<<j<<"["<<jj<<"](NaN)\n";
                        }
                    }
                }
            }
        }
    }
    for( unsigned i=0; i<paths.size(); i++){
        std::map < double, unsigned > cuts;
        for( unsigned k=0; k<size(); k++){
            CrossingPoint cp = (*this)[k];
            if (cp.i == i) cuts[cp.ti] = k;
            if (cp.j == i) cuts[cp.tj] = k;
        }
        unsigned count = 0;
        for (auto & cut : cuts){
            if ( ((*this)[cut.second].i == i) && ((*this)[cut.second].ti == cut.first) ){
                (*this)[cut.second].ni = count;
            }else{
                (*this)[cut.second].nj = count;
            }
            count++;
        }
    }
}

CrossingPoints::CrossingPoints(std::vector<double> const &input) : std::vector<CrossingPoint>()
{
    if ( (input.size() > 0) && (input.size()%9 == 0) ){
        using namespace Geom;
        for( unsigned n=0; n<input.size();  ){
            CrossingPoint cp;
            cp.pt[X] = input[n++];
            cp.pt[Y] = input[n++];
            cp.i = input[n++];
            cp.j = input[n++];
            cp.ni = input[n++];
            cp.nj = input[n++];
            cp.ti = input[n++];
            cp.tj = input[n++];
            cp.sign = input[n++];
            push_back(cp);
        }
    }
}

std::vector<double>
CrossingPoints::to_vector()
{
    using namespace Geom;
    std::vector<double> result;
    for( unsigned n=0; n<size(); n++){
        CrossingPoint cp = (*this)[n];
        result.push_back(cp.pt[X]);
        result.push_back(cp.pt[Y]);
        result.push_back(double(cp.i));
        result.push_back(double(cp.j));
        result.push_back(double(cp.ni));
        result.push_back(double(cp.nj));
        result.push_back(double(cp.ti));
        result.push_back(double(cp.tj));
        result.push_back(double(cp.sign));
    }
    return result;
}

//FIXME: rewrite to check success: return bool, put result in arg.
CrossingPoint
CrossingPoints::get(unsigned const i, unsigned const ni)
{
    for (unsigned k=0; k<size(); k++){
        if (    ( ((*this)[k].i==i) && ((*this)[k].ni==ni) )
             || ( ((*this)[k].j==i) && ((*this)[k].nj==ni) )  )
        {
            return (*this)[k];
        }
    }
    g_warning("LPEKnotNS::CrossingPoints::get error. %uth crossing along string %u not found.",ni,i);
    assert(false);//debug purpose...
    return CrossingPoint();
}

static unsigned
idx_of_nearest(CrossingPoints const &cpts, Geom::Point const &p)
{
    double dist=-1;
    unsigned result = cpts.size();
    for (unsigned k=0; k<cpts.size(); k++){
        double dist_k = Geom::L2(p-cpts[k].pt);
        if ( (dist < 0) || (dist > dist_k) ) {
            result = k;
            dist = dist_k;
        }
    }
    return result;
}

//TODO: Find a way to warn the user when the topology changes.
//TODO: be smarter at guessing the signs when the topology changed?
void
CrossingPoints::inherit_signs(CrossingPoints const &other, int default_value)
{
    bool topo_changed = false;
    for (unsigned n=0; n < size(); n++){
        if ( (n < other.size())
             && (other[n].i  == (*this)[n].i)
             && (other[n].j  == (*this)[n].j)
             && (other[n].ni == (*this)[n].ni)
             && (other[n].nj == (*this)[n].nj) )
        {
            (*this)[n].sign = other[n].sign;
        } else {
            topo_changed = true;
            break;
        }
    }
    if (topo_changed) {
        //TODO: Find a way to warn the user!!
//        std::cout<<"knot topolgy changed!\n";
        for (unsigned n=0; n < size(); n++){
            Geom::Point p = (*this)[n].pt;
            unsigned idx = idx_of_nearest(other,p);
            if (idx < other.size()) {
                (*this)[n].sign = other[idx].sign;
            } else {
                (*this)[n].sign = default_value;
            }
        }
    }
}

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//LPEKnot effect.
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------


LPEKnot::LPEKnot(LivePathEffectObject *lpeobject)
    : Effect(lpeobject)
    ,
    // initialise your parameters here:
    interruption_width(_("_Gap length:"), _("Size of hidden region of lower string"), "interruption_width", &wr, this,
                       3)
    , prop_to_stroke_width(
          _("_In units of stroke width"),
          _("Gap width is given in multiples of stroke width. When unchecked, document units are used."),
          "prop_to_stroke_width", &wr, this, true)
    , both(_("_Gaps in both"), _("At path intersections, both parts will have a gap"), "both", &wr, this, false)
    , inverse_width(_("_Groups: Inverse"), _("Use other stroke width, useful in groups with different stroke widths"),
                    "inverse_width", &wr, this, false)
    , add_stroke_width("St_roke width", "Add the stroke width to the gap size", "add_stroke_width", &wr, this,
                       "inkscape_1.0_and_up", true)
    , add_other_stroke_width("_Crossing path stroke width", "Add crossed stroke width to the gap size",
                             "add_other_stroke_width", &wr, this, "inkscape_1.0_and_up", true)
    , switcher_size(_("S_witcher size:"), _("Orientation indicator/switcher size"), "switcher_size", &wr, this, 15)
    , crossing_points_vector(_("Crossing Signs"), _("Crossing signs"), "crossing_points_vector", &wr, this)
    , crossing_points()
    , gpaths()
    , gstroke_widths()
    , selectedCrossing(0)
    , switcher(0., 0.)
{
    // register all your parameters here, so Inkscape knows which parameters this effect has:
    registerParameter(&switcher_size);
    registerParameter(&interruption_width);
    registerParameter(&prop_to_stroke_width);
    registerParameter(&add_stroke_width);
    registerParameter(&both);
    registerParameter(&inverse_width);
    registerParameter(&add_other_stroke_width);
    registerParameter(&crossing_points_vector);

    _provides_knotholder_entities = true;
}

LPEKnot::~LPEKnot() = default;

void
LPEKnot::updateSwitcher(){
    if (selectedCrossing < crossing_points.size()){
        switcher = crossing_points[selectedCrossing].pt;
        //std::cout<<"placing switcher at "<<switcher<<" \n";
    }else if (crossing_points.size()>0){
        selectedCrossing = 0;
        switcher = crossing_points[selectedCrossing].pt;
        //std::cout<<"placing switcher at "<<switcher<<" \n";
    }else{
        //std::cout<<"hiding switcher!\n";
        switcher = Geom::Point(Geom::infinity(),Geom::infinity());
    }
}

Geom::PathVector
LPEKnot::doEffect_path (Geom::PathVector const &path_in)
{
    using namespace Geom;
    Geom::PathVector path_out;

    if (gpaths.size()==0){
        return path_in;
    }
    Geom::PathVector const original_pathv = pathv_to_linear_and_cubic_beziers(path_in);
    for (const auto & comp : original_pathv){

        //find the relevant path component in gpaths (required to allow groups!)
        //Q: do we always receive the group members in the same order? can we rest on that?
        unsigned i0 = 0;
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        gint precision = prefs->getInt("/options/svgoutput/numericprecision");
        prefs->setInt("/options/svgoutput/numericprecision", 4); // I think this is enough for minor differences
        for (i0=0; i0<gpaths.size(); i0++){
            if (sp_svg_write_path(comp) == sp_svg_write_path(gpaths[i0]))
                break;
        }
        prefs->setInt("/options/svgoutput/numericprecision", precision);
        if (i0 == gpaths.size() ) {THROW_EXCEPTION("lpe-knot error: group member not recognized");}// this should not happen...

        std::vector<Interval> dom;
        dom.emplace_back(0., size_nondegenerate(gpaths[i0]));
        for (auto & crossing_point : crossing_points){
            if ( (crossing_point.i == i0) || (crossing_point.j == i0) ) {
                unsigned i = crossing_point.i;
                unsigned j = crossing_point.j;
                double ti = crossing_point.ti;
                double tj = crossing_point.tj;
                
                double curveidx, t;
                
                t = modf(ti, &curveidx);
                if(curveidx == size_nondegenerate(gpaths[i]) ) { curveidx--; t = 1.;}
                assert(curveidx >= 0 && curveidx < size_nondegenerate(gpaths[i]));
                std::vector<Point> flag_i = gpaths[i][curveidx].pointAndDerivatives(t,1);

                t = modf(tj, &curveidx);
                if(curveidx == size_nondegenerate(gpaths[j]) ) { curveidx--; t = 1.;}
                assert(curveidx >= 0 && curveidx < size_nondegenerate(gpaths[j]));
                std::vector<Point> flag_j = gpaths[j][curveidx].pointAndDerivatives(t,1);


                int geom_sign = ( cross(flag_i[1], flag_j[1]) < 0 ? 1 : -1);
                bool i0_is_under = false;
                double width = interruption_width;
                if ( crossing_point.sign * geom_sign > 0 ){
                    i0_is_under = ( i == i0 );
                }
                else if (crossing_point.sign * geom_sign < 0) {
                    if (j == i0){
                        i0_is_under = true;
                    }
                }
                i0_is_under = crossing_point.sign != 0 && both ? true : i0_is_under;
                if (i0_is_under && j == i0) {
                    // last check of sign makes sure we get different outputs when
                    // path components are part of the same subpath (i == j)
                    if (!(i == j && !both && crossing_point.sign * geom_sign > 0)) {
                        std::swap(i, j);
                        std::swap(ti, tj);
                        std::swap(flag_i, flag_j);
                    }
                }
                if (i0_is_under){
                    if ( prop_to_stroke_width.get_value() ) {
                        if (inverse_width) {
                            width *= gstroke_widths[j];
                        }
                        else {
                            width *= gstroke_widths[i];
                        }
                    }
                    if (add_stroke_width.get_value() == "true") {
                        width += gstroke_widths[i];
                    }
                    if (add_other_stroke_width.get_value() == "true") {
                        width += gstroke_widths[j];
                    }
                    Interval hidden = findShadowedTime(gpaths[i0], flag_j, ti, width/2);
                    double period  = size_nondegenerate(gpaths[i0]);
                    if (hidden.max() > period ) hidden -= period;
                    if (hidden.min()<0){
                        dom = complementOf( Interval(0,hidden.max()) ,dom);
                        dom = complementOf( Interval(hidden.min()+period, period) ,dom);
                    }else{
                        dom = complementOf(hidden,dom);
                    }
                    if (crossing_point.i == i0 && crossing_point.j == i0 && crossing_point.sign != 0 &&
                        both) {
                        hidden = findShadowedTime(gpaths[i0], flag_i, tj, width / 2);
                        period = size_nondegenerate(gpaths[i0]);
                        if (hidden.max() > period)
                            hidden -= period;
                        if (hidden.min() < 0) {
                            dom = complementOf(Interval(0, hidden.max()), dom);
                            dom = complementOf(Interval(hidden.min() + period, period), dom);
                        }
                        else {
                            dom = complementOf(hidden, dom);
                        }
                    }
                }
            }
        }

        //If the all component is hidden, continue.
        if (dom.empty()){
            continue;
        }

        //If the current path is closed and the last/first point is still there, glue first and last piece.
        unsigned beg_comp = 0, end_comp = dom.size();
        if ( gpaths[i0].closed() && (dom.front().min() == 0) && (dom.back().max() == size_nondegenerate(gpaths[i0])) ) {
            if ( dom.size() == 1){
                path_out.push_back(gpaths[i0]);
                continue;
            }else{
                // std::cout<<"fusing first and last component\n";
                ++beg_comp;
                --end_comp;
                Geom::Path first = gpaths[i0].portion(dom.back());
                //FIXME: stitching should not be necessary (?!?)
                first.setStitching(true);
                first.append(gpaths[i0].portion(dom.front()));
                path_out.push_back(first);
            }
        }
        for (unsigned comp = beg_comp; comp < end_comp; comp++){
            assert(dom.at(comp).min() >=0 && dom.at(comp).max() <= size_nondegenerate(gpaths.at(i0)));
            path_out.push_back(gpaths[i0].portion(dom.at(comp)));
        }
    }
    return path_out;
}



//recursively collect gpaths and stroke widths (stolen from "sp-lpe_item.cpp").
static void
collectPathsAndWidths (SPLPEItem const *lpeitem, Geom::PathVector &paths, std::vector<double> &stroke_widths){
    auto lpeitem_mutable = const_cast<SPLPEItem *>(lpeitem);

    if (auto group = cast<SPGroup>(lpeitem_mutable)) {
        std::vector<SPItem*> item_list = group->item_list();
        for (auto subitem : item_list) {
            if (is<SPLPEItem>(subitem)) {
                collectPathsAndWidths(cast<SPLPEItem>(subitem), paths, stroke_widths);
            }
        }
    } else if (auto shape = cast<SPShape>(lpeitem)) {
        SPCurve const *c = shape->curve();
        if (c) {
            Geom::PathVector subpaths = pathv_to_linear_and_cubic_beziers(c->get_pathvector());
            for (const auto & subpath : subpaths){
                paths.push_back(subpath);
                //FIXME: do we have to be more careful when trying to access stroke width?
                stroke_widths.push_back(lpeitem->style->stroke_width.computed);
            }
        }
    }
}


void
LPEKnot::doBeforeEffect (SPLPEItem const* lpeitem)
{
    using namespace Geom;
    original_bbox(lpeitem);
    
    if (is<SPPath>(lpeitem)) {
        supplied_path = cast<SPPath>(lpeitem)->curve()->get_pathvector();
    }

    gpaths.clear();
    gstroke_widths.clear();

    collectPathsAndWidths(lpeitem, gpaths, gstroke_widths);

//     std::cout<<"\nPaths on input:\n";
//     for (unsigned i=0; i<gpaths.size(); i++){
//         for (unsigned ii=0; ii<gpaths[i].size(); ii++){
//             std::cout << gpaths[i][ii].toSBasis()[Geom::X] <<"\n";
//             std::cout << gpaths[i][ii].toSBasis()[Geom::Y] <<"\n";
//             std::cout<<"--\n";
//         }
//     }
                        
    //std::cout<<"crossing_pts_vect: "<<crossing_points_vector.param_getSVGValue()<<".\n";
    //std::cout<<"prop_to_stroke_width: "<<prop_to_stroke_width.param_getSVGValue()<<".\n";

    LPEKnotNS::CrossingPoints old_crdata(crossing_points_vector.data());

//     std::cout<<"\nVectorParam size:"<<crossing_points_vector.data().size()<<"\n";

//     std::cout<<"\nOld crdata ("<<old_crdata.size()<<"): \n";
//     for (unsigned toto=0; toto<old_crdata.size(); toto++){
//         std::cout<<"(";
//         std::cout<<old_crdata[toto].i<<",";
//         std::cout<<old_crdata[toto].j<<",";
//         std::cout<<old_crdata[toto].ni<<",";
//         std::cout<<old_crdata[toto].nj<<",";
//         std::cout<<old_crdata[toto].ti<<",";
//         std::cout<<old_crdata[toto].tj<<",";
//         std::cout<<old_crdata[toto].sign<<"),";
//     }

    //if ( old_crdata.size() > 0 ) std::cout<<"first crossing sign = "<<old_crdata[0].sign<<".\n";
    //else std::cout<<"old data is empty!!\n";
    crossing_points = LPEKnotNS::CrossingPoints(gpaths);
//     std::cout<<"\nNew crdata ("<<crossing_points.size()<<"): \n";
//     for (unsigned toto=0; toto<crossing_points.size(); toto++){
//         std::cout<<"(";
//         std::cout<<crossing_points[toto].i<<",";
//         std::cout<<crossing_points[toto].j<<",";
//         std::cout<<crossing_points[toto].ni<<",";
//         std::cout<<crossing_points[toto].nj<<",";
//         std::cout<<crossing_points[toto].ti<<",";
//         std::cout<<crossing_points[toto].tj<<",";
//         std::cout<<crossing_points[toto].sign<<"),";
//     }
    crossing_points.inherit_signs(old_crdata);

    // Don't write to XML here, only store it in the param itself. Will be written to SVG later
    crossing_points_vector.param_setValue(crossing_points.to_vector());

    updateSwitcher();
}

void
LPEKnot::addCanvasIndicators(SPLPEItem const */*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
    using namespace Geom;
    double r = switcher_size*.1;
    char const * svgd;
    //TODO: use a nice path!
    if ( (selectedCrossing >= crossing_points.size()) || (crossing_points[selectedCrossing].sign > 0) ) {
        //svgd = "M -10,0 A 10 10 0 1 0 0,-10 l  5,-1 -1,2";
        svgd = "m -7.07,7.07 c 3.9,3.91 10.24,3.91 14.14,0 3.91,-3.9 3.91,-10.24 0,-14.14 -3.9,-3.91 -10.24,-3.91 -14.14,0 l 2.83,-4.24 0.7,2.12";
    } else if (crossing_points[selectedCrossing].sign < 0) {
        //svgd = "M  10,0 A 10 10 0 1 1 0,-10 l -5,-1  1,2";
        svgd = "m 7.07,7.07 c -3.9,3.91 -10.24,3.91 -14.14,0 -3.91,-3.9 -3.91,-10.24 0,-14.14 3.9,-3.91 10.24,-3.91 14.14,0 l -2.83,-4.24 -0.7,2.12";
    } else {
        //svgd = "M 10,0 A 10 10 0 1 0 -10,0 A 10 10 0 1 0 10,0 ";
        svgd = "M 10,0 C 10,5.52 5.52,10 0,10 -5.52,10 -10,5.52 -10,0 c 0,-5.52 4.48,-10 10,-10 5.52,0 10,4.48 10,10 z";
    }
    PathVector pathv = sp_svg_read_pathv(svgd);
    pathv *= Affine(r,0,0,r,0,0) * Translate(switcher);
    hp_vec.push_back(pathv);
}

void LPEKnot::addKnotHolderEntities(KnotHolder *knotholder, SPItem *item)
{
    KnotHolderEntity *e = new KnotHolderEntityCrossingSwitcher(this);
    e->create(nullptr, item, knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:CrossingSwitcher",
              _("Drag to select a crossing, click to flip it, Shift + click to change all crossings, Ctrl + click to "
                "reset and change all crossings"));
    knotholder->add(e);
};


void
KnotHolderEntityCrossingSwitcher::knot_set(Geom::Point const &p, Geom::Point const &/*origin*/, guint /*state*/)
{
    LPEKnot* lpe = dynamic_cast<LPEKnot *>(_effect);

    lpe->selectedCrossing = idx_of_nearest(lpe->crossing_points,p);
    lpe->updateSwitcher();
    // FIXME: this should not directly ask for updating the item. It should write to SVG, which triggers updating.
    sp_lpe_item_update_patheffect (cast<SPLPEItem>(item), false, true);
}

Geom::Point
KnotHolderEntityCrossingSwitcher::knot_get() const
{
    LPEKnot const *lpe = dynamic_cast<LPEKnot const*>(_effect);
    return lpe->switcher;
}

void
KnotHolderEntityCrossingSwitcher::knot_click(guint state)
{
    LPEKnot* lpe = dynamic_cast<LPEKnot *>(_effect);
    unsigned s = lpe->selectedCrossing;
    if (s < lpe->crossing_points.size()){
        if (state & GDK_SHIFT_MASK){
            for (auto & crossing_point : lpe->crossing_points) {
                crossing_point.sign = ((crossing_point.sign + 2) % 3) - 1;
            }
        }
        else if (state & GDK_CONTROL_MASK) {
            int sign = lpe->crossing_points[s].sign;
            for (auto & crossing_point : lpe->crossing_points) {
                crossing_point.sign = ((sign + 2) % 3) - 1;
            }
        }else{
            int sign = lpe->crossing_points[s].sign;
            lpe->crossing_points[s].sign = ((sign+2)%3)-1;
            //std::cout<<"crossing set to"<<lpe->crossing_points[s].sign<<".\n";
        }
        lpe->crossing_points_vector.param_set_and_write_new_value(lpe->crossing_points.to_vector());
        lpe->makeUndoDone(_("Change knot crossing"));
        // FIXME: this should not directly ask for updating the item. It should write to SVG, which triggers updating.
//        sp_lpe_item_update_patheffect (cast<SPLPEItem>(item), false, true);
    }
}


/* ######################## */

} // namespace LivePathEffect
} // namespace Inkscape

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

