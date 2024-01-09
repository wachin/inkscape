// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Our nice measuring tool
 *
 * Authors:
 *   Felipe Correa da Silva Sanches <juca@members.fsf.org>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Jabiertxo Arraiza <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2011 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "measure-tool.h"

#include <iomanip>

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include <boost/none_t.hpp>

#include <2geom/line.h>
#include <2geom/path-intersection.h>

#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "layer-manager.h"
#include "page-manager.h"
#include "path-chemistry.h"
#include "rubberband.h"
#include "text-editing.h"

#include "display/curve.h"
#include "display/control/canvas-item-curve.h"
#include "display/control/canvas-item-ctrl.h"
#include "display/control/canvas-item-group.h"
#include "display/control/canvas-item-text.h"

#include "object/sp-defs.h"
#include "object/sp-flowtext.h"
#include "object/sp-namedview.h"
#include "object/sp-root.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"

#include "svg/stringstream.h"
#include "svg/svg-color.h"
#include "svg/svg.h"

#include "ui/dialog/knot-properties.h"
#include "ui/icon-names.h"
#include "ui/knot/knot.h"
#include "ui/tools/freehand-base.h"
#include "ui/widget/canvas.h" // Canvas area

#include "util/units.h"

using Inkscape::Util::unit_table;
using Inkscape::DocumentUndo;

const guint32 MT_KNOT_COLOR_NORMAL = 0xffffff00;
const guint32 MT_KNOT_COLOR_MOUSEOVER = 0xff000000;


namespace Inkscape {
namespace UI {
namespace Tools {

namespace {

/**
 * Simple class to use for removing label overlap.
 */
class LabelPlacement {
public:

    double lengthVal;
    double offset;
    Geom::Point start;
    Geom::Point end;
};

bool SortLabelPlacement(LabelPlacement const &first, LabelPlacement const &second)
{
    if (first.end[Geom::Y] == second.end[Geom::Y]) {
        return first.end[Geom::X] < second.end[Geom::X];
    } else {
        return first.end[Geom::Y] < second.end[Geom::Y];
    }
}

//precision is for give the number of decimal positions
//of the label to calculate label width
void repositionOverlappingLabels(std::vector<LabelPlacement> &placements, SPDesktop *desktop, Geom::Point const &normal, double fontsize, int precision)
{
    std::sort(placements.begin(), placements.end(), SortLabelPlacement);

    double border = 3;
    Geom::Rect box;
    {
        Geom::Point tmp(fontsize * (6 + precision) + (border * 2), fontsize + (border * 2));
        tmp = desktop->w2d(tmp);
        box = Geom::Rect(-tmp[Geom::X] / 2, -tmp[Geom::Y] / 2, tmp[Geom::X] / 2, tmp[Geom::Y] / 2);
    }

    // Using index since vector may be re-ordered as we go.
    // Starting at one, since the first item can't overlap itself
    for (size_t i = 1; i < placements.size(); i++) {
        LabelPlacement &place = placements[i];

        bool changed = false;
        do {
            Geom::Rect current(box + place.end);

            changed = false;
            bool overlaps = false;
            for (size_t j = i; (j > 0) && !overlaps; --j) {
                LabelPlacement &otherPlace = placements[j - 1];
                Geom::Rect target(box + otherPlace.end);
                if (current.intersects(target)) {
                    overlaps = true;
                }
            }
            if (overlaps) {
                place.offset += (fontsize + border);
                place.end = place.start - desktop->w2d(normal * place.offset);
                changed = true;
            }
        } while (changed);

        std::sort(placements.begin(), placements.begin() + i + 1, SortLabelPlacement);
    }
}

/**
 * Calculates where to place the anchor for the display text and arc.
 *
 * @param desktop the desktop that is being used.
 * @param angle the angle to be displaying.
 * @param baseAngle the angle of the initial baseline.
 * @param startPoint the point that is the vertex of the selected angle.
 * @param endPoint the point that is the end the user is manipulating for measurement.
 * @param fontsize the size to display the text label at.
 */
Geom::Point calcAngleDisplayAnchor(SPDesktop *desktop, double angle, double baseAngle,
                                   Geom::Point const &startPoint, Geom::Point const &endPoint,
                                   double fontsize)
{
    // Time for the trick work of figuring out where things should go, and how.
    double lengthVal = (endPoint - startPoint).length();
    double effective = baseAngle + (angle / 2);
    Geom::Point where(lengthVal, 0);
    where *= Geom::Affine(Geom::Rotate(effective)) * Geom::Affine(Geom::Translate(startPoint));

    // When the angle is tight, the label would end up under the cursor and/or lines. Bump it
    double scaledFontsize = std::abs(fontsize * desktop->w2d(Geom::Point(0, 1.0))[Geom::Y]);
    if (std::abs((where - endPoint).length()) < scaledFontsize) {
        where[Geom::Y] += scaledFontsize * 2;
    }

    // We now have the ideal position, but need to see if it will fit/work.

    Geom::Rect screen_world = desktop->getCanvas()->get_area_world();
    if (screen_world.interiorContains(desktop->d2w(startPoint)) ||
        screen_world.interiorContains(desktop->d2w(endPoint))) {
        screen_world.expandBy(fontsize * -3, fontsize / -2);
        where = desktop->w2d(screen_world.clamp(desktop->d2w(where)));
    } // else likely initialized the measurement tool, keep display near the measurement.

    return where;
}

} // namespace

/**
 * Given an angle, the arc center and edge point, draw an arc segment centered around that edge point.
 *
 * @param desktop the desktop that is being used.
 * @param center the center point for the arc.
 * @param end the point that ends at the edge of the arc segment.
 * @param anchor the anchor point for displaying the text label.
 * @param angle the angle of the arc segment to draw.
 * @param measure_rpr the container of the curve if converted to items.
 *
 */
void MeasureTool::createAngleDisplayCurve(Geom::Point const &center, Geom::Point const &end, Geom::Point const &anchor,
                                          double angle, bool to_phantom,
                                          Inkscape::XML::Node *measure_repr)
{
    // Given that we have a point on the arc's edge and the angle of the arc, we need to get the two endpoints.

    double textLen = std::abs((anchor - center).length());
    double sideLen = std::abs((end - center).length());
    if (sideLen > 0.0) {
        double factor = std::min(1.0, textLen / sideLen);

        // arc start
        Geom::Point p1 = end * (Geom::Affine(Geom::Translate(-center))
                                * Geom::Affine(Geom::Scale(factor))
                                * Geom::Affine(Geom::Translate(center)));

        // arc end
        Geom::Point p4 = p1 * (Geom::Affine(Geom::Translate(-center))
                               * Geom::Affine(Geom::Rotate(-angle))
                               * Geom::Affine(Geom::Translate(center)));

        // from Riskus
        double xc = center[Geom::X];
        double yc = center[Geom::Y];
        double ax = p1[Geom::X] - xc;
        double ay = p1[Geom::Y] - yc;
        double bx = p4[Geom::X] - xc;
        double by = p4[Geom::Y] - yc;
        double q1 = (ax * ax) + (ay * ay);
        double q2 = q1 + (ax * bx) + (ay * by);

        double k2;

        /*
         * The denominator of the expression for k2 can become 0, so this should be handled.
         * The function for k2 tends to a limit for very small values of (ax * by) - (ay * bx), so theoretically
         * it should be correct for values close to 0, however due to floating point inaccuracies this
         * is not the case, and instabilities still exist. Therefore do a range check on the denominator.
         * (This also solves some instances where again due to floating point inaccuracies, the square root term
         * becomes slightly negative in case of very small values for ax * by - ay * bx).
         * The values of this range have been generated by trying to make this term as small as possible,
         * by zooming in as much as possible in the GUI, using the measurement tool and
         * trying to get as close to 180 or 0 degrees as possible.
         * Smallest value I was able to get was around 1e-5, and then I added some zeroes for good measure.
         */
        if (!((ax * by - ay * bx < 0.00000000001) && (ax * by - ay * bx > -0.00000000001))) {
            k2 = (4.0 / 3.0) * (std::sqrt(2 * q1 * q2) - q2) / ((ax * by) - (ay * bx));
        } else {
            // If the denominator is 0, there are 2 cases:
            // Either the angle is (almost) +-180 degrees, in which case the limit of k2 tends to -+4.0/3.0.
            if (angle > 3.14 || angle < -3.14) { // The angle is in radians
                // Now there are also 2 cases, where inkscape thinks it is 180 degrees, or -180 degrees.
                // Adjust the value of k2 accordingly
                if (angle > 0) {
                    k2 = -4.0 / 3.0;
                } else {
                    k2 = 4.0 / 3.0;
                }
            } else {
                // if the angle is (almost) 0, k2 is equal to 0
                k2 = 0.0;
            }
        }

        Geom::Point p2(xc + ax - (k2 * ay),
                       yc + ay  + (k2 * ax));
        Geom::Point p3(xc + bx + (k2 * by),
                       yc + by - (k2 * bx));

        auto *curve = new Inkscape::CanvasItemCurve(_desktop->getCanvasTemp(), p1, p2, p3, p4);
        curve->set_name("CanvasItemCurve:MeasureToolCurve");
        curve->set_stroke(Inkscape::CANVAS_ITEM_SECONDARY);
        curve->lower_to_bottom();
        curve->show();
        if(to_phantom){
            curve->set_stroke(0x8888887f);
            measure_phantom_items.emplace_back(curve);
        } else {
            measure_tmp_items.emplace_back(curve);
        }

        if(measure_repr) {
            Geom::PathVector pathv;
            Geom::Path path;
            path.start(_desktop->doc2dt(p1));
            path.appendNew<Geom::CubicBezier>(_desktop->doc2dt(p2), _desktop->doc2dt(p3), _desktop->doc2dt(p4));
            pathv.push_back(path);
            auto layer = _desktop->layerManager().currentLayer();
            pathv *= layer->i2doc_affine().inverse();
            if(!pathv.empty()) {
                setMeasureItem(pathv, true, false, 0xff00007f, measure_repr);
            }
        }
    }
}

std::optional<Geom::Point> explicit_base_tmp = std::nullopt;

MeasureTool::MeasureTool(SPDesktop *desktop)
    : ToolBase(desktop, "/tools/measure", "measure.svg")
{
    start_p = readMeasurePoint(true);
    end_p = readMeasurePoint(false);

    // create the knots
    this->knot_start = new SPKnot(desktop, _("Measure start, <b>Shift+Click</b> for position dialog"),
                                  Inkscape::CANVAS_ITEM_CTRL_TYPE_SHAPER, "CanvasItemCtrl:MeasureTool");
    this->knot_start->setMode(Inkscape::CANVAS_ITEM_CTRL_MODE_XOR);
    this->knot_start->setFill(MT_KNOT_COLOR_NORMAL, MT_KNOT_COLOR_MOUSEOVER, MT_KNOT_COLOR_MOUSEOVER, MT_KNOT_COLOR_MOUSEOVER);
    this->knot_start->setStroke(0x0000007f, 0x0000007f, 0x0000007f, 0x0000007f);
    this->knot_start->setShape(Inkscape::CANVAS_ITEM_CTRL_SHAPE_CIRCLE);
    this->knot_start->updateCtrl();
    this->knot_start->moveto(start_p);
    this->knot_start->show();

    this->knot_end = new SPKnot(desktop, _("Measure end, <b>Shift+Click</b> for position dialog"),
                                Inkscape::CANVAS_ITEM_CTRL_TYPE_SHAPER, "CanvasItemCtrl:MeasureTool");
    this->knot_end->setMode(Inkscape::CANVAS_ITEM_CTRL_MODE_XOR);
    this->knot_end->setFill(MT_KNOT_COLOR_NORMAL, MT_KNOT_COLOR_MOUSEOVER, MT_KNOT_COLOR_MOUSEOVER, MT_KNOT_COLOR_MOUSEOVER);
    this->knot_end->setStroke(0x0000007f, 0x0000007f, 0x0000007f, 0x0000007f);
    this->knot_end->setShape(Inkscape::CANVAS_ITEM_CTRL_SHAPE_CIRCLE);
    this->knot_end->updateCtrl();
    this->knot_end->moveto(end_p);
    this->knot_end->show();

    showCanvasItems();

    this->_knot_start_moved_connection = this->knot_start->moved_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotStartMovedHandler));
    this->_knot_start_click_connection = this->knot_start->click_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotClickHandler));
    this->_knot_start_ungrabbed_connection = this->knot_start->ungrabbed_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotUngrabbedHandler));
    this->_knot_end_moved_connection = this->knot_end->moved_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotEndMovedHandler));
    this->_knot_end_click_connection = this->knot_end->click_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotClickHandler));
    this->_knot_end_ungrabbed_connection = this->knot_end->ungrabbed_signal.connect(sigc::mem_fun(*this, &MeasureTool::knotUngrabbedHandler));

}

