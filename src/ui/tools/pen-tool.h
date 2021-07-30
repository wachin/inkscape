// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * PenTool: a context for pen tool events.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_PEN_CONTEXT_H
#define SEEN_PEN_CONTEXT_H

#include "ui/tools/freehand-base.h"
#include "live_effects/effect.h"

#define SP_PEN_CONTEXT(obj) (dynamic_cast<Inkscape::UI::Tools::PenTool*>((Inkscape::UI::Tools::ToolBase*)obj))
#define SP_IS_PEN_CONTEXT(obj) (dynamic_cast<const Inkscape::UI::Tools::PenTool*>((const Inkscape::UI::Tools::ToolBase*)obj) != NULL)

namespace Inkscape {

class CanvasItemCtrl;
class CanvasItemCurve;

namespace UI {
namespace Tools {

/**
 * PenTool: a context for pen tool events.
 */
class PenTool : public FreehandBase {
public:
    PenTool(const std::string& cursor_filename = "pen.svg");
    PenTool(gchar const *const *cursor_shape);
    ~PenTool() override;

    enum Mode {
        MODE_CLICK,
        MODE_DRAG
    };

    enum State {
        POINT,
        CONTROL,
        CLOSE,
        STOP
    };

    Geom::Point p[5];
    Geom::Point previous;
    /** \invar npoints in {0, 2, 5}. */
    // npoints somehow determines the type of the node (what does it mean, exactly? the number of Bezier handles?)
    gint npoints = 0;

    Mode mode = MODE_CLICK;
    State state = POINT;
    bool polylines_only = false;
    bool polylines_paraxial = false;
    Geom::Point paraxial_angle;

    bool spiro = false;  // Spiro mode active?
    bool bspline = false; // BSpline mode active?
    int num_clicks = 0;;

    unsigned int expecting_clicks_for_LPE = 0; // if positive, finish the path after this many clicks
    Inkscape::LivePathEffect::Effect *waiting_LPE = nullptr; // if NULL, waiting_LPE_type in SPDrawContext is taken into account
    SPLPEItem *waiting_item = nullptr;

    Inkscape::CanvasItemCtrl *c0 = nullptr; // Start point of path.
    Inkscape::CanvasItemCtrl *c1 = nullptr; // End point of path.
    
    Inkscape::CanvasItemCurve *cl0 = nullptr;
    Inkscape::CanvasItemCurve *cl1 = nullptr;
    
    bool events_disabled = false;

    static const std::string prefsPath;

    const std::string& getPrefsPath() override;

    void nextParaxialDirection(Geom::Point const &pt, Geom::Point const &origin, guint state);
    void setPolylineMode();
    bool hasWaitingLPE();
    void waitForLPEMouseClicks(Inkscape::LivePathEffect::EffectType effect_type, unsigned int num_clicks, bool use_polylines = true);

protected:
    void setup() override;
    void finish() override;
    void set(const Inkscape::Preferences::Entry& val) override;
    bool root_handler(GdkEvent* event) override;
    bool item_handler(SPItem* item, GdkEvent* event) override;

private:
    bool _handleButtonPress(GdkEventButton const &bevent);
    bool _handleMotionNotify(GdkEventMotion const &mevent);
    bool _handleButtonRelease(GdkEventButton const &revent);
    bool _handle2ButtonPress(GdkEventButton const &bevent);
    bool _handleKeyPress(GdkEvent *event);
    //this function changes the colors red, green and blue making them transparent or not depending on if the function uses spiro
    void _bsplineSpiroColor();
    //creates a node in bspline or spiro modes
    void _bsplineSpiro(bool shift);
    //creates a node in bspline or spiro modes
    void _bsplineSpiroOn();
    //creates a CUSP node
    void _bsplineSpiroOff();
    //continues the existing curve in bspline or spiro mode
    void _bsplineSpiroStartAnchor(bool shift);
    //continues the existing curve with the union node in bspline or spiro modes
    void _bsplineSpiroStartAnchorOn();
    //continues an existing curve with the union node in CUSP mode
    void _bsplineSpiroStartAnchorOff();
    //modifies the "red_curve" when it detects movement
    void _bsplineSpiroMotion(guint const state);
    //closes the curve with the last node in bspline or spiro mode
    void _bsplineSpiroEndAnchorOn();
    //closes the curve with the last node in CUSP mode
    void _bsplineSpiroEndAnchorOff();
    //apply the effect
    void _bsplineSpiroBuild();

    void _setInitialPoint(Geom::Point const p);
    void _setSubsequentPoint(Geom::Point const p, bool statusbar, guint status = 0);
    void _setCtrl(Geom::Point const p, guint state);
    void _finishSegment(Geom::Point p, guint state);
    bool _undoLastPoint();

    void _finish(gboolean closed);

    void _resetColors();

    void _disableEvents();
    void _enableEvents();

    void _setToNearestHorizVert(Geom::Point &pt, guint const state) const;

    void _setAngleDistanceStatusMessage(Geom::Point const p, int pc_point_to_compare, gchar const *message);

    void _lastpointToLine();
    void _lastpointToCurve();
    void _lastpointMoveScreen(gdouble x, gdouble y);
    void _lastpointMove(gdouble x, gdouble y);
    void _redrawAll();

    void _endpointSnapHandle(Geom::Point &p, guint const state);
    void _endpointSnap(Geom::Point &p, guint const state);

    void _cancel();
};

}
}
}

#endif /* !SEEN_PEN_CONTEXT_H */

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
