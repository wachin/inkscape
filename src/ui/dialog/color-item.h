// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Color item used in palettes and swatches UI.
 */
/* Authors: PBS <pbs3141@gmail.com>
 * Copyright (C) 2022 PBS
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_COLOR_ITEM_H
#define INKSCAPE_UI_DIALOG_COLOR_ITEM_H

#include <boost/variant.hpp> // TODO: Upgrade to boost::variant2 or std::variant when possible.
#include <boost/noncopyable.hpp>
#include <cairomm/cairomm.h>
#include <gtkmm/drawingarea.h>

#include "inkscape-preferences.h"
#include "widgets/paintdef.h"

class SPGradient;

namespace Inkscape {
namespace UI {
namespace Dialog {

class DialogBase;

/**
 * The color item you see on-screen as a clickable box.
 *
 * Note: This widget must be outlived by its parent dialog, passed in the constructor.
 */
class ColorItem final : public Gtk::DrawingArea, boost::noncopyable
{
public:
    /// Create a static color from a paintdef.
    ColorItem(PaintDef const&, DialogBase*);

    /**
     * Create a dynamically-updating color from a gradient, to which it remains linked.
     * If the gradient is destroyed, the widget will go into an inactive state.
     */
    ColorItem(SPGradient*, DialogBase*);

    /// Update the fill indicator, showing this widget is the fill of the current selection.
    void set_fill(bool);

    /// Update the stroke indicator, showing this widget is the stroke of the current selection.
    void set_stroke(bool);

    /// Update whether this item is pinned.
    bool is_pinned() const;
    void set_pinned_pref(const std::string &path);

    const Glib::ustring &get_description() const { return description; }

    sigc::signal<void ()>& signal_modified() { return _signal_modified; };
    sigc::signal<void ()>& signal_pinned() { return _signal_pinned; };

protected:
    bool on_draw(Cairo::RefPtr<Cairo::Context> const&) override;
    void on_size_allocate(Gtk::Allocation&) override;
    bool on_enter_notify_event(GdkEventCrossing*) override;
    bool on_leave_notify_event(GdkEventCrossing*) override;
    bool on_button_press_event(GdkEventButton*) override;
    bool on_button_release_event(GdkEventButton*) override;
    void on_drag_data_get(Glib::RefPtr<Gdk::DragContext> const &context, Gtk::SelectionData &selection_data, guint info, guint time) override;
    void on_drag_begin(Glib::RefPtr<Gdk::DragContext> const&) override;

private:
    // Common post-construction setup.
    void common_setup();

    // Perform the on-click action of setting the fill or stroke.
    void on_click(bool stroke);

    // Perform the right-click action of showing the context menu.
    void on_rightclick(GdkEventButton *event);

    // Draw the color only (i.e. no indicators) to a Cairo context. Used for drawing both the widget and the drag/drop icon.
    void draw_color(Cairo::RefPtr<Cairo::Context> const &cr, int w, int h) const;

    // Construct an equivalent paintdef for use during drag/drop.
    PaintDef to_paintdef() const;

    // Return the color (or average if a gradient), for choosing the color of the fill/stroke indicators.
    std::array<double, 3> average_color() const;

    // Description of the color, shown in help text.
    Glib::ustring description;
    Glib::ustring color_id;

    /// The pinned preference path
    Glib::ustring pinned_pref;
    bool pinned_default = false;

    // The color.
    struct NoneData {};
    struct RGBData { std::array<unsigned, 3> rgb; };
    struct GradientData { SPGradient *gradient; };
    boost::variant<NoneData, RGBData, GradientData> data;

    // The dialog this widget belongs to. Used for determining what desktop to take action on.
    DialogBase *dialog;

    // Whether this color is in use as the fill or stroke of the current selection.
    bool is_fill = false;
    bool is_stroke = false;

    // A cache of the widget contents, if necessary.
    Cairo::RefPtr<Cairo::ImageSurface> cache;
    bool cache_dirty = true;
    bool was_grad_pinned = false;

    // For ensuring that clicks that release outside the widget don't count.
    bool mouse_inside = false;

    sigc::signal<void ()> _signal_modified;
    sigc::signal<void ()> _signal_pinned;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_COLOR_ITEM_H
