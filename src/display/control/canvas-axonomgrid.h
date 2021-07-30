// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef CANVAS_AXONOMGRID_H
#define CANVAS_AXONOMGRID_H

/*
 * Authors:
 *    Johan Engelen <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Copyright (C) 2006-2012 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "line-snapper.h"
#include "canvas-grid.h"

class SPNamedView;

namespace Inkscape {
class CanvasItemBuffer;
namespace XML {
    class Node;
};

class CanvasAxonomGrid : public CanvasGrid {
public:
    CanvasAxonomGrid(SPNamedView * nv, Inkscape::XML::Node * in_repr, SPDocument * in_doc);
    ~CanvasAxonomGrid() override;

    void Update (Geom::Affine const &affine, unsigned int flags) override;
    void Render (Inkscape::CanvasItemBuffer *buf) override;

    void readRepr() override;
    void onReprAttrChanged (Inkscape::XML::Node * repr, char const *key, char const *oldval, char const *newval, bool is_interactive) override;

    double lengthy;       /**< The lengths of the primary y-axis */
    double angle_deg[3];  /**< Angle of each axis (note that angle[2] == 0) */
    double angle_rad[3];  /**< Angle of each axis (note that angle[2] == 0) */
    double tan_angle[3];  /**< tan(angle[.]) */

    bool scaled;          /**< Whether the grid is in scaled mode */

protected:
    friend class CanvasAxonomGridSnapper;

    Geom::Point ow;         /**< Transformed origin by the affine for the zoom */
    double lyw   = 1.0;     /**< Transformed length y by the affine for the zoom */
    double lxw_x = 1.0;
    double lxw_z = 1.0;
    double spacing_ylines = 1.0;

    Geom::Point sw;          /**< the scaling factors of the affine transform */

    Gtk::Widget * newSpecificWidget() override;

private:
    CanvasAxonomGrid(const CanvasAxonomGrid&) = delete;
    CanvasAxonomGrid& operator=(const CanvasAxonomGrid&) = delete;

    void updateWidgets();

    Inkscape::UI::Widget::RegisteredUnitMenu *_rumg;
    Inkscape::UI::Widget::RegisteredScalarUnit *_rsu_ox;
    Inkscape::UI::Widget::RegisteredScalarUnit *_rsu_oy;
    Inkscape::UI::Widget::RegisteredScalarUnit *_rsu_sy;
    Inkscape::UI::Widget::RegisteredScalar *_rsu_ax;
    Inkscape::UI::Widget::RegisteredScalar *_rsu_az;
    Inkscape::UI::Widget::RegisteredColorPicker *_rcp_gcol;
    Inkscape::UI::Widget::RegisteredColorPicker *_rcp_gmcol;
    Inkscape::UI::Widget::RegisteredSuffixedInteger *_rsi;
};



class CanvasAxonomGridSnapper : public LineSnapper
{
public:
    CanvasAxonomGridSnapper(CanvasAxonomGrid *grid, SnapManager *sm, Geom::Coord const d);
    bool ThisSnapperMightSnap() const override;

    Geom::Coord getSnapperTolerance() const override; //returns the tolerance of the snapper in screen pixels (i.e. independent of zoom)
    bool getSnapperAlwaysSnap() const override; //if true, then the snapper will always snap, regardless of its tolerance

private:
    LineList _getSnapLines(Geom::Point const &p) const override;
    void _addSnappedLine(IntermSnapResults &isr, Geom::Point const &snapped_point, Geom::Coord const &snapped_distance, SnapSourceType const &source, long source_num, Geom::Point const &normal_to_line, const Geom::Point &point_on_line) const override;
    void _addSnappedPoint(IntermSnapResults &isr, Geom::Point const &snapped_point, Geom::Coord const &snapped_distance, SnapSourceType const &source, long source_num, bool constrained_snap) const override;
    void _addSnappedLinePerpendicularly(IntermSnapResults &isr, Geom::Point const &snapped_point, Geom::Coord const &snapped_distance, SnapSourceType const &source, long source_num, bool constrained_snap) const override;

    CanvasAxonomGrid *grid;
};


}; //namespace Inkscape



#endif