MeasureTool::~MeasureTool()
{
    this->enableGrDrag(false);
    ungrabCanvasEvents();

    this->_knot_start_moved_connection.disconnect();
    this->_knot_start_ungrabbed_connection.disconnect();
    this->_knot_end_moved_connection.disconnect();
    this->_knot_end_ungrabbed_connection.disconnect();

    /* unref should call destroy */
    knot_unref(this->knot_start);
    knot_unref(this->knot_end);

    measure_tmp_items.clear();
    measure_item.clear();
    measure_phantom_items.clear();
}

static char const *endpoint_to_pref(bool is_start)
{
    return is_start ? "/tools/measure/measure-start" : "/tools/measure/measure-end";
}

Geom::Point MeasureTool::readMeasurePoint(bool is_start)
{
    return Preferences::get()->getPoint(endpoint_to_pref(is_start), Geom::Point(Geom::infinity(), Geom::infinity()));
}

void MeasureTool::writeMeasurePoint(Geom::Point point, bool is_start)
{
    Preferences::get()->setPoint(endpoint_to_pref(is_start), point);
}

//This function is used to reverse the Measure, I do it in two steps because when
//we move the knot the start_ or the end_p are overwritten so I need the original values.
void MeasureTool::reverseKnots()
{
    Geom::Point start = start_p;
    Geom::Point end = end_p;
    this->knot_start->moveto(end);
    this->knot_start->show();
    this->knot_end->moveto(start);
    this->knot_end->show();
    start_p = end;
    end_p = start;
    this->showCanvasItems();
}

void MeasureTool::knotClickHandler(SPKnot *knot, guint state)
{
    if (state & GDK_SHIFT_MASK) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        Glib::ustring const unit_name =  prefs->getString("/tools/measure/unit", "px");
        explicit_base = explicit_base_tmp;
        Inkscape::UI::Dialogs::KnotPropertiesDialog::showDialog(_desktop, knot, unit_name);
    }
}

