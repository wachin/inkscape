// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_CANVAS_H
#define SEEN_SP_CANVAS_H

/**
 * @file
 * SPCanvas, SPCanvasBuf.
 */
/*
 * Authors:
 *   Federico Mena <federico@nuclecu.unam.mx>
 *   Raph Levien <raph@gimp.org>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2016 Google Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <2geom/affine.h>
#include <2geom/rect.h>
#include <cstdint>
#include <glib.h>
#include <glibmm/ustring.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SP_TYPE_CANVAS (sp_canvas_get_type())
#define SP_CANVAS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), SP_TYPE_CANVAS, SPCanvas))
#define SP_IS_CANVAS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), SP_TYPE_CANVAS))

struct SPCanvas;
struct SPCanvasItem;
struct SPCanvasGroup;

enum {
    SP_CANVAS_UPDATE_REQUESTED  = 1 << 0,
    SP_CANVAS_UPDATE_AFFINE     = 1 << 1
};

/**
 * Structure used when rendering canvas items.
 */
struct SPCanvasBuf {
    cairo_t *ct;
    Geom::IntRect rect;
    Geom::IntRect canvas_rect; // visible window in world coordinates (i.e. offset by _x0, _y0)

    unsigned char *buf;
    int buf_rowstride;
    int device_scale; // For high DPI monitors.
    bool is_empty;
};

G_END_DECLS

// SPCanvas -------------------------------------------------

struct PaintRectSetup;

GType sp_canvas_get_type() G_GNUC_CONST;

/**
 * Port of GnomeCanvas for inkscape needs.
 */
struct SPCanvas {
    /// Scrolls canvas to specific position (c is measured in screen pixels).
    void scrollTo(Geom::Point const &c, unsigned int clear, bool is_scrolling = false);

    /// Synchronously updates the canvas if necessary.
    void updateNow();

    /// Queues a redraw of rectangular canvas area.
    void requestRedraw(int x1, int y1, int x2, int y2);
    void requestFullRedraw();
    void requestUpdate();

    void forceFullRedrawAfterInterruptions(unsigned int count, bool reset = true);
    void endForcedFullRedraws();

    Geom::Rect getViewbox() const;
    Geom::IntRect getViewboxIntegers() const;
    SPCanvasGroup *getRoot();

    void setBackgroundColor(guint32 rgba);
    void setBackgroundCheckerboard(guint32 rgba = 0xC4C4C4FF);

    /// Returns new canvas as widget.
    static GtkWidget *createAA();

private:
    /// Emits an event for an item in the canvas, be it the current
    /// item, grabbed item, or focused item, as appropriate.
    int emitEvent(GdkEvent *event);    

    /// Re-picks the current item in the canvas, based on the event's
    /// coordinates and emits enter/leave events for items as appropriate.
    int pickCurrentItem(GdkEvent *event);
    void shutdownTransients();

    /// Allocates a new tile array for the canvas, copying overlapping tiles from the old array
    void resizeTiles(int nl, int nt, int nr, int nb);

    /// Marks the specified area as dirty (requiring redraw)
    void dirtyRect(Geom::IntRect const &area);
    /// Marks the whole widget for redraw
    void dirtyAll();
    void markRect(Geom::IntRect const &area, uint8_t val);

    /// Invokes update, paint, and repick on canvas.
    int doUpdate();

    void paintSingleBuffer(Geom::IntRect const &paint_rect, Geom::IntRect const &canvas_rect, int sw);
    void paintXRayBuffer(Geom::IntRect const &paint_rect, Geom::IntRect const &canvas_rect);

    /**
     * Paint the given rect, recursively subdividing the region until it is the size of a single
     * buffer.
     *
     * @return true if the drawing completes
     */
    int paintRectInternal(PaintRectSetup const *setup, Geom::IntRect const &this_rect);

    void paintSpliter();
    void set_cursor(GtkWidget *widget);
    /// Draws a specific rectangular part of the canvas.
    /// @return true if the rectangle painting succeeds.
    bool paintRect(int xx0, int yy0, int xx1, int yy1);

    /// Repaints the areas in the canvas that need it.
    /// @return true if all the dirty parts have been redrawn
    int paint();

    /// Idle handler for the canvas that deals with pending updates and redraws.
    static gint idle_handler(gpointer data);

    /// Convenience function to add an idle handler to a canvas.
    void addIdle();
    void removeIdle();

public:
    // GTK virtual functions.
    static void dispose(GObject *object);
    static void handle_realize(GtkWidget *widget);
    static void handle_unrealize(GtkWidget *widget);
    static void handle_get_preferred_width(GtkWidget *widget, gint *min_w, gint *nat_w);
    static void handle_get_preferred_height(GtkWidget *widget, gint *min_h, gint *nat_h);
    static void handle_size_allocate(GtkWidget *widget, GtkAllocation *allocation);
    static gint handle_button(GtkWidget *widget, GdkEventButton *event);
    static gint handle_doubleclick(GtkWidget *widget, GdkEventButton *event);

