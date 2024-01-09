// SPDX-License-Identifier: GPL-2.0-or-later
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
 */

#include "canvas-item.h"
#include "canvas-item-group.h"
#include "canvas-item-ctrl.h"

#include "ui/widget/canvas.h"

constexpr bool DEBUG_LOGGING = false;
constexpr bool DEBUG_BOUNDS = false;

namespace Inkscape {

CanvasItem::CanvasItem(CanvasItemContext *context)
    : _context(context)
    , _parent(nullptr)
{
    if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem: create root " << get_name() << std::endl;
    request_update();
}

CanvasItem::CanvasItem(CanvasItemGroup *parent)
    : _context(parent->_context)
    , _parent(parent)
{
    if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem: add " << get_name() << " to " << parent->get_name() << " " << parent->items.size() << std::endl;
    defer([=] {
        parent->items.push_back(*this);
        request_update();
    });
}

void CanvasItem::unlink()
{
    defer([=] {
        // Clear canvas of item.
        request_redraw();

        // Remove from parent.
        if (_parent) {
            if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem: remove " << get_name() << " from " << _parent->get_name() << " " << _parent->items.size() << std::endl;
            auto it = _parent->items.iterator_to(*this);
            assert(it != _parent->items.end());
            _parent->items.erase(it);
            _parent->request_update();
        } else {
            if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem: destroy root " << get_name() << std::endl;
        }

        delete this;
    });
}

CanvasItem::~CanvasItem()
{
    // Clear any pointers to this object in canvas.
    get_canvas()->canvas_item_destructed(this);
}

bool CanvasItem::is_descendant_of(CanvasItem const *ancestor) const
{
    auto item = this;
    while (item) {
        if (item == ancestor) {
            return true;
        }
        item = item->_parent;
    }
    return false;
}

void CanvasItem::set_z_position(int zpos)
{
    if (!_parent) {
        std::cerr << "CanvasItem::set_z_position: No parent!" << std::endl;
        return;
    }

    defer([=] {
        _parent->items.erase(_parent->items.iterator_to(*this));

        if (zpos <= 0) {
            _parent->items.push_front(*this);
        } else if (zpos >= _parent->items.size() - 1) {
            _parent->items.push_back(*this);
        } else {
            auto it = _parent->items.begin();
            std::advance(it, zpos);
            _parent->items.insert(it, *this);
        }
    });
}

void CanvasItem::raise_to_top()
{
    if (!_parent) {
        std::cerr << "CanvasItem::raise_to_top: No parent!" << std::endl;
        return;
    }

    defer([=] {
        _parent->items.erase(_parent->items.iterator_to(*this));
        _parent->items.push_back(*this);
    });
}

void CanvasItem::lower_to_bottom()
{
    if (!_parent) {
        std::cerr << "CanvasItem::lower_to_bottom: No parent!" << std::endl;
        return;
    }

    defer([=] {
        _parent->items.erase(_parent->items.iterator_to(*this));
        _parent->items.push_front(*this);
    });
}

// Indicate geometry changed and bounds needs recalculating.
void CanvasItem::request_update()
{
    if (_need_update || !_visible) {
        return;
    }

    _need_update = true;

    if (_parent) {
        _parent->request_update();
    } else {
        get_canvas()->request_update();
    }
}

void CanvasItem::update(bool propagate)
{
    if (!_visible) {
        _mark_net_invisible();
        return;
    }

    bool reappearing = !_net_visible;
    _net_visible = true;

    if (!_need_update && !reappearing && !propagate) {
        return;
    }

    _need_update = false;

    // Get new bounds
    _update(propagate);

    if (reappearing) {
        request_redraw();
    }
}

void CanvasItem::_mark_net_invisible()
{
    if (!_net_visible) {
        return;
    }
    _net_visible = false;
    _need_update = false;
    request_redraw();
    _bounds = {};
}

// Grab all events!
void CanvasItem::grab(Gdk::EventMask event_mask, Glib::RefPtr<Gdk::Cursor> const &cursor)
{
    if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem::grab: " << _name << std::endl;

    auto canvas = get_canvas();

    // Don't grab if we already have a grabbed item!
    if (canvas->get_grabbed_canvas_item()) {
        return;
    }

    gtk_grab_add(GTK_WIDGET(canvas->gobj()));

    canvas->set_grabbed_canvas_item(this, event_mask);
    canvas->set_current_canvas_item(this); // So that all events go to grabbed item.
}

void CanvasItem::ungrab()
{
    if constexpr (DEBUG_LOGGING) std::cout << "CanvasItem::ungrab: " << _name << std::endl;

    auto canvas = get_canvas();

    if (canvas->get_grabbed_canvas_item() != this) {
        return; // Sanity check
    }

    canvas->set_grabbed_canvas_item(nullptr, (Gdk::EventMask)0); // Zero mask

    gtk_grab_remove(GTK_WIDGET(canvas->gobj()));
}

void CanvasItem::render(CanvasItemBuffer &buf) const
{
    if (_visible && _bounds && _bounds->interiorIntersects(buf.rect)) {
        _render(buf);
        if constexpr (DEBUG_BOUNDS) {
            auto bounds = *_bounds;
            bounds.expandBy(-1);
            bounds -= buf.rect.min();
            buf.cr->set_source_rgba(1.0, 0.0, 0.0, 1.0);
            buf.cr->rectangle(bounds.min().x(), bounds.min().y(), bounds.width(), bounds.height());
            buf.cr->stroke();
        }
    }
}

/*
 * The main invariant of the invisibility system is
 *
 *     x needs update and is visible  ==>  parent(x) needs update or is invisible
 *
 * When x belongs to the visible subtree, meaning it and all its parents are visible,
 * this condition reduces to
 *
 *     x needs update  ==>  parent(x) needs update
 *
 * Thus within the visible subtree, the subset of nodes that need updating forms a subtree.
 *
 * In the update() function, we only walk this latter subtree.
 */

void CanvasItem::set_visible(bool visible)
{
    defer([=] {
        if (_visible == visible) return;
        if (_visible) {
            request_update();
            _visible = false;
        } else {
            _visible = true;
            _need_update = false;
            request_update();
        }
    });
}

void CanvasItem::request_redraw()
{
    // Queue redraw request
    if (_bounds) {
        get_canvas()->redraw_area(*_bounds);
    }
}

void CanvasItem::set_fill(uint32_t fill)
{
    defer([=] {
        if (_fill == fill) return;
        _fill = fill;
        request_redraw();
    });
}

void CanvasItem::set_stroke(uint32_t stroke)
{
    defer([=] {
        if (_stroke == stroke) return;
        _stroke = stroke;
        request_redraw();
    });
}

void CanvasItem::update_canvas_item_ctrl_sizes(int size_index)
{
    if (auto ctrl = dynamic_cast<CanvasItemCtrl*>(this)) {
        // We can't use set_size_default as the preference file is updated ->after<- the signal is emitted!
        ctrl->set_size_via_index(size_index);
    } else if (auto group = dynamic_cast<CanvasItemGroup*>(this)) {
        for (auto &item : group->items) {
            item.update_canvas_item_ctrl_sizes(size_index);
        }
    }
}

void CanvasItem::canvas_item_print_tree(int level, int zorder) const
{
    if (level == 0) {
        std::cout << "Canvas Item Tree" << std::endl;
    }

    std::cout << "CC: ";
    for (int i = 0; i < level; ++i) {
        std::cout << "  ";
    }

    std::cout << zorder << ": " << _name << std::endl;

    if (auto group = dynamic_cast<Inkscape::CanvasItemGroup const*>(this)) {
        int i = 0;
        for (auto &item : group->items) {
            item.canvas_item_print_tree(level + 1, i);
            i++;
        }
    }
}

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