void MeasureTool::knotStartMovedHandler(SPKnot */*knot*/, Geom::Point const &ppointer, guint state)
{
    Geom::Point point = this->knot_start->position();
    if (state & GDK_CONTROL_MASK) {
        spdc_endpoint_snap_rotation(this, point, end_p, state);
    } else if (!(state & GDK_SHIFT_MASK)) {
        SnapManager &snap_manager = _desktop->namedview->snap_manager;
        snap_manager.setup(_desktop);
        Inkscape::SnapCandidatePoint scp(point, Inkscape::SNAPSOURCE_OTHER_HANDLE);
        scp.addOrigin(this->knot_end->position());
        Inkscape::SnappedPoint sp = snap_manager.freeSnap(scp);
        point = sp.getPoint();
        snap_manager.unSetup();
    }
    if(start_p != point) {
        start_p = point;
        this->knot_start->moveto(start_p);
    }
    showCanvasItems();
}

void MeasureTool::knotEndMovedHandler(SPKnot */*knot*/, Geom::Point const &ppointer, guint state)
{
    Geom::Point point = this->knot_end->position();
    if (state & GDK_CONTROL_MASK) {
        spdc_endpoint_snap_rotation(this, point, start_p, state);
    } else if (!(state & GDK_SHIFT_MASK)) {
        SnapManager &snap_manager = _desktop->namedview->snap_manager;
        snap_manager.setup(_desktop);
        Inkscape::SnapCandidatePoint scp(point, Inkscape::SNAPSOURCE_OTHER_HANDLE);
        scp.addOrigin(this->knot_start->position());
        Inkscape::SnappedPoint sp = snap_manager.freeSnap(scp);
        point = sp.getPoint();
        snap_manager.unSetup();
    }
    if(end_p != point) {
        end_p = point;
        this->knot_end->moveto(end_p);
    }
    showCanvasItems();
}

void MeasureTool::knotUngrabbedHandler(SPKnot */*knot*/,  unsigned int state)
{
    this->knot_start->moveto(start_p);
    this->knot_end->moveto(end_p);
    showCanvasItems();
}

static void calculate_intersections(SPDesktop *desktop, SPItem *item, Geom::PathVector const &lineseg,
                                    SPCurve curve, std::vector<double> &intersections)
{
    curve.transform(item->i2doc_affine());
    // Find all intersections of the control-line with this shape
    Geom::CrossingSet cs = Geom::crossings(lineseg, curve.get_pathvector());
    Geom::delete_duplicates(cs[0]);

    // Reconstruct and store the points of intersection
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool show_hidden = prefs->getBool("/tools/measure/show_hidden", true);
    for (const auto & m : cs[0]) {
        if (!show_hidden) {
            double eps = 0.0001;
            if ((m.ta > eps &&
             item == desktop->getItemAtPoint(desktop->d2w(desktop->dt2doc(lineseg[0].pointAt(m.ta - eps))), true, nullptr)) ||
            (m.ta + eps < 1 &&
             item == desktop->getItemAtPoint(desktop->d2w(desktop->dt2doc(lineseg[0].pointAt(m.ta + eps))), true, nullptr))) {
                intersections.push_back(m.ta);
            }
        } else {
            intersections.push_back(m.ta);
        }
    }
}

bool MeasureTool::root_handler(GdkEvent* event)
{
    gint ret = FALSE;

    switch (event->type) {
    case GDK_BUTTON_PRESS: {
        if (event->button.button != 1) {
            break;
        }
        this->knot_start->hide();
        this->knot_end->hide();
        Geom::Point const button_w(event->button.x, event->button.y);
        explicit_base = std::nullopt;
        explicit_base_tmp = std::nullopt;
        last_end = std::nullopt;

        // save drag origin
        start_p = _desktop->w2d(Geom::Point(event->button.x, event->button.y));
        within_tolerance = true;

        SnapManager &snap_manager = _desktop->namedview->snap_manager;
        snap_manager.setup(_desktop);
        snap_manager.freeSnapReturnByRef(start_p, Inkscape::SNAPSOURCE_OTHER_HANDLE);
        snap_manager.unSetup();

        grabCanvasEvents(Gdk::KEY_PRESS_MASK      |
                         Gdk::KEY_RELEASE_MASK    |
                         Gdk::BUTTON_PRESS_MASK   |
                         Gdk::BUTTON_RELEASE_MASK |
                         Gdk::POINTER_MOTION_MASK );
        ret = TRUE;
        break;
    }
    case GDK_KEY_PRESS: {
        if ((event->key.keyval == GDK_KEY_Control_L) || (event->key.keyval == GDK_KEY_Control_R)) {
            explicit_base_tmp = explicit_base;
            explicit_base = end_p;
            showInfoBox(last_pos, true);
        }
        break;
    }
    case GDK_KEY_RELEASE: {
        if ((event->key.keyval == GDK_KEY_Control_L) || (event->key.keyval == GDK_KEY_Control_R)) {
            showInfoBox(last_pos, false);
        }
        break;
    }
    case GDK_MOTION_NOTIFY: {
        if (!(event->motion.state & GDK_BUTTON1_MASK)) {
            if(!(event->motion.state & GDK_SHIFT_MASK)) {
                Geom::Point const motion_w(event->motion.x, event->motion.y);
                Geom::Point const motion_dt(_desktop->w2d(motion_w));

                SnapManager &snap_manager = _desktop->namedview->snap_manager;
                snap_manager.setup(_desktop);

                Inkscape::SnapCandidatePoint scp(motion_dt, Inkscape::SNAPSOURCE_OTHER_HANDLE);
                scp.addOrigin(start_p);

                snap_manager.preSnap(scp);
                snap_manager.unSetup();
            }
            last_pos = Geom::Point(event->motion.x, event->motion.y);
            if (event->motion.state & GDK_CONTROL_MASK) {
                showInfoBox(last_pos, true);
            } else {
                showInfoBox(last_pos, false);
            }
        } else {
            // Inkscape::Util::Unit const * unit = _desktop->getNamedView()->getDisplayUnit();
            measure_item.clear();

            ret = TRUE;
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);
            Geom::Point const motion_w(event->motion.x, event->motion.y);
            if ( within_tolerance) {
                if ( Geom::LInfty( motion_w - start_p ) < tolerance) {
                    return FALSE;   // Do not drag if we're within tolerance from origin.
                }
            }
            // Once the user has moved farther than tolerance from the original location
            // (indicating they intend to move the object, not click), then always process the
            // motion notify coordinates as given (no snapping back to origin)
            within_tolerance = false;
            if(event->motion.time == 0 || !last_end  || Geom::LInfty( motion_w - *last_end ) > (tolerance/4.0)) {
                Geom::Point const motion_dt(_desktop->w2d(motion_w));
                end_p = motion_dt;

                if (event->motion.state & GDK_CONTROL_MASK) {
                    spdc_endpoint_snap_rotation(this, end_p, start_p, event->motion.state);
                } else if (!(event->motion.state & GDK_SHIFT_MASK)) {
                    SnapManager &snap_manager = _desktop->namedview->snap_manager;
                    snap_manager.setup(_desktop);
                    Inkscape::SnapCandidatePoint scp(end_p, Inkscape::SNAPSOURCE_OTHER_HANDLE);
                    scp.addOrigin(start_p);
                    Inkscape::SnappedPoint sp = snap_manager.freeSnap(scp);
                    end_p = sp.getPoint();
                    snap_manager.unSetup();
                }
                showCanvasItems();
                last_end = motion_w ;
            }
            gobble_motion_events(GDK_BUTTON1_MASK);
        }
        break;
    }
    case GDK_BUTTON_RELEASE: {
        if (event->button.button != 1) {
            break;
        }
        this->knot_start->moveto(start_p);
        this->knot_start->show();
        if(last_end) {
            end_p = _desktop->w2d(*last_end);
            if (event->button.state & GDK_CONTROL_MASK) {
                spdc_endpoint_snap_rotation(this, end_p, start_p, event->motion.state);
            } else if (!(event->button.state & GDK_SHIFT_MASK)) {
                SnapManager &snap_manager = _desktop->namedview->snap_manager;
                snap_manager.setup(_desktop);
                Inkscape::SnapCandidatePoint scp(end_p, Inkscape::SNAPSOURCE_OTHER_HANDLE);
                scp.addOrigin(start_p);
                Inkscape::SnappedPoint sp = snap_manager.freeSnap(scp);
                end_p = sp.getPoint();
                snap_manager.unSetup();
            }
        }
        this->knot_end->moveto(end_p);
        this->knot_end->show();
        showCanvasItems();

        ungrabCanvasEvents();
        break;
    }
    default:
        break;
    }
    if (!ret) {
        ret = ToolBase::root_handler(event);
    }

    return ret;
}

