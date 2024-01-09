// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_H
#define SEEN_CANVAS_ITEM_H

/**
 * Abstract base class for on-canvas control items.
 */

/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCanvasItem
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 * A note about coordinates:
 *
 *   1. Canvas items are constructed using document (SVG) coordinates.
 *   2. Calculations are made in canvas units, which is equivalent of SVG units multiplied by zoom factor.
 *      This is true for bounds and closest distance calculations.
 *   3  Drawing is done in screen units which is the same as canvas units but translated.
 *   The document and canvas origins overlap.
 *   The affine contains only scaling and rotating components.
 */

#include <cstdint>
#include <boost/intrusive/list.hpp>
#include <2geom/rect.h>
#include <sigc++/sigc++.h>

#include <gdkmm/device.h> // Gdk::EventMask
#include <gdk/gdk.h>  // GdkEvent

#include "canvas-item-enums.h"
#include "canvas-item-buffer.h"
#include "canvas-item-context.h"

class SPItem;

namespace Inkscape {

inline constexpr uint32_t CANVAS_ITEM_COLORS[] = { 0x0000ff7f, 0xff00007f, 0xffff007f };

namespace UI::Widget { class Canvas; }
class CanvasItemGroup;

class CanvasItem
{
public:
    CanvasItem(CanvasItemContext *context);
    CanvasItem(CanvasItemGroup *parent);
    CanvasItem(CanvasItem const &) = delete;
    CanvasItem &operator=(CanvasItem const &) = delete;
    void unlink();

    // Structure
    UI::Widget::Canvas *get_canvas() const { return _context->canvas(); }
    CanvasItemGroup *get_parent() const { return _parent; }
    bool is_descendant_of(CanvasItem const *ancestor) const;

    // Z Position
    void set_z_position(int zpos);
    void raise_to_top();    // Move to top of group (last entry).
    void lower_to_bottom(); // Move to bottom of group (first entry).

    // Geometry
    void request_update();
    void update(bool propagate);
    virtual void visit_page_rects(std::function<void(Geom::Rect const &)> const &) const {}
    Geom::OptRect const &get_bounds() const { return _bounds; }

    // Selection
    virtual bool contains(Geom::Point const &p, double tolerance = 0) { return _bounds && _bounds->interiorContains(p); }
    void grab(Gdk::EventMask event_mask, Glib::RefPtr<Gdk::Cursor> const & = {});
    void ungrab();

    // Display
    void render(Inkscape::CanvasItemBuffer &buf) const;
    bool is_visible() const { return _visible; }
    virtual void set_visible(bool visible);
    void show() { set_visible(true); }
    void hide() { set_visible(false); }
    void request_redraw(); // queue redraw request

    // Properties
    virtual void set_fill(uint32_t rgba);
    void set_fill(CanvasItemColor color) { set_fill(CANVAS_ITEM_COLORS[color]); }
    virtual void set_stroke(uint32_t rgba);
    void set_stroke(CanvasItemColor color) { set_stroke(CANVAS_ITEM_COLORS[color]); }
    void set_name(std::string &&name) { _name = std::move(name); }
    std::string const &get_name() const { return _name; }
    void update_canvas_item_ctrl_sizes(int size_index);

    // Events
    void set_pickable(bool pickable) { _pickable = pickable; }
    bool is_pickable() const { return _pickable; }
    sigc::connection connect_event(sigc::slot<bool(GdkEvent*)> const &slot) {
        return _event_signal.connect(slot);
    }
    virtual bool handle_event(GdkEvent *event) {
        return _event_signal.emit(event); // Default just emits event.
    }

    // Recursively print CanvasItem tree.
    void canvas_item_print_tree(int level = 0, int zorder = 0) const;

    // Boost linked list member hook, speeds deletion.
    boost::intrusive::list_member_hook<> member_hook;

protected:
    friend class CanvasItemGroup;

    virtual ~CanvasItem();

    // Structure
    CanvasItemContext *_context;
    CanvasItemGroup *_parent;

    // Geometry
    Geom::OptRect _bounds;
    bool _need_update = false;
    Geom::Affine const &affine() const { return _context->affine(); }
    virtual void _update(bool propagate) = 0;
    virtual void _mark_net_invisible();

    // Display
    bool _visible = true;
    bool _net_visible = true;
    virtual void _render(Inkscape::CanvasItemBuffer &buf) const = 0;

    // Selection
    bool _pickable = false; // Most items are just for display and are not pickable!

    // Properties
    uint32_t _fill    = CANVAS_ITEM_COLORS[CANVAS_ITEM_SECONDARY];
    uint32_t _stroke  = CANVAS_ITEM_COLORS[CANVAS_ITEM_PRIMARY];
    std::string _name; // For debugging

    // Events
    sigc::signal<bool (GdkEvent*)> _event_signal;

    // Snapshotting
    template<typename F>
    void defer(F &&f) { _context->defer(std::forward<F>(f)); }
};

} // namespace Inkscape

// Todo: Move to lib2geom.
inline auto &operator<<(std::ostream &s, Geom::OptRect const &rect)
{
    return rect ? (s << *rect) : (s << "(empty)");
}

#endif // SEEN_CANVAS_ITEM_H

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
