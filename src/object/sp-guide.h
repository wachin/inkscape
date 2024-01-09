// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SPGuide -- a guideline
 *//*
 * Authors:
 *   Lauris Kaplinski 2000
 *   Johan Engelen 2007
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 * 
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_GUIDE_H
#define SEEN_SP_GUIDE_H

#include <2geom/point.h>
#include <vector>

#include "display/control/canvas-item-ptr.h"
#include "sp-object.h"

typedef unsigned int guint32;
extern "C" {
    typedef void (*GCallback) ();
}

class SPDesktop;

namespace Inkscape {
  class CanvasItemGroup;
  class CanvasItemGuideLine;

namespace UI::Widget {
  class Canvas;

}
} // namespace Inkscape


/* Represents the constraint on p that dot(g.direction, p) == g.position. */
class SPGuide final : public SPObject {
public:
    SPGuide();
    ~SPGuide() override = default;
    int tag() const override { return tag_of<decltype(*this)>; }

    void set_color(const unsigned r, const unsigned g, const unsigned b, bool const commit);
    void setColor(guint32 c);
    void setHiColor(guint32 h) { hicolor = h; }

    guint32 getColor() const { return color; }
    guint32 getHiColor() const { return hicolor; }
    Geom::Point getPoint() const { return point_on_line; }
    Geom::Point getNormal() const { return normal_to_line; }

    void moveto(Geom::Point const point_on_line, bool const commit);
    void set_normal(Geom::Point const normal_to_line, bool const commit);

    void set_label(const char* label, bool const commit);
    char const* getLabel() const { return label; }

    void set_locked(const bool locked, bool const commit);
    bool getLocked() const { return locked; }

    static SPGuide *createSPGuide(SPDocument *doc, Geom::Point const &pt1, Geom::Point const &pt2);
    SPGuide *duplicate();

    void showSPGuide(Inkscape::CanvasItemGroup *group);
    void hideSPGuide(Inkscape::UI::Widget::Canvas *canvas);
    void showSPGuide(); // argument-free versions
    void hideSPGuide();
    bool remove(bool force=false);

    void sensitize(Inkscape::UI::Widget::Canvas *canvas, bool sensitive);

    bool isHorizontal() const { return (normal_to_line[Geom::X] == 0.); };
    bool isVertical() const { return (normal_to_line[Geom::Y] == 0.); };

    char* description(bool const verbose = true) const;

    double angle() const { return std::atan2( - normal_to_line[Geom::X], normal_to_line[Geom::Y] ); }

protected:
    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
    void release() override;
    void set(SPAttr key, const char* value) override;

    char* label;
    std::vector<CanvasItemPtr<Inkscape::CanvasItemGuideLine>> views; // See display/control/guideline.h.
    bool locked;
    Geom::Point normal_to_line;
    Geom::Point point_on_line;

    guint32 color;
    guint32 hicolor;
};

// These functions rightfully belong to SPDesktop. What gives?!
void sp_guide_pt_pairs_to_guides(SPDocument *doc, std::list<std::pair<Geom::Point, Geom::Point> > &pts);
void sp_guide_create_guides_around_page(SPDocument *doc);
void sp_guide_delete_all_guides(SPDocument *doc);

#endif // SEEN_SP_GUIDE_H

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