void MeasureTool::setMarkers()
{
    SPDocument *doc = _desktop->getDocument();
    SPObject *arrowStart = doc->getObjectById("Arrow2Sstart");
    SPObject *arrowEnd = doc->getObjectById("Arrow2Send");
    if (!arrowStart) {
        setMarker(true);
    }
    if(!arrowEnd) {
        setMarker(false);
    }
}
void MeasureTool::setMarker(bool isStart)
{
    SPDocument *doc = _desktop->getDocument();
    SPDefs *defs = doc->getDefs();
    Inkscape::XML::Node *rmarker;
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    rmarker = xml_doc->createElement("svg:marker");
    rmarker->setAttribute("id", isStart ? "Arrow2Sstart" : "Arrow2Send");
    rmarker->setAttribute("inkscape:isstock", "true");
    rmarker->setAttribute("inkscape:stockid", isStart ? "Arrow2Sstart" : "Arrow2Send");
    rmarker->setAttribute("orient", "auto");
    rmarker->setAttribute("refX", "0.0");
    rmarker->setAttribute("refY", "0.0");
    rmarker->setAttribute("style", "overflow:visible;");
    auto marker = cast<SPItem>(defs->appendChildRepr(rmarker));
    Inkscape::GC::release(rmarker);
    marker->updateRepr();
    Inkscape::XML::Node *rpath;
    rpath = xml_doc->createElement("svg:path");
    rpath->setAttribute("d", "M 8.72,4.03 L -2.21,0.02 L 8.72,-4.00 C 6.97,-1.63 6.98,1.62 8.72,4.03 z");
    rpath->setAttribute("id", isStart ? "Arrow2SstartPath" : "Arrow2SendPath");
    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_set_property (css, "stroke", "none");
    sp_repr_css_set_property (css, "fill", "#000000");
    sp_repr_css_set_property (css, "fill-opacity", "1");
    Glib::ustring css_str;
    sp_repr_css_write_string(css,css_str);
    rpath->setAttribute("style", css_str);
    sp_repr_css_attr_unref (css);
    rpath->setAttribute("transform", isStart ? "scale(0.3) translate(-2.3,0)" : "scale(0.3) rotate(180) translate(-2.3,0)");
    auto path = cast<SPItem>(marker->appendChildRepr(rpath));
    Inkscape::GC::release(rpath);
    path->updateRepr();
}

void MeasureTool::toGuides()
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite() || start_p == end_p) {
        return;
    }
    SPDocument *doc = _desktop->getDocument();
    Geom::Point start = _desktop->doc2dt(start_p) * _desktop->doc2dt();
    Geom::Point end = _desktop->doc2dt(end_p) * _desktop->doc2dt();
    Geom::Ray ray(start,end);
    SPNamedView *namedview = _desktop->namedview;
    if(!namedview) {
        return;
    }
    setGuide(start,ray.angle(), _("Measure"));
    if(explicit_base) {
        auto layer = _desktop->layerManager().currentLayer();
        explicit_base = *explicit_base * layer->i2doc_affine().inverse();
        ray.setPoints(start, *explicit_base);
        if(ray.angle() != 0) {
            setGuide(start,ray.angle(), _("Base"));
        }
    }
    setGuide(start,0,"");
    setGuide(start,Geom::rad_from_deg(90),_("Start"));
    setGuide(end,0,_("End"));
    setGuide(end,Geom::rad_from_deg(90),"");
    showCanvasItems(true);
    doc->ensureUpToDate();
    DocumentUndo::done(_desktop->getDocument(), _("Add guides from measure tool"), INKSCAPE_ICON("tool-measure"));
}

void MeasureTool::toPhantom()
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite() || start_p == end_p) {
        return;
    }
    SPDocument *doc = _desktop->getDocument();

    measure_phantom_items.clear();
    measure_tmp_items.clear();

    showCanvasItems(false, false, true);
    doc->ensureUpToDate();
    DocumentUndo::done(_desktop->getDocument(), _("Keep last measure on the canvas, for reference"), INKSCAPE_ICON("tool-measure"));
}

void MeasureTool::toItem()
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite() || start_p == end_p) {
        return;
    }
    SPDocument *doc = _desktop->getDocument();
    Geom::Ray ray(start_p,end_p);
    guint32 line_color_primary = 0x0000ff7f;
    Inkscape::XML::Document *xml_doc = _desktop->doc()->getReprDoc();
    Inkscape::XML::Node *rgroup = xml_doc->createElement("svg:g");
    showCanvasItems(false, true, false, rgroup);
    setLine(start_p,end_p, false, line_color_primary, rgroup);
    auto measure_item = cast<SPItem>(_desktop->layerManager().currentLayer()->appendChildRepr(rgroup));
    Inkscape::GC::release(rgroup);
    measure_item->updateRepr();
    doc->ensureUpToDate();
    DocumentUndo::done(_desktop->getDocument(), _("Convert measure to items"), INKSCAPE_ICON("tool-measure"));
    reset();
}

void MeasureTool::toMarkDimension()
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite() || start_p == end_p) {
        return;
    }
    SPDocument *doc = _desktop->getDocument();
    setMarkers();
    Geom::Ray ray(start_p,end_p);
    Geom::Point start = start_p + Geom::Point::polar(ray.angle(), 5);
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    dimension_offset = prefs->getDouble("/tools/measure/offset", 5.0);
    start = start + Geom::Point::polar(ray.angle() + Geom::rad_from_deg(90), -dimension_offset);
    Geom::Point end = end_p + Geom::Point::polar(ray.angle(), -5);
    end = end+ Geom::Point::polar(ray.angle() + Geom::rad_from_deg(90), -dimension_offset);
    guint32 color = 0x000000ff;
    setLine(start, end, true, color);
    Glib::ustring unit_name = prefs->getString("/tools/measure/unit");
    if (!unit_name.compare("")) {
        unit_name = DEFAULT_UNIT_NAME;
    }
    double fontsize = prefs->getDouble("/tools/measure/fontsize", 10.0);

    Geom::Point middle = Geom::middle_point(start, end);
    double totallengthval = (end_p - start_p).length();
    totallengthval = Inkscape::Util::Quantity::convert(totallengthval, "px", unit_name);
    double scale = prefs->getDouble("/tools/measure/scale", 100.0) / 100.0;


    int precision = prefs->getInt("/tools/measure/precision", 2);
    Glib::ustring total = Glib::ustring::format(std::fixed, std::setprecision(precision), totallengthval * scale);
    total += unit_name;

    double textangle = Geom::rad_from_deg(180) - ray.angle();
    if (_desktop->is_yaxisdown()) {
        textangle = ray.angle() - Geom::rad_from_deg(180);
    }

    setLabelText(total, middle, fontsize, textangle, color);

    doc->ensureUpToDate();
    DocumentUndo::done(_desktop->getDocument(), _("Add global measure line"), INKSCAPE_ICON("tool-measure"));
}