    /**
     * Scroll event handler for the canvas.
     *
     * @todo FIXME: generate motion events to re-select items.
     */
    static gint handle_scroll(GtkWidget *widget, GdkEventScroll *event);
    static gint handle_motion(GtkWidget *widget, GdkEventMotion *event);
    static gboolean handle_draw(GtkWidget *widget, cairo_t *cr);
    static gint handle_key_event(GtkWidget *widget, GdkEventKey *event);
    static gint handle_crossing(GtkWidget *widget, GdkEventCrossing *event);
    static gint handle_focus_in(GtkWidget *widget, GdkEventFocus *event);
    static gint handle_focus_out(GtkWidget *widget, GdkEventFocus *event);

public:
    // Data members: ----------------------------------------------------------
    GtkWidget _widget;

    guint _idle_id;

    SPCanvasItem *_root;

    Geom::OptIntRect _spliter;
    Geom::OptIntRect _spliter_area;
    Geom::OptIntRect _spliter_control;
    Geom::OptIntRect _spliter_top;
    Geom::OptIntRect _spliter_bottom;
    Geom::OptIntRect _spliter_left;
    Geom::OptIntRect _spliter_right;
    Geom::OptIntRect _xray_rect;
    Geom::Point _spliter_control_pos;
    Geom::Point _spliter_in_control_pos;
    Geom::Point _xray_orig;
    double _split_value;
    bool _split_vertical;
    bool _split_inverse;
    bool _split_hover_vertical;
    bool _split_hover_horizontal;
    bool _split_hover;
    bool _split_pressed;
    bool _split_control_pressed;
    bool _split_dragging;
    double _xray_radius;
    bool _xray;
    bool _is_dragging;
    guint _changecursor;
    double _dx0;
    double _dy0;
    int _x0; ///< World coordinate of the leftmost pixels of window
    int _y0; ///< World coordinate of the topmost pixels of window
    int _device_scale; ///< Scale for high DPI montiors
    gint64 _idle_time;
    int _splits;
    gint64 _totalelapsed;

    /// Image surface storing the contents of the widget
    cairo_surface_t *_backing_store;
    /// This should be e.g. a cairo-xlib surface that is used to allocate _backing_store; might be NULL.
    cairo_surface_t *_surface_for_similar;
    /// Area of the widget that has up-to-date content
    cairo_region_t *_clean_region;
    /// Widget background, defaults to white
    cairo_pattern_t *_background;
    bool _background_is_checkerboard;

    /// Last known modifier state, for deferred repick when a button is down.
    int _state;

    /** The item containing the mouse pointer, or NULL if none. */
    SPCanvasItem *_current_item;

    /** Item that is about to become current (used to track deletions and such). */
    SPCanvasItem *_new_current_item;

    /** Item that holds a pointer grab, or NULL if none. */
    SPCanvasItem *_grabbed_item;

    /** Event mask specified when grabbing an item. */
    guint _grabbed_event_mask;

    /** If non-NULL, the currently focused item. */
    SPCanvasItem *_focused_item;

    /** Event on which selection of current item is based. */
    GdkEvent _pick_event;

    int _close_enough;

    unsigned int _need_update : 1;
    unsigned int _need_repick : 1;

    int _forced_redraw_count;
    int _forced_redraw_limit;

    /** For use by internal pick_current_item() function. */
    unsigned int _left_grabbed_item : 1;

    /** For use by internal pick_current_item() function. */
    unsigned int _in_repick : 1;

    // In most tools Inkscape only generates enter and leave events
    // on the current item, but no other enter events if a mouse button
    // is depressed -- see function pickCurrentItem().  Some tools
    // may wish the canvas to generate to all enter events, (e.g., the
    // connector tool).  If so, they may temporarily set this flag to
    // 'true'.
    bool _gen_all_enter_events;
    
    /** For scripting, sometimes we want to delay drawing. */
    bool _drawing_disabled;

    int _rendermode;
    int _colorrendermode;

#if defined(HAVE_LIBLCMS2)
    bool _enable_cms_display_adj;
    Glib::ustring _cms_key;
#endif // defined(HAVE_LIBLCMS2)

    bool _is_scrolling;
};

bool sp_canvas_world_pt_inside_window(SPCanvas const *canvas, Geom::Point const &world);

void sp_canvas_window_to_world(SPCanvas const *canvas, double winx, double winy, double *worldx, double *worldy);
void sp_canvas_world_to_window(SPCanvas const *canvas, double worldx, double worldy, double *winx, double *winy);

Geom::Point sp_canvas_window_to_world(SPCanvas const *canvas, Geom::Point const win);
Geom::Point sp_canvas_world_to_window(SPCanvas const *canvas, Geom::Point const world);

#endif // SEEN_SP_CANVAS_H

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
