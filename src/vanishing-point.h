// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Vanishing point for 3D perspectives
 *
 * Authors:
 *   Maximilian Albert <Anhalter42@gmx.de>
 *
 * Copyright (C) 2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_VANISHING_POINT_H
#define SEEN_VANISHING_POINT_H

#include <2geom/point.h>
#include <list>
#include <set>

#include "selection.h"

#include "display/control/canvas-item-enums.h"

#include "object/persp3d.h"

class SPBox3D;
class SPKnot;

namespace Inkscape {
class CanvasItemCurve;
}

namespace Box3D {

enum VPState {
    VP_FINITE = 0, // perspective lines meet in the VP
    VP_INFINITE    // perspective lines are parallel
};

/* VanishingPoint is a simple wrapper class to easily extract VP data from perspectives.
 * A VanishingPoint represents a VP in a certain direction (X, Y, Z) of a single perspective.
 * In particular, it can potentially have more than one box linked to it (although in facth they
 * are rather linked to the parent perspective).
 */
// FIXME: Don't store the box in the VP but rather the perspective (and link the box to it)!!
class VanishingPoint {
public:
    VanishingPoint() : my_counter(VanishingPoint::global_counter++), _persp(nullptr), _axis(Proj::NONE) {}
    VanishingPoint(Persp3D *persp, Proj::Axis axis) : my_counter(VanishingPoint::global_counter++), _persp(persp), _axis(axis) {}
    VanishingPoint(const VanishingPoint &other) : my_counter(VanishingPoint::global_counter++), _persp(other._persp), _axis(other._axis) {}

    inline VanishingPoint &operator=(VanishingPoint const &rhs) = default;
    inline bool operator==(VanishingPoint const &rhs) const {
        /* vanishing points coincide if they belong to the same perspective */
        return (_persp == rhs._persp && _axis == rhs._axis);
    }

    inline bool operator<(VanishingPoint const &rhs) const {
        return my_counter < rhs.my_counter;
    }

    inline void set(Persp3D *persp, Proj::Axis axis) {
        _persp = persp;
        _axis = axis;
    }
    void set_pos(Proj::Pt2 const &pt);
    inline bool is_finite() const {
        g_return_val_if_fail (_persp, false);
        return _persp->get_VP (_axis).is_finite();
    }
    inline Geom::Point get_pos() const {
        g_return_val_if_fail (_persp, Geom::Point (Geom::infinity(), Geom::infinity()));
        return _persp->get_VP (_axis).affine();
    }
    inline Persp3D * get_perspective() const {
        return _persp;
    }
    inline Persp3D * set_perspective(Persp3D *persp) {
        return _persp = persp;
    }

    inline bool hasBox (SPBox3D *box) {
        return _persp->has_box(box);
    }
    inline unsigned int numberOfBoxes() const {
        return _persp->num_boxes();
    }

    /* returns all selected boxes sharing this perspective */
    std::list<SPBox3D *> selectedBoxes(Inkscape::Selection *sel);

    inline void updateBoxDisplays() const {
        g_return_if_fail (_persp);
        _persp->update_box_displays();
    }
    inline void updateBoxReprs() const {
        g_return_if_fail (_persp);
        _persp->update_box_reprs();
    }
    inline void updatePerspRepr() const {
        g_return_if_fail (_persp);
        _persp->updateRepr(SP_OBJECT_WRITE_EXT);
    }
    inline void printPt() const {
        g_return_if_fail (_persp);
        _persp->get_VP (_axis).print("");
    }
    inline char const *axisString () { return Proj::string_from_axis(_axis); }

    unsigned int my_counter;
    static unsigned int global_counter; // FIXME: Only to implement operator< so that we can merge lists. Do this in a better way!!
private:
    Persp3D *_persp;
    Proj::Axis _axis;
};

struct VPDrag;

struct VPDragger {
public:
    VPDragger(VPDrag *parent, Geom::Point p, VanishingPoint &vp);
    ~VPDragger();

    VPDrag *parent;
    SPKnot *knot;

    // position of the knot, desktop coords
    Geom::Point point;
    // position of the knot before it began to drag; updated when released
    Geom::Point point_original;

    bool dragging_started;

    std::list<VanishingPoint> vps;

    void addVP(VanishingPoint &vp, bool update_pos = false);
    void removeVP(const VanishingPoint &vp);

    void updateTip();

    unsigned int numberOfBoxes(); // the number of boxes linked to all VPs of the dragger
    VanishingPoint *findVPWithBox(SPBox3D *box);
    std::set<VanishingPoint*> VPsOfSelectedBoxes();

    bool hasPerspective(const Persp3D *persp);
    void mergePerspectives(); // remove duplicate perspectives

    void updateBoxDisplays();
    void updateVPs(Geom::Point const &pt);
    void updateZOrders();

    void printVPs();

private:
    sigc::connection _moved_connection;
    sigc::connection _grabbed_connection;
    sigc::connection _ungrabbed_connection;
};

struct VPDrag {
public:
    VPDrag(SPDocument *document);
    ~VPDrag();

    VPDragger *getDraggerFor (VanishingPoint const &vp);

    bool dragging;

    SPDocument *document;
    std::vector<VPDragger *> draggers;
    std::vector<Inkscape::CanvasItemCurve *> item_curves;

    void printDraggers(); // convenience for debugging
    /*
     * FIXME: Should the following functions be merged?
     *        Also, they should make use of the info in a VanishingPoint structure (regarding boxes
     *        and perspectives) rather than each time iterating over the whole list of selected items?
     */
    void updateDraggers ();
    void updateLines ();
    void updateBoxHandles ();
    void updateBoxReprs ();
    void updateBoxDisplays ();
    void drawLinesForFace (const SPBox3D *box, Proj::Axis axis); //, guint corner1, guint corner2, guint corner3, guint corner4);
    bool show_lines; /* whether perspective lines are drawn at all */
    unsigned int front_or_rear_lines; /* whether we draw perspective lines from all corners or only the
                                  front/rear corners (indicated by the first/second bit, respectively  */


    inline bool hasEmptySelection() { return this->selection->isEmpty(); }
    bool allBoxesAreSelected (VPDragger *dragger);

    // FIXME: Should this be private? (It's the case with the corresponding function in gradient-drag.h)
    //        But vp_knot_grabbed_handler
    void addDragger (VanishingPoint &vp);

    void swap_perspectives_of_VPs(Persp3D *persp2, Persp3D *persp1);

private:
    //void deselect_all();

    /**
     * Create a line from p1 to p2 and add it to the item_curves list.
     */
    void addCurve(Geom::Point const &p1, Geom::Point const &p2, Inkscape::CanvasItemColor color);

    Inkscape::Selection *selection;
    sigc::connection sel_changed_connection;
    sigc::connection sel_modified_connection;
};

} // namespace Box3D


#endif /* !SEEN_VANISHING_POINT_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