void MeasureTool::setGuide(Geom::Point origin, double angle, const char *label)
{
    SPDocument *doc = _desktop->getDocument();
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    SPRoot const *root = doc->getRoot();
    Geom::Affine affine(Geom::identity());
    if(root) {
        affine *= root->c2p.inverse();
    }
    SPNamedView *namedview = _desktop->namedview;
    if(!namedview) {
        return;
    }

    // <sodipodi:guide> stores inverted y-axis coordinates
    if (_desktop->is_yaxisdown()) {
        origin[Geom::Y] = doc->getHeight().value("px") - origin[Geom::Y];
        angle *= -1.0;
    }

    origin *= affine;
    //measure angle
    Inkscape::XML::Node *guide;
    guide = xml_doc->createElement("sodipodi:guide");
    std::stringstream position;
    position.imbue(std::locale::classic());
    position <<  origin[Geom::X] << "," << origin[Geom::Y];
    guide->setAttribute("position", position.str() );
    guide->setAttribute("inkscape:color", "rgb(167,0,255)");
    guide->setAttribute("inkscape:label", label);
    Geom::Point unit_vector = Geom::rot90(origin.polar(angle));
    std::stringstream angle_str;
    angle_str.imbue(std::locale::classic());
    angle_str << unit_vector[Geom::X] << "," << unit_vector[Geom::Y];
    guide->setAttribute("orientation", angle_str.str());
    namedview->appendChild(guide);
    Inkscape::GC::release(guide);
}

void MeasureTool::setLine(Geom::Point start_point,Geom::Point end_point, bool markers, guint32 color, Inkscape::XML::Node *measure_repr)
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite()) {
        return;
    }
    Geom::PathVector pathv;
    Geom::Path path;
    path.start(_desktop->doc2dt(start_point));
    path.appendNew<Geom::LineSegment>(_desktop->doc2dt(end_point));
    pathv.push_back(path);
    pathv *= _desktop->layerManager().currentLayer()->i2doc_affine().inverse();
    if(!pathv.empty()) {
        setMeasureItem(pathv, false, markers, color, measure_repr);
    }
}

void MeasureTool::setPoint(Geom::Point origin, Inkscape::XML::Node *measure_repr)
{
    if (!_desktop || !origin.isFinite()) {
        return;
    }
    char const * svgd;
    svgd = "m 0.707,0.707 6.586,6.586 m 0,-6.586 -6.586,6.586";
    Geom::PathVector pathv = sp_svg_read_pathv(svgd);
    Geom::Scale scale = Geom::Scale(_desktop->current_zoom()).inverse();
    pathv *= Geom::Translate(Geom::Point(-3.5,-3.5));
    pathv *= scale;
    pathv *= Geom::Translate(Geom::Point() - (scale.vector() * 0.5));
    pathv *= Geom::Translate(_desktop->doc2dt(origin));
    pathv *= _desktop->layerManager().currentLayer()->i2doc_affine().inverse();
    if (!pathv.empty()) {
        guint32 line_color_secondary = 0xff0000ff;
        setMeasureItem(pathv, false, false, line_color_secondary, measure_repr);
    }
}

void MeasureTool::setLabelText(Glib::ustring const &value, Geom::Point pos, double fontsize, Geom::Coord angle,
                               guint32 background, Inkscape::XML::Node *measure_repr)
{
    Inkscape::XML::Document *xml_doc = _desktop->doc()->getReprDoc();
    /* Create <text> */
    pos = _desktop->doc2dt(pos);
    Inkscape::XML::Node *rtext = xml_doc->createElement("svg:text");
    rtext->setAttribute("xml:space", "preserve");


    /* Set style */
    sp_desktop_apply_style_tool(_desktop, rtext, "/tools/text", true);
    if(measure_repr) {
        rtext->setAttributeSvgDouble("x", 2);
        rtext->setAttributeSvgDouble("y", 2);
    } else {
        rtext->setAttributeSvgDouble("x", 0);
        rtext->setAttributeSvgDouble("y", 0);
    }

    /* Create <tspan> */
    Inkscape::XML::Node *rtspan = xml_doc->createElement("svg:tspan");
    rtspan->setAttribute("sodipodi:role", "line");
    SPCSSAttr *css = sp_repr_css_attr_new();
    std::stringstream font_size;
    font_size.imbue(std::locale::classic());
    if(measure_repr) {
        font_size <<  fontsize;
    } else {
        font_size <<  fontsize << "pt";
    }
    sp_repr_css_set_property (css, "font-size", font_size.str().c_str());
    sp_repr_css_set_property (css, "font-style", "normal");
    sp_repr_css_set_property (css, "font-weight", "normal");
    sp_repr_css_set_property (css, "line-height", "125%");
    sp_repr_css_set_property (css, "letter-spacing", "0");
    sp_repr_css_set_property (css, "word-spacing", "0");
    sp_repr_css_set_property (css, "text-align", "center");
    sp_repr_css_set_property (css, "text-anchor", "middle");
    if(measure_repr) {
        sp_repr_css_set_property (css, "fill", "#FFFFFF");
    } else {
        sp_repr_css_set_property (css, "fill", "#000000");
    }
    sp_repr_css_set_property (css, "fill-opacity", "1");
    sp_repr_css_set_property (css, "stroke", "none");
    Glib::ustring css_str;
    sp_repr_css_write_string(css,css_str);
    rtspan->setAttribute("style", css_str);
    sp_repr_css_attr_unref (css);
    rtext->addChild(rtspan, nullptr);
    Inkscape::GC::release(rtspan);
    /* Create TEXT */
    Inkscape::XML::Node *rstring = xml_doc->createTextNode(value.c_str());
    rtspan->addChild(rstring, nullptr);
    Inkscape::GC::release(rstring);
    auto layer = _desktop->layerManager().currentLayer();
    auto text_item = cast<SPText>(layer->appendChildRepr(rtext));
    Inkscape::GC::release(rtext);
    text_item->rebuildLayout();
    text_item->updateRepr();
    Geom::OptRect bbox = text_item->geometricBounds();
    if (!measure_repr && bbox) {
        Geom::Point center = bbox->midpoint();
        text_item->transform *= Geom::Translate(center).inverse();
        pos += Geom::Point::polar(angle+ Geom::rad_from_deg(90), -bbox->height());
    }
    if(measure_repr) {
        /* Create <group> */
        Inkscape::XML::Node *rgroup = xml_doc->createElement("svg:g");
        /* Create <rect> */
        Inkscape::XML::Node *rrect = xml_doc->createElement("svg:rect");
        SPCSSAttr *css = sp_repr_css_attr_new ();
        gchar color_line[64];
        sp_svg_write_color (color_line, sizeof(color_line), background);
        sp_repr_css_set_property (css, "fill", color_line);
        sp_repr_css_set_property (css, "fill-opacity", "0.5");
        sp_repr_css_set_property (css, "stroke-width", "0");
        Glib::ustring css_str;
        sp_repr_css_write_string(css,css_str);
        rrect->setAttribute("style", css_str);
        sp_repr_css_attr_unref (css);
        rgroup->setAttributeSvgDouble("x", 0);
        rgroup->setAttributeSvgDouble("y", 0);
        rrect->setAttributeSvgDouble("x", -bbox->width()/2.0);
        rrect->setAttributeSvgDouble("y", -bbox->height());
        rrect->setAttributeSvgDouble("width", bbox->width() + 6);
        rrect->setAttributeSvgDouble("height", bbox->height() + 6);
        Inkscape::XML::Node *rtextitem = text_item->getRepr();
        text_item->deleteObject();
        rgroup->addChild(rtextitem, nullptr);
        Inkscape::GC::release(rtextitem);
        rgroup->addChild(rrect, nullptr);
        Inkscape::GC::release(rrect);
        auto text_item_box = cast<SPItem>(layer->appendChildRepr(rgroup));
        Geom::Scale scale = Geom::Scale(_desktop->current_zoom()).inverse();
        if(bbox) {
            text_item_box->transform *= Geom::Translate(bbox->midpoint() - Geom::Point(1.0,1.0)).inverse();
        }
        text_item_box->transform *= scale;
        text_item_box->transform *= Geom::Translate(Geom::Point() - (scale.vector() * 0.5));
        text_item_box->transform *= Geom::Translate(pos);
        text_item_box->transform *= layer->i2doc_affine().inverse();
        text_item_box->updateRepr();
        text_item_box->doWriteTransform(text_item_box->transform, nullptr, true);
        Inkscape::XML::Node *rlabel = text_item_box->getRepr();
        text_item_box->deleteObject();
        measure_repr->addChild(rlabel, nullptr);
        Inkscape::GC::release(rlabel);
    } else {
        text_item->transform *= Geom::Rotate(angle);
        text_item->transform *= Geom::Translate(pos);
        text_item->transform *= layer->i2doc_affine().inverse();
        text_item->doWriteTransform(text_item->transform, nullptr, true);
    }
}

