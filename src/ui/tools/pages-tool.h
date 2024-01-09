// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef __UI_TOOLS_PAGES_CONTEXT_H__
#define __UI_TOOLS_PAGES_CONTEXT_H__

/*
 * Page editing tool
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tools/tool-base.h"
#include "2geom/rect.h"
#include "display/control/canvas-item-ptr.h"

#define SP_PAGES_CONTEXT(obj) (dynamic_cast<Inkscape::UI::Tools::PagesTool *>((Inkscape::UI::Tools::ToolBase *)obj))
#define SP_IS_PAGES_CONTEXT(obj) \
    (dynamic_cast<const Inkscape::UI::Tools::PagesTool *>((const Inkscape::UI::Tools::ToolBase *)obj) != NULL)

class SPDocument;
class SPObject;
class SPPage;
class SPKnot;
class SnapManager;

namespace Inkscape {
class SnapCandidatePoint;
class CanvasItemGroup;
class CanvasItemRect;
class CanvasItemBpath;

namespace UI {
namespace Tools {

class PagesTool : public ToolBase
{
public:
    PagesTool(SPDesktop *desktop);
    ~PagesTool() override;

    bool root_handler(GdkEvent *event) override;
    void menu_popup(GdkEvent *event, SPObject *obj = nullptr) override;
private:
    void selectionChanged(SPDocument *doc, SPPage *page);
    void connectDocument(SPDocument *doc);
    SPPage *pageUnder(Geom::Point pt, bool retain_selected = true);
    bool viewboxUnder(Geom::Point pt);
    void addDragShapes(SPPage *page, Geom::Affine tr);
    void addDragShape(SPItem *item, Geom::Affine tr);
    void addDragShape(Geom::PathVector &&pth, Geom::Affine tr);
    void clearDragShapes();

    Geom::Point getSnappedResizePoint(Geom::Point point, guint state, Geom::Point origin, SPObject *target = nullptr);
    void resizeKnotSet(Geom::Rect rect);
    void resizeKnotMoved(SPKnot *knot, Geom::Point const &ppointer, guint state);
    void resizeKnotFinished(SPKnot *knot, guint state);
    void pageModified(SPObject *object, guint flags);

    void marginKnotSet(Geom::Rect margin_rect);
    bool marginKnotMoved(SPKnot *knot, Geom::Point *point, guint state);
    void marginKnotFinished(SPKnot *knot, guint state);

    void grabPage(SPPage *target);
    Geom::Affine moveTo(Geom::Point xy, bool snap);

    sigc::connection _selector_changed_connection;
    sigc::connection _page_modified_connection;
    sigc::connection _doc_replaced_connection;
    sigc::connection _zoom_connection;

    bool dragging_viewbox = false;
    bool mouse_is_pressed = false;
    Geom::Point drag_origin_w;
    Geom::Point drag_origin_dt;
    int drag_tolerance = 5;

    std::vector<SPKnot *> resize_knots;
    std::vector<SPKnot *> margin_knots;
    SPKnot *grabbed_knot = nullptr;
    SPPage *highlight_item = nullptr;
    SPPage *dragging_item = nullptr;
    std::optional<Geom::Rect> on_screen_rect; ///< On-screen rectangle, in desktop coordinates.
    CanvasItemPtr<CanvasItemRect> visual_box;
    CanvasItemPtr<CanvasItemGroup> drag_group;
    std::vector<Inkscape::CanvasItemBpath *> drag_shapes;
    std::vector<Inkscape::SnapCandidatePoint> _bbox_points;

    static Geom::Point middleOfSide(int side, const Geom::Rect &rect);
};

} // namespace Tools
} // namespace UI
} // namespace Inkscape

#endif
