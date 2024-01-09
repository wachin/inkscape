// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Gradient image widget with stop handles
 *
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2020-2021 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>

#include "gradient-with-stops.h"
#include "object/sp-gradient.h"
#include "object/sp-stop.h"
#include "display/cairo-utils.h"
#include "io/resource.h"
#include "ui/cursor-utils.h"
#include "ui/util.h"

// widget's height; it should take stop template's height into account
// current value is fine-tuned to make stop handles overlap gradient image just the right amount
const int GRADIENT_WIDGET_HEIGHT = 33;
// gradient's image height (multiple of checkerboard tiles, they are 6x6)
const int GRADIENT_IMAGE_HEIGHT = 3 * 6;

namespace Inkscape {
namespace UI {
namespace Widget {

using namespace Inkscape::IO;

std::string get_stop_template_path(const char* filename) {
    // "stop handle" template files path
    return Resource::get_filename(Resource::UIS, filename);
}

GradientWithStops::GradientWithStops() :
    _template(get_stop_template_path("gradient-stop.svg").c_str()),
    _tip_template(get_stop_template_path("gradient-tip.svg").c_str())
    {
    // default color, it will be updated
    _background_color.set_grey(0.5);
    // for theming, but not used
    set_name("GradientEdit");
    // we need some events
    add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::BUTTON_MOTION_MASK |
        Gdk::POINTER_MOTION_MASK | Gdk::KEY_PRESS_MASK);
    set_can_focus();
}

void GradientWithStops::set_gradient(SPGradient* gradient) {
    _gradient = gradient;

    // listen to release & changes
    _release  = gradient ? gradient->connectRelease([=](SPObject*){ set_gradient(nullptr); }) : sigc::connection();
    _modified = gradient ? gradient->connectModified([=](SPObject*, guint){ modified(); }) : sigc::connection();

    // TODO: check selected/focused stop index

    modified();

    set_sensitive(gradient != nullptr);
}

void GradientWithStops::modified() {
    // gradient has been modified

    // read all stops
    _stops.clear();

    if (_gradient) {
        SPStop* stop = _gradient->getFirstStop();
        while (stop) {
            _stops.push_back(stop_t {
                .offset = stop->offset, .color = stop->getColor(), .opacity = stop->getOpacity()
            });
            stop = stop->getNextStop();
        }
    }

    update();
}

void GradientWithStops::size_request(GtkRequisition* requisition) const {
    requisition->width = 60;
    requisition->height = GRADIENT_WIDGET_HEIGHT;
}

void GradientWithStops::get_preferred_width_vfunc(int& minimal_width, int& natural_width) const {
    GtkRequisition requisition;
    size_request(&requisition);
    minimal_width = natural_width = requisition.width;
}

void GradientWithStops::get_preferred_height_vfunc(int& minimal_height, int& natural_height) const {
    GtkRequisition requisition;
    size_request(&requisition);
    minimal_height = natural_height = requisition.height;
}

void GradientWithStops::update() {
    if (get_is_drawable()) {
        queue_draw();
    }
}

// capture background color when styles change
void GradientWithStops::on_style_updated() {
    if (auto wnd = dynamic_cast<Gtk::Window*>(this->get_toplevel())) {
        auto sc = wnd->get_style_context();
        _background_color = get_background_color(sc);
    }

    // load and cache cursors
    auto wnd = get_window();
    if (wnd && !_cursor_mouseover) {
        // use standard cursors:
        _cursor_mouseover = Gdk::Cursor::create(get_display(), "grab");
        _cursor_dragging =  Gdk::Cursor::create(get_display(), "grabbing");
        _cursor_insert =    Gdk::Cursor::create(get_display(), "crosshair");
        // or custom cursors:
        // _cursor_mouseover = load_svg_cursor(get_display(), wnd, "gradient-over-stop.svg");
        // _cursor_dragging  = load_svg_cursor(get_display(), wnd, "gradient-drag-stop.svg");
        // _cursor_insert    = load_svg_cursor(get_display(), wnd, "gradient-add-stop.svg");
        wnd->set_cursor();
    }
}

void draw_gradient(const Cairo::RefPtr<Cairo::Context>& cr, SPGradient* gradient, int x, int width) {
    cairo_pattern_t* check = ink_cairo_pattern_create_checkerboard();

    cairo_set_source(cr->cobj(), check);
    cr->fill_preserve();
    cairo_pattern_destroy(check);

    if (gradient) {
        auto p = gradient->create_preview_pattern(width);
        cairo_matrix_t m;
        cairo_matrix_init_translate(&m, -x, 0);
        cairo_pattern_set_matrix(p, &m);
        cairo_set_source(cr->cobj(), p);
        cr->fill();
        cairo_pattern_destroy(p);
    }
}

// return on-screen position of the UI stop corresponding to the gradient's color stop at 'index'
GradientWithStops::stop_pos_t GradientWithStops::get_stop_position(size_t index, const layout_t& layout) const {
    if (!_gradient || index >= _stops.size()) {
        return stop_pos_t {};
    }

    // half of the stop template width; round it to avoid half-pixel coordinates
    const auto dx = round((_template.get_width_px() + 1) / 2);

    auto pos = [&](double offset) { return round(layout.x + layout.width * CLAMP(offset, 0, 1)); };
    const auto& v = _stops;

    auto offset = pos(v[index].offset);
    auto left = offset - dx;
    if (index > 0) {
        // check previous stop; it may overlap
        auto prev = pos(v[index - 1].offset) + dx;
        if (prev > left) {
            // overlap
            left = round((left + prev) / 2);
        }
    }

    auto right = offset + dx;
    if (index + 1 < v.size()) {
        // check next stop for overlap
        auto next = pos(v[index + 1].offset) - dx;
        if (right > next) {
            // overlap
            right = round((right + next) / 2);
        }
    }

    return stop_pos_t {
        .left = left,
        .tip = offset,
        .right = right,
        .top = layout.height - _template.get_height_px(),
        .bottom = layout.height
    };
}

// widget's layout; mainly location of the gradient's image and stop handles
GradientWithStops::layout_t GradientWithStops::get_layout() const {
    auto allocation = get_allocation();

    const auto stop_width = _template.get_width_px();
    const auto half_stop = round((stop_width + 1) / 2);
    const auto x = half_stop;
    const double width = allocation.get_width() - stop_width;
    const double height = allocation.get_height();

    return layout_t {
        .x = x,
        .y = 0,
        .width = width,
        .height = height
    };
}

// check if stop handle is under (x, y) location, return its index or -1 if not hit
int GradientWithStops::find_stop_at(double x, double y) const {
    if (!_gradient) return -1;

    const auto& v = _stops;
    const auto& layout = get_layout();

    // find stop handle at (x, y) position; note: stops may not be ordered by offsets
    for (size_t i = 0; i < v.size(); ++i) {
        auto pos = get_stop_position(i, layout);
        if (x >= pos.left && x <= pos.right && y >= pos.top && y <= pos.bottom) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

// this is range of offset adjustment for a given stop
GradientWithStops::limits_t GradientWithStops::get_stop_limits(int maybe_index) const {
    if (!_gradient) return limits_t {};

    // let negative index turn into a large out-of-range number
    auto index = static_cast<size_t>(maybe_index);

    const auto& v = _stops;

    if (index < v.size()) {
        double min = 0;
        double max = 1;

        if (v.size() > 1) {
                std::vector<double> offsets;
                offsets.reserve(v.size());
                for (auto& s : _stops) {
                    offsets.push_back(s.offset);
                }
                std::sort(offsets.begin(), offsets.end());

            // special cases:
            if (index == 0) { // first stop
                max = offsets[index + 1];
            }
            else if (index + 1 == v.size()) { // last stop
                min = offsets[index - 1];
            }
            else {
                // stops "inside" gradient
                min = offsets[index - 1];
                max = offsets[index + 1];
            }
        }
        return limits_t { .min_offset = min, .max_offset = max, .offset = v[index].offset };
    }
    else {
        return limits_t {};
    }
}

bool GradientWithStops::on_focus_out_event(GdkEventFocus* event) {
    update();
    return false;
}

bool GradientWithStops::on_focus_in_event(GdkEventFocus* event) {
    update();
    return false;
}

bool GradientWithStops::on_focus(Gtk::DirectionType direction) {
    if (has_focus()) {
        return false; // let focus go
    }

    grab_focus();
    // TODO - add focus indicator frame or some focus indicator
    return true;
}

bool GradientWithStops::on_key_press_event(GdkEventKey* key_event) {
    bool consumed = false;
    // currently all keyboard activity involves acting on focused stop handle; bail if nothing's selected
    if (_focused_stop < 0) return consumed;

    unsigned int key = 0;
    auto modifier = static_cast<GdkModifierType>(key_event->state);
    gdk_keymap_translate_keyboard_state(Gdk::Display::get_default()->get_keymap(),
        key_event->hardware_keycode, modifier, 0, &key, nullptr, nullptr, nullptr);

    auto delta = _stop_move_increment;
    if (modifier & GDK_SHIFT_MASK) {
        delta *= 10;
    }

    consumed = true;

    switch (key) {
        case GDK_KEY_Left:
        case GDK_KEY_KP_Left:
            move_stop(_focused_stop, -delta);
            break;
        case GDK_KEY_Right:
        case GDK_KEY_KP_Right:
            move_stop(_focused_stop, delta);
            break;
        case GDK_KEY_BackSpace:
        case GDK_KEY_Delete:
            _signal_delete_stop.emit(_focused_stop);
            break;
        default:
            consumed = false;
            break;
    }

    return consumed;
}

bool GradientWithStops::on_button_press_event(GdkEventButton* event) {
    // single button press selects stop and can start dragging it
    constexpr auto LEFT_BTN = 1;
    if (event->button == LEFT_BTN && _gradient && event->type == GDK_BUTTON_PRESS) {
        _focused_stop = -1;

        if (!has_focus()) {
            // grab focus, so we can show selection indicator and move selected stop with left/right keys
            grab_focus();
        }
        update();

        // find stop handle
        auto index = find_stop_at(event->x, event->y);

        if (index >= 0) {
            _focused_stop = index;
            // fire stop selection, whether stop can be moved or not
            _signal_stop_selected.emit(index);

            auto limits = get_stop_limits(index);

            // check if clicked stop can be moved
            if (limits.min_offset < limits.max_offset) {
                // TODO: to facilitate selecting stops without accidentally moving them,
                // delay dragging mode until mouse cursor moves certain distance...
                _dragging = true;
                _pointer_x = event->x;
                _stop_offset = _stops.at(index).offset;

                if (_cursor_dragging) {
                    gdk_window_set_cursor(event->window, _cursor_dragging->gobj());
                }
            }
        }
    }
    else if (event->button == LEFT_BTN && _gradient && event->type == GDK_2BUTTON_PRESS) {
        // double-click may insert a new stop
        auto index = find_stop_at(event->x, event->y);
        if (index < 0) {
            auto layout = get_layout();
            if (layout.width > 0 && event->x > layout.x && event->x < layout.x + layout.width) {
                double position = (event->x - layout.x) / layout.width;
                // request new stop
                _signal_add_stop_at.emit(position);
            }
        }
    }

    return false;
}

bool GradientWithStops::on_button_release_event(GdkEventButton* event) {
    GdkCursor* cursor = get_cursor(event->x, event->y);
    gdk_window_set_cursor(event->window, cursor);

    _dragging = false;
    return false;
}

// move stop by a given amount (delta)
void GradientWithStops::move_stop(int stop_index, double offset_shift) {
    auto layout = get_layout();
    if (layout.width > 0) {
        auto limits = get_stop_limits(stop_index);
        if (limits.min_offset < limits.max_offset) {
            auto new_offset = CLAMP(limits.offset + offset_shift, limits.min_offset, limits.max_offset);
            if (new_offset != limits.offset) {
                _signal_stop_offset_changed.emit(stop_index, new_offset);
            }
        }
    }
}

bool GradientWithStops::on_motion_notify_event(GdkEventMotion* event) {
    if (_dragging && _gradient) {
        // move stop to a new position (adjust offset)
        auto dx = event->x - _pointer_x;
        auto layout = get_layout();
        if (layout.width > 0) {
            auto delta = dx / layout.width;
            auto limits = get_stop_limits(_focused_stop);
            if (limits.min_offset < limits.max_offset) {
                auto new_offset = CLAMP(_stop_offset + delta, limits.min_offset, limits.max_offset);
                _signal_stop_offset_changed.emit(_focused_stop, new_offset);
            }
        }
    }
    else if (!_dragging && _gradient) {
        GdkCursor* cursor = get_cursor(event->x, event->y);
        gdk_window_set_cursor(event->window, cursor);
    }

    return false;
}

GdkCursor* GradientWithStops::get_cursor(double x, double y) const {
    GdkCursor* cursor = nullptr;
    if (_gradient) {
        // check if mouse if over stop handle that we can adjust
        auto index = find_stop_at(x, y);
        if (index >= 0) {
            auto limits = get_stop_limits(index);
            if (limits.min_offset < limits.max_offset && _cursor_mouseover) {
                cursor = _cursor_mouseover->gobj();
            }
        }
        else {
            if (_cursor_insert) {
                cursor = _cursor_insert->gobj();
            }
        }
    }
    return cursor;
}

bool GradientWithStops::on_draw(const Cairo::RefPtr<Cairo::Context>& cr) {
    auto allocation = get_allocation();
    auto context = get_style_context();
    const double scale = get_scale_factor();
    const auto layout = get_layout();

    if (layout.width <= 0) return true;

    context->render_background(cr, 0, 0, allocation.get_width(), allocation.get_height());

    // empty gradient checkboard or gradient itself
    cr->rectangle(layout.x, layout.y, layout.width, GRADIENT_IMAGE_HEIGHT);
    draw_gradient(cr, _gradient, layout.x, layout.width);

    if (!_gradient) return true;

    // draw stop handles

    cr->begin_new_path();

    Gdk::RGBA fg = context->get_color(get_state_flags());
    Gdk::RGBA bg = _background_color;

    // stop handle outlines and selection indicator use theme colors:
    _template.set_style(".outer", "fill", rgba_to_css_color(fg));
    _template.set_style(".inner", "stroke", rgba_to_css_color(bg));
    _template.set_style(".hole", "fill", rgba_to_css_color(bg));

    auto tip = _tip_template.render(scale);

    for (size_t i = 0; i < _stops.size(); ++i) {
        const auto& stop = _stops[i];

        // stop handle shows stop color and opacity:
        _template.set_style(".color", "fill", rgba_to_css_color(stop.color));
        _template.set_style(".opacity", "opacity", double_to_css_value(stop.opacity));

        // show/hide selection indicator
        const auto is_selected = _focused_stop == static_cast<int>(i);
        _template.set_style(".selected", "opacity", double_to_css_value(is_selected ? 1 : 0));

        // render stop handle
        auto pix = _template.render(scale);

        if (!pix) {
            g_warning("Rendering gradient stop failed.");
            break;
        }

        auto pos = get_stop_position(i, layout);

        // selected handle sports a 'tip' to make it easily noticeable
        if (is_selected && tip) {
            if (auto surface = Gdk::Cairo::create_surface_from_pixbuf(tip, 1)) {
                cr->save();
                // scale back to physical pixels
                cr->scale(1 / scale, 1 / scale);
                // paint tip bitmap
                cr->set_source(surface, round(pos.tip * scale - tip->get_width() / 2), layout.y * scale);
                cr->paint();
                cr->restore();
            }
        }

        // surface from pixbuf *without* scaling (scale = 1)
        auto surface = Gdk::Cairo::create_surface_from_pixbuf(pix, 1);
        if (!surface) continue;

        // calc space available for stop marker
        cr->save();
        cr->rectangle(pos.left, layout.y, pos.right - pos.left, layout.height);
        cr->clip();
        // scale back to physical pixels
        cr->scale(1 / scale, 1 / scale);
        // paint bitmap
        cr->set_source(surface, round(pos.tip * scale - pix->get_width() / 2), pos.top * scale);
        cr->paint();
        cr->restore();
        cr->reset_clip();
    }

    return true;
}

// focused/selected stop indicator
void GradientWithStops::set_focused_stop(int index) {
    if (_focused_stop != index) {
        _focused_stop = index;

        if (has_focus()) {
            update();
        }
    }
}


} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