void MeasureTool::reset()
{
    this->knot_start->hide();
    this->knot_end->hide();

    measure_tmp_items.clear();
}

void MeasureTool::setMeasureCanvasText(bool is_angle, double precision, double amount, double fontsize,
                                       Glib::ustring unit_name, Geom::Point position, guint32 background,
                                       bool to_left, bool to_item,
                                       bool to_phantom, Inkscape::XML::Node *measure_repr)
{
    Glib::ustring measure = Glib::ustring::format(std::setprecision(precision), std::fixed, amount);
    measure += " ";
    measure += (is_angle ? "°" : unit_name);
    auto canvas_tooltip = new Inkscape::CanvasItemText(_desktop->getCanvasTemp(), position, measure);
    canvas_tooltip->set_fontsize(fontsize);
    canvas_tooltip->set_fill(0xffffffff);
    canvas_tooltip->set_background(background);
    if (to_left) {
        canvas_tooltip->set_anchor(Geom::Point(0, 0.5));
    } else {
        canvas_tooltip->set_anchor(Geom::Point(0.5, 0.5));
    }

    if (to_phantom){
        canvas_tooltip->set_background(0x4444447f);
        measure_phantom_items.emplace_back(canvas_tooltip);
    } else {
        measure_tmp_items.emplace_back(canvas_tooltip);
    }

    if (to_item) {
        setLabelText(measure, position, fontsize, 0, background, measure_repr);
    }

    canvas_tooltip->show();

}

void MeasureTool::setMeasureCanvasItem(Geom::Point position, bool to_item, bool to_phantom, Inkscape::XML::Node *measure_repr){
    guint32 color = 0xff0000ff;
    if (to_phantom){
        color = 0x888888ff;
    }

    auto canvas_item = new Inkscape::CanvasItemCtrl(_desktop->getCanvasTemp(), Inkscape::CANVAS_ITEM_CTRL_TYPE_POINT, position);
    canvas_item->set_stroke(color);
    canvas_item->lower_to_bottom();
    canvas_item->set_pickable(false);
    canvas_item->show();

    if (to_phantom){
        measure_phantom_items.emplace_back(canvas_item);
    } else {
        measure_tmp_items.emplace_back(canvas_item);
    }

    if(to_item) {
        setPoint(position, measure_repr);
    }
}

void MeasureTool::setMeasureCanvasControlLine(Geom::Point start, Geom::Point end, bool to_item, bool to_phantom,
                                              Inkscape::CanvasItemColor ctrl_line_type,
                                              Inkscape::XML::Node *measure_repr){
    gint32 color = (ctrl_line_type == Inkscape::CANVAS_ITEM_PRIMARY) ? 0x0000ff7f : 0xff00007f;
    if (to_phantom) {
        color = (ctrl_line_type == Inkscape::CANVAS_ITEM_PRIMARY) ? 0x4444447f : 0x8888887f;
    }

    auto control_line = new Inkscape::CanvasItemCurve(_desktop->getCanvasTemp(), start, end);
    control_line->set_stroke(color);
    control_line->lower_to_bottom();
    control_line->show();

    if (to_phantom) {
        measure_phantom_items.emplace_back(control_line);
    } else {
        measure_tmp_items.emplace_back(control_line);
    }

    if (to_item) {
        setLine(start, end, false, color, measure_repr);
    }
}

// This is the text that follows the cursor around.
void MeasureTool::showItemInfoText(Geom::Point pos, Glib::ustring const &measure_str, double fontsize)
{
    auto canvas_tooltip = new CanvasItemText(_desktop->getCanvasTemp(), pos, measure_str);
    canvas_tooltip->set_fontsize(fontsize);
    canvas_tooltip->set_fill(0xffffffff);
    canvas_tooltip->set_background(0x00000099);
    canvas_tooltip->set_anchor(Geom::Point(0, 0));
    canvas_tooltip->set_fixed_line(true);
    canvas_tooltip->show();
    measure_item.emplace_back(canvas_tooltip);
}

void MeasureTool::showInfoBox(Geom::Point cursor, bool into_groups)
{
    using Inkscape::Util::Quantity;

    measure_item.clear();

    SPItem *newover = _desktop->getItemAtPoint(cursor, into_groups);
    if (!newover) {
        // Clear over when the cursor isn't over anything.
        over = nullptr;
        return;
    }
    Inkscape::Util::Unit const *unit = _desktop->getNamedView()->getDisplayUnit();

    // Load preferences for measuring the new object.
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int precision = prefs->getInt("/tools/measure/precision", 2);
    bool selected = prefs->getBool("/tools/measure/only_selected", false);
    auto box_type = prefs->getBool("/tools/bounding_box", false) ? SPItem::GEOMETRIC_BBOX : SPItem::VISUAL_BBOX;
    double fontsize = prefs->getDouble("/tools/measure/fontsize", 10.0);
    double scale    = prefs->getDouble("/tools/measure/scale", 100.0) / 100.0;
    Glib::ustring unit_name = prefs->getString("/tools/measure/unit", unit->abbr);

    Geom::Scale zoom = Geom::Scale(Quantity::convert(_desktop->current_zoom(), "px", unit->abbr)).inverse();

    if(newover != over) {
        // Get information for the item, and cache it to save time.
        over = newover;
        auto affine = over->i2dt_affine() * Geom::Scale(scale);
        // Correct for the current page's position.
        if (prefs->getBool("/options/origincorrection/page", true)) {
            affine *= _desktop->getDocument()->getPageManager().getSelectedPageAffine().inverse();
        }
        if (auto bbox = over->bounds(box_type, affine)) {
            item_width  = Quantity::convert(bbox->width(), "px", unit_name);
            item_height = Quantity::convert(bbox->height(), "px", unit_name);
            item_x      = Quantity::convert(bbox->left(), "px", unit_name);
            item_y      = Quantity::convert(bbox->top(), "px", unit_name);

            if (auto shape = cast<SPShape>(over)) {
                auto pw = paths_to_pw(shape->curve()->get_pathvector());
                item_length = Quantity::convert(Geom::length(pw * affine), "px", unit_name);
            }
        }
    }

    gchar *measure_str = nullptr;
    std::stringstream precision_str;
    precision_str.imbue(std::locale::classic());
    double origin = Quantity::convert(14, "px", unit->abbr);
    double yaxis_shift = Quantity::convert(fontsize, "px", unit->abbr);
    Geom::Point rel_position = Geom::Point(origin, origin + yaxis_shift);
    /* Keeps infobox just above the cursor */
    Geom::Point pos = _desktop->w2d(cursor);
    double gap = Quantity::convert(7 + fontsize, "px", unit->abbr);
    double yaxisdir = _desktop->yaxisdir();

    if (selected) {
        showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), _desktop->getSelection()->includes(over) ? _("Selected") : _("Not selected"), fontsize);
        rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);
    }

    if (is<SPShape>(over)) {

        precision_str << _("Length") <<  ": %." << precision << "f %s";
        measure_str = g_strdup_printf(precision_str.str().c_str(), item_length, unit_name.c_str());
        precision_str.str("");
        showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
        rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);

    } else if (is<SPGroup>(over)) {

        measure_str = _("Press 'CTRL' to measure into group");
        showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
        rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);

    }

    precision_str <<  "Y: %." << precision << "f %s";
    measure_str = g_strdup_printf(precision_str.str().c_str(), item_y, unit_name.c_str());
    precision_str.str("");
    showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
    rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);

    precision_str <<  "X: %." << precision << "f %s";
    measure_str = g_strdup_printf(precision_str.str().c_str(), item_x, unit_name.c_str());
    precision_str.str("");
    showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
    rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);

    precision_str << _("Height") << ": %." << precision << "f %s";
    measure_str = g_strdup_printf(precision_str.str().c_str(), item_height, unit_name.c_str());
    precision_str.str("");
    showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
    rel_position = Geom::Point(rel_position[Geom::X], rel_position[Geom::Y] + gap);

    precision_str << _("Width") << ": %." << precision << "f %s";
    measure_str = g_strdup_printf(precision_str.str().c_str(), item_width, unit_name.c_str());
    precision_str.str("");
    showItemInfoText(pos - (yaxisdir * Geom::Point(0, rel_position[Geom::Y]) * zoom), measure_str, fontsize);
    g_free(measure_str);
}

