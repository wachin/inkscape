// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_TEXT_H
#define SEEN_CANVAS_ITEM_TEXT_H

/**
 * A class to represent on-screen text.
 */

/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCanvasText.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/point.h>
#include <2geom/transforms.h>

#include <glibmm/ustring.h>

#include "canvas-item.h"

namespace Inkscape {

class CanvasItemText final : public CanvasItem
{
public:
    CanvasItemText(CanvasItemGroup *group);
    CanvasItemText(CanvasItemGroup *group, Geom::Point const &p, Glib::ustring text, bool scaled = false);

    // Geometry
    void set_coord(Geom::Point const &p);
    void set_bg_radius(double rad);

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Properties
    void set_text(Glib::ustring text);
    void set_fontsize(double fontsize);
    void set_border(double border);
    void set_background(uint32_t background);
    void set_anchor(Geom::Point const &anchor_pt);
    void set_adjust(Geom::Point const &adjust_pt);
    void set_fixed_line(bool fixed_line);

protected:
    ~CanvasItemText() override = default;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    Geom::Point _p;  // Position of text (not box around text).
    Cairo::TextExtents _text_extent;
    Cairo::TextExtents _text_size;
    Geom::Point _anchor_position;
    Geom::Point _adjust_offset;
    Geom::Rect _text_box;
    Glib::ustring _text;
    std::string _fontname = "sans-serif";
    double _fontsize = 10;
    double _border = 3;
    double _bg_rad = 0;
    uint32_t _background = 0x0000007f;
    bool _use_background = false;
    bool _fixed_line = false; // Correction for font heights
    bool _scaled = false;

    Geom::Rect load_text_extents();
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_TEXT_H

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
