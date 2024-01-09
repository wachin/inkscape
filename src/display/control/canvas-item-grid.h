// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of GridCanvasItem.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_CANVAS_ITEM_GRID_H
#define SEEN_CANVAS_ITEM_GRID_H

#include <cstdint>
#include <2geom/point.h>

#include "canvas-item.h"
#include "preferences.h"

uint32_t constexpr GRID_DEFAULT_MAJOR_COLOR = 0x0099e54d;
uint32_t constexpr GRID_DEFAULT_MINOR_COLOR = 0x0099e526;

namespace Inkscape {

class CanvasItemGrid : public CanvasItem
{
public:
    CanvasItemGrid(CanvasItemGroup *group);

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Properties
    void set_major_color(uint32_t color);
    void set_minor_color(uint32_t color);
    void set_origin(Geom::Point const &point);
    void set_spacing(Geom::Point const &point);
    void set_dotted(bool b);
    void set_major_line_interval(int n);
    void set_no_emp_when_zoomed_out(bool noemp);

protected:
    ~CanvasItemGrid() override = default;

    bool _dotted;

    Geom::Point _origin;

    Geom::Point _spacing; /**< Spacing between elements of the grid */

    int _major_line_interval;
    bool _no_emp_when_zoomed_out;
    uint32_t _major_color;
    uint32_t _minor_color;

private:
    std::unique_ptr<Preferences::PreferencesObserver> _pref_tracker;
};

/** Canvas Item for rectangular grids */
class CanvasItemGridXY final : public CanvasItemGrid
{
public:
    CanvasItemGridXY(CanvasItemGroup *group);

protected:
    friend class GridSnapperXY;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;

    bool scaled[2];    /**< Whether the grid is in scaled mode, which can
                            be different in the X or Y direction, hence two
                            variables */
    Geom::Point ow;      /**< Transformed origin by the affine for the zoom */
    Geom::Point sw[2];   /**< Transformed spacing by the affine for the zoom */
};

/** Canvas Item for axonometric grids */
class CanvasItemGridAxonom final : public CanvasItemGrid
{
public:
    CanvasItemGridAxonom(CanvasItemGroup *group);

    // Properties
    void set_angle_x(double value);
    void set_angle_z(double value);

protected:
    friend class GridSnapperAxonom;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;

    bool scaled;          /**< Whether the grid is in scaled mode */

    double angle_deg[3];  /**< Angle of each axis (note that angle[2] == 0) */
    double angle_rad[3];  /**< Angle of each axis (note that angle[2] == 0) */
    double tan_angle[3];  /**< tan(angle[.]) */

    double lyw   = 1.0;     /**< Transformed length y by the affine for the zoom */
    double lxw_x = 1.0;
    double lxw_z = 1.0;
    double spacing_ylines = 1.0;

    Geom::Point ow;         /**< Transformed origin by the affine for the zoom */
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_GRID_H

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