void MeasureTool::showCanvasItems(bool to_guides, bool to_item, bool to_phantom, Inkscape::XML::Node *measure_repr)
{
    if (!_desktop || !start_p.isFinite() || !end_p.isFinite() || start_p == end_p) {
        return;
    }
    writeMeasurePoint(start_p, true);
    writeMeasurePoint(end_p, false);

    //clear previous canvas items, we'll draw new ones
    measure_tmp_items.clear();

    //TODO:Calculate the measure area for current length and origin
    // and use canvas->redraw_all(). In the calculation need a gap for outside text
    // maybe this remove the trash lines on measure use
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool show_in_between = prefs->getBool("/tools/measure/show_in_between", true);
    bool all_layers = prefs->getBool("/tools/measure/all_layers", true);
    dimension_offset = 70;
    Geom::PathVector lineseg;
    Geom::Path p;
    Geom::Point start_p_doc = start_p * _desktop->dt2doc();
    Geom::Point end_p_doc = end_p * _desktop->dt2doc();
    p.start(start_p_doc);
    p.appendNew<Geom::LineSegment>(end_p_doc);
    lineseg.push_back(p);

    double angle = atan2(end_p - start_p);
    double baseAngle = 0;

    if (explicit_base) {
        baseAngle = atan2(*explicit_base - start_p);
        angle -= baseAngle;

        // make sure that the angle is between -pi and pi.
        if (angle > M_PI) {
            angle -= 2 * M_PI;
        }
        if (angle < -M_PI) {
            angle += 2 * M_PI;
        }
    }

    std::vector<SPItem*> items;
    SPDocument *doc = _desktop->getDocument();
    Geom::Rect rect(start_p_doc, end_p_doc);
    items = doc->getItemsPartiallyInBox(_desktop->dkey, rect, false, true, false, true);
    SPGroup *current_layer = _desktop->layerManager().currentLayer();

    std::vector<double> intersection_times;
    bool only_selected = prefs->getBool("/tools/measure/only_selected", false);
    for (auto i : items) {
        SPItem *item = i;
        if (!_desktop->getSelection()->includes(i) && only_selected) {
            continue;
        }
        if (all_layers || _desktop->layerManager().layerForObject(item) == current_layer) {
            if (auto shape = cast<SPShape>(item)) {
                calculate_intersections(_desktop, item, lineseg, *shape->curve(), intersection_times);
            } else {
                if (is<SPText>(item) || is<SPFlowtext>(item)) {
                    Inkscape::Text::Layout::iterator iter = te_get_layout(item)->begin();
                    do {
                        Inkscape::Text::Layout::iterator iter_next = iter;
                        iter_next.nextGlyph(); // iter_next is one glyph ahead from iter
                        if (iter == iter_next) {
                            break;
                        }

                        // get path from iter to iter_next:
                        auto curve = te_get_layout(item)->convertToCurves(iter, iter_next);
                        iter = iter_next; // shift to next glyph
                        if (curve.is_empty()) { // whitespace glyph?
                            continue;
                        }

                        calculate_intersections(_desktop, item, lineseg, std::move(curve), intersection_times);
                        if (iter == te_get_layout(item)->end()) {
                            break;
                        }
                    } while (true);
                }
            }
        }
    }
    Glib::ustring unit_name = prefs->getString("/tools/measure/unit");
    if (!unit_name.compare("")) {
        unit_name = DEFAULT_UNIT_NAME;
    }
    double scale = prefs->getDouble("/tools/measure/scale", 100.0) / 100.0;
    double fontsize = prefs->getDouble("/tools/measure/fontsize", 10.0);
    // Normal will be used for lines and text
    Geom::Point windowNormal = Geom::unit_vector(Geom::rot90(_desktop->d2w(end_p - start_p)));
    Geom::Point normal = _desktop->w2d(windowNormal);

    std::vector<Geom::Point> intersections;
    std::sort(intersection_times.begin(), intersection_times.end());
    for (double & intersection_time : intersection_times) {
        intersections.push_back(lineseg[0].pointAt(intersection_time));
    }

    if(!show_in_between && intersection_times.size() > 1) {
        Geom::Point start = lineseg[0].pointAt(intersection_times[0]);
        Geom::Point end = lineseg[0].pointAt(intersection_times[intersection_times.size()-1]);
        intersections.clear();
        intersections.push_back(start);
        intersections.push_back(end);
    }
    if (!prefs->getBool("/tools/measure/ignore_1st_and_last", true)) {
        intersections.insert(intersections.begin(),lineseg[0].pointAt(0));
        intersections.push_back(lineseg[0].pointAt(1));
    }
    std::vector<LabelPlacement> placements;
    for (size_t idx = 1; idx < intersections.size(); ++idx) {
        LabelPlacement placement;
        placement.lengthVal = (intersections[idx] - intersections[idx - 1]).length();
        placement.lengthVal = Inkscape::Util::Quantity::convert(placement.lengthVal, "px", unit_name);
        placement.offset = dimension_offset / 2;
        placement.start = _desktop->doc2dt((intersections[idx - 1] + intersections[idx]) / 2);
        placement.end = placement.start - (normal * placement.offset);

        placements.push_back(placement);
    }
    int precision = prefs->getInt("/tools/measure/precision", 2);
    // Adjust positions
    repositionOverlappingLabels(placements, _desktop, windowNormal, fontsize, precision);
    for (auto & place : placements) {
        setMeasureCanvasText(false, precision, place.lengthVal * scale, fontsize, unit_name, place.end, 0x0000007f,
                             false, to_item, to_phantom, measure_repr);
    }
    Geom::Point angleDisplayPt = calcAngleDisplayAnchor(_desktop, angle, baseAngle, start_p, end_p, fontsize);

    setMeasureCanvasText(true, precision, Geom::deg_from_rad(angle), fontsize, unit_name, angleDisplayPt, 0x337f337f,
                         false, to_item, to_phantom, measure_repr);

    {
        double totallengthval = (end_p - start_p).length();
        totallengthval = Inkscape::Util::Quantity::convert(totallengthval, "px", unit_name);
        Geom::Point origin = end_p + _desktop->w2d(Geom::Point(3 * fontsize, -fontsize));
        setMeasureCanvasText(false, precision, totallengthval * scale, fontsize, unit_name, origin, 0x3333337f,
                             true, to_item, to_phantom, measure_repr);
    }

    if (intersections.size() > 2) {
        double totallengthval = (intersections[intersections.size()-1] - intersections[0]).length();
        totallengthval = Inkscape::Util::Quantity::convert(totallengthval, "px", unit_name);
        Geom::Point origin = _desktop->doc2dt((intersections[0] + intersections[intersections.size()-1])/2) + normal * dimension_offset;
        setMeasureCanvasText(false, precision, totallengthval * scale, fontsize, unit_name, origin, 0x33337f7f,
                             false, to_item, to_phantom, measure_repr);
    }

    // Initial point
    setMeasureCanvasItem(start_p, false, to_phantom, measure_repr);

    // Now that text has been added, we can add lines and controls so that they go underneath
    for (size_t idx = 0; idx < intersections.size(); ++idx) {
        setMeasureCanvasItem(_desktop->doc2dt(intersections[idx]), to_item, to_phantom, measure_repr);
        if(to_guides) {
            gchar *cross_number;
            if (!prefs->getBool("/tools/measure/ignore_1st_and_last", true)) {
                cross_number= g_strdup_printf(_("Crossing %lu"), static_cast<unsigned long>(idx));
            } else {
                cross_number= g_strdup_printf(_("Crossing %lu"), static_cast<unsigned long>(idx + 1));
            }
            if (!prefs->getBool("/tools/measure/ignore_1st_and_last", true) && idx == 0) {
                setGuide(_desktop->doc2dt(intersections[idx]), angle + Geom::rad_from_deg(90), "");
            } else {
                setGuide(_desktop->doc2dt(intersections[idx]), angle + Geom::rad_from_deg(90), cross_number);
            }
            g_free(cross_number);
        }
    }
    // Since adding goes to the bottom, do all lines last.

    // draw main control line
    {
        setMeasureCanvasControlLine(start_p, end_p, false, to_phantom, Inkscape::CANVAS_ITEM_PRIMARY, measure_repr);
        double length = std::abs((end_p - start_p).length());
        Geom::Point anchorEnd = start_p;
        anchorEnd[Geom::X] += length;
        if (explicit_base) {
            anchorEnd *= (Geom::Affine(Geom::Translate(-start_p))
                          * Geom::Affine(Geom::Rotate(baseAngle))
                          * Geom::Affine(Geom::Translate(start_p)));
        }
        setMeasureCanvasControlLine(start_p, anchorEnd, to_item, to_phantom, Inkscape::CANVAS_ITEM_SECONDARY, measure_repr);
        createAngleDisplayCurve(start_p, end_p, angleDisplayPt, angle, to_phantom, measure_repr);
    }

    if (intersections.size() > 2) {
        setMeasureCanvasControlLine(_desktop->doc2dt(intersections[0]) + normal * dimension_offset, _desktop->doc2dt(intersections[intersections.size() - 1]) + normal * dimension_offset, to_item, to_phantom, Inkscape::CANVAS_ITEM_PRIMARY , measure_repr);

        setMeasureCanvasControlLine(_desktop->doc2dt(intersections[0]), _desktop->doc2dt(intersections[0]) + normal * dimension_offset, to_item, to_phantom, Inkscape::CANVAS_ITEM_PRIMARY , measure_repr);

        setMeasureCanvasControlLine(_desktop->doc2dt(intersections[intersections.size() - 1]), _desktop->doc2dt(intersections[intersections.size() - 1]) + normal * dimension_offset, to_item, to_phantom, Inkscape::CANVAS_ITEM_PRIMARY , measure_repr);
    }

    // call-out lines
    for (auto & place : placements) {
        setMeasureCanvasControlLine(place.start, place.end, to_item, to_phantom, Inkscape::CANVAS_ITEM_SECONDARY, measure_repr);
    }

    {
        for (size_t idx = 1; idx < intersections.size(); ++idx) {
            Geom::Point measure_text_pos = (intersections[idx - 1] + intersections[idx]) / 2;
            setMeasureCanvasControlLine(_desktop->doc2dt(measure_text_pos), _desktop->doc2dt(measure_text_pos) - (normal * dimension_offset / 2), to_item, to_phantom, Inkscape::CANVAS_ITEM_SECONDARY, measure_repr);
        }
    }
}

/**
 * Create a measure item in current document.
 *
 * @param pathv the path to create.
 * @param markers if the path results get markers.
 * @param color of the stroke.
 * @param measure_repr container element.
 */
void MeasureTool::setMeasureItem(Geom::PathVector pathv, bool is_curve, bool markers, guint32 color, Inkscape::XML::Node *measure_repr)
{
    if(!_desktop) {
        return;
    }
    SPDocument *doc = _desktop->getDocument();
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();
    Inkscape::XML::Node *repr;
    repr = xml_doc->createElement("svg:path");
    auto str = sp_svg_write_path(pathv);
    SPCSSAttr *css = sp_repr_css_attr_new();
    auto layer = _desktop->layerManager().currentLayer();
    Geom::Coord strokewidth = layer->i2doc_affine().inverse().expansionX();
    std::stringstream stroke_width;
    stroke_width.imbue(std::locale::classic());
    if(measure_repr) {
        stroke_width <<  strokewidth / _desktop->current_zoom();
    } else {
        stroke_width <<  strokewidth;
    }
    sp_repr_css_set_property (css, "stroke-width", stroke_width.str().c_str());
    sp_repr_css_set_property (css, "fill", "none");
    if(color) {
        gchar color_line[64];
        sp_svg_write_color (color_line, sizeof(color_line), color);
        sp_repr_css_set_property (css, "stroke", color_line);
    } else {
        sp_repr_css_set_property (css, "stroke", "#ff0000");
    }
    char const * stroke_linecap = is_curve ? "butt" : "square";
    sp_repr_css_set_property (css, "stroke-linecap", stroke_linecap);
    sp_repr_css_set_property (css, "stroke-linejoin", "miter");
    sp_repr_css_set_property (css, "stroke-miterlimit", "4");
    sp_repr_css_set_property (css, "stroke-dasharray", "none");
    if(measure_repr) {
        sp_repr_css_set_property (css, "stroke-opacity", "0.5");
    } else {
        sp_repr_css_set_property (css, "stroke-opacity", "1");
    }
    if(markers) {
        sp_repr_css_set_property (css, "marker-start", "url(#Arrow2Sstart)");
        sp_repr_css_set_property (css, "marker-end", "url(#Arrow2Send)");
    }
    Glib::ustring css_str;
    sp_repr_css_write_string(css,css_str);
    repr->setAttribute("style", css_str);
    sp_repr_css_attr_unref (css);
    repr->setAttribute("d", str);
    if(measure_repr) {
        measure_repr->addChild(repr, nullptr);
        Inkscape::GC::release(repr);
    } else {
        auto item = cast<SPItem>(layer->appendChildRepr(repr));
        Inkscape::GC::release(repr);
        item->updateRepr();
        _desktop->getSelection()->clear();
        _desktop->getSelection()->add(item);
    }
}
}
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
