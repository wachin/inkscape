// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Tavmjong Bah
 *   PBS <pbs3141@gmail.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <iostream> // Logging
#include <algorithm> // Sort
#include <set> // Coarsener
#include <thread>
#include <mutex>
#include <array>
#include <cassert>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <2geom/convex-hull.h>

#include "canvas.h"
#include "canvas-grid.h"

#include "color.h"          // Background color
#include "cms-system.h"     // Color correction
#include "desktop.h"
#include "document.h"
#include "preferences.h"
#include "ui/util.h"
#include "helper/geom.h"

#include "canvas/prefs.h"
#include "canvas/fragment.h"
#include "canvas/util.h"
#include "canvas/stores.h"
#include "canvas/graphics.h"
#include "canvas/synchronizer.h"
#include "display/drawing.h"
#include "display/control/canvas-item-drawing.h"
#include "display/control/canvas-item-group.h"
#include "display/control/snap-indicator.h"

#include "ui/tools/tool-base.h"      // Default cursor

#include "canvas/updaters.h"         // Update strategies
#include "canvas/framecheck.h"       // For frame profiling
#define framecheck_whole_function(D) \
    auto framecheckobj = D->prefs.debug_framecheck ? FrameCheck::Event(__func__) : FrameCheck::Event();

/*
 *   The canvas is responsible for rendering the SVG drawing with various "control"
 *   items below and on top of the drawing. Rendering is triggered by a call to one of:
 *
 *
 *   * redraw_all()     Redraws the entire canvas by calling redraw_area() with the canvas area.
 *
 *   * redraw_area()    Redraws the indicated area. Use when there is a change that doesn't affect
 *                      a CanvasItem's geometry or size.
 *
 *   * request_update() Redraws after recalculating bounds for changed CanvasItems. Use if a
 *                      CanvasItem's geometry or size has changed.
 *
 *   The first three functions add a request to the Gtk's "idle" list via
 *
 *   * add_idle()       Which causes Gtk to call when resources are available:
 *
 *   * on_idle()        Which sets up the backing stores, divides the area of the canvas that has been marked
 *                      unclean into rectangles that are small enough to render quickly, and renders them outwards
 *                      from the mouse with a call to:
 *
 *   * paint_rect_internal() Which paints the rectangle using paint_single_buffer(). It renders onto a Cairo
 *                           surface "backing_store". After a piece is rendered there is a call to:
 *
 *   * queue_draw_area() A Gtk function for marking areas of the window as needing a repaint, which when
 *                       the time is right calls:
 *
 *   * on_draw()        Which blits the Cairo surface to the screen.
 *
 *   The other responsibility of the canvas is to determine where to send GUI events. It does this
 *   by determining which CanvasItem is "picked" and then forwards the events to that item. Not all
 *   items can be picked. As a last resort, the "CatchAll" CanvasItem will be picked as it is the
 *   lowest CanvasItem in the stack (except for the "root" CanvasItem). With a small be of work, it
 *   should be possible to make the "root" CanvasItem a "CatchAll" eliminating the need for a
 *   dedicated "CatchAll" CanvasItem. There probably could be efficiency improvements as some
 *   items that are not pickable probably should be which would save having to effectively pick
 *   them "externally" (e.g. gradient CanvasItemCurves).
 */

namespace Inkscape::UI::Widget {
namespace {

/*
 * Utilities
 */

// GdkEvents can only be safely copied using gdk_event_copy. Since this function allocates, we need the following smart pointer to wrap the result.
struct GdkEventFreer {void operator()(GdkEvent *ev) const {gdk_event_free(ev);}};
using GdkEventUniqPtr = std::unique_ptr<GdkEvent, GdkEventFreer>;

// Copies a GdkEvent, returning the result as a smart pointer.
auto make_unique_copy(GdkEvent const *ev) { return GdkEventUniqPtr(gdk_event_copy(ev)); }

// Convert an integer received from preferences into an Updater enum.
auto pref_to_updater(int index)
{
    constexpr auto arr = std::array{Updater::Strategy::Responsive,
                                    Updater::Strategy::FullRedraw,
                                    Updater::Strategy::Multiscale};
    assert(1 <= index && index <= arr.size());
    return arr[index - 1];
}

// Represents the raster data and location of an in-flight tile (one that is drawn, but not yet pasted into the stores).
struct Tile
{
    Fragment fragment;
    Cairo::RefPtr<Cairo::ImageSurface> surface;
    Cairo::RefPtr<Cairo::ImageSurface> outline_surface;
};

// The urgency with which the async redraw process should exit.
enum class AbortFlags : int
{
    None = 0,
    Soft = 1, // exit if reached prerender phase
    Hard = 2  // exit in any phase
};

// A copy of all the data the async redraw process needs access to, along with its internal state.
struct RedrawData
{
    // Data on what/how to draw.
    Geom::IntPoint mouse_loc;
    Geom::IntRect visible;
    Fragment store;
    bool decoupled_mode;
    Cairo::RefPtr<Cairo::Region> snapshot_drawn;
    Geom::OptIntRect grabbed;

    // Saved prefs
    int coarsener_min_size;
    int coarsener_glue_size;
    double coarsener_min_fullness;
    int tile_size;
    int preempt;
    int margin;
    std::optional<int> redraw_delay;
    int render_time_limit;
    int numthreads;
    bool background_in_stores_required;
    uint64_t page, desk;
    bool debug_framecheck;
    bool debug_show_redraw;

    // State
    std::mutex mutex;
    gint64 start_time;
    int numactive;
    int phase;
    Geom::OptIntRect vis_store;

    Geom::IntRect bounds;
    Cairo::RefPtr<Cairo::Region> clean;
    bool interruptible;
    bool preemptible;
    std::vector<Geom::IntRect> rects;
    int effective_tile_size;

    // Results
    std::mutex tiles_mutex;
    std::vector<Tile> tiles;
    bool timeoutflag;

    // Return comparison object for sorting rectangles by distance from mouse point.
    auto getcmp() const
    {
        return [mouse_loc = mouse_loc] (Geom::IntRect const &a, Geom::IntRect const &b) {
            return a.distanceSq(mouse_loc) > b.distanceSq(mouse_loc);
        };
    }
};

} // namespace

/*
 * Implementation class
 */

class CanvasPrivate
{
public:
    friend class Canvas;
    Canvas *q;
    CanvasPrivate(Canvas *q)
        : q(q)
        , stores(prefs) {}

    // Lifecycle
    bool active = false;
    void activate();
    void deactivate();

    // CanvasItem tree
    std::optional<CanvasItemContext> canvasitem_ctx;

    // Preferences
    Prefs prefs;

    // Stores
    Stores stores;
    void handle_stores_action(Stores::Action action);

    // Invalidation
    std::unique_ptr<Updater> updater; // Tracks the unclean region and decides how to redraw it.
    Cairo::RefPtr<Cairo::Region> invalidated; // Buffers invalidations while the updater is in use by the background process.

    // Graphics state; holds all the graphics resources, including the drawn content.
    std::unique_ptr<Graphics> graphics;
    void activate_graphics();
    void deactivate_graphics();

    // Redraw process management.
    bool redraw_active = false;
    bool redraw_requested = false;
    sigc::connection schedule_redraw_conn;
    void schedule_redraw(int priority = Glib::PRIORITY_DEFAULT);
    void launch_redraw();
    void after_redraw();
    void commit_tiles();

    // Event handling.
    bool process_event(const GdkEvent*);
    bool pick_current_item(const GdkEvent*);
    bool emit_event(const GdkEvent*);
    CanvasItem *pre_scroll_grabbed_item;

    // Various state affecting what is drawn.
    uint32_t desk   = 0xffffffff; // The background colour, with the alpha channel used to control checkerboard.
    uint32_t border = 0x000000ff; // The border colour, used only to control shadow colour.
    uint32_t page   = 0xffffffff; // The page colour, also with alpha channel used to control checkerboard.

    bool clip_to_page = false; // Whether to enable clip-to-page mode.
    PageInfo pi; // The list of page rectangles.
    std::optional<Geom::PathVector> calc_page_clip() const; // Union of the page rectangles if in clip-to-page mode, otherwise no clip.
    bool is_point_on_page(const Geom::Point &point) const;

    int scale_factor = 1; // The device scale the stores are drawn at.

    RenderMode render_mode = RenderMode::NORMAL;
    SplitMode  split_mode  = SplitMode::NORMAL;

    bool outlines_enabled = false; // Whether to enable the outline layer.
    bool outlines_required() const { return split_mode != SplitMode::NORMAL || render_mode == RenderMode::OUTLINE_OVERLAY; }

    bool background_in_stores_enabled = false; // Whether the page and desk should be drawn into the stores/tiles; if not then transparency is used instead.
    bool background_in_stores_required() const { return !q->get_opengl_enabled() && SP_RGBA32_A_U(page) == 255 && SP_RGBA32_A_U(desk) == 255; } // Enable solid colour optimisation if both page and desk are solid (as opposed to checkerboard).

    // Async redraw process.
    std::optional<boost::asio::thread_pool> pool;
    int numthreads;
    int get_numthreads() const;

    Synchronizer sync;
    RedrawData rd;
    std::atomic<int> abort_flags;

    void init_tiler();
    bool init_redraw();
    bool end_redraw(); // returns true to indicate further redraw cycles required
    void process_redraw(Geom::IntRect const &bounds, Cairo::RefPtr<Cairo::Region> clean, bool interruptible = true, bool preemptible = true);
    void render_tile(int debug_id);
    void paint_rect(Geom::IntRect const &rect);
    void paint_single_buffer(const Cairo::RefPtr<Cairo::ImageSurface> &surface, const Geom::IntRect &rect, bool need_background, bool outline_pass);
    void paint_error_buffer(const Cairo::RefPtr<Cairo::ImageSurface> &surface);

    // Trivial overload of GtkWidget function.
    void queue_draw_area(Geom::IntRect const &rect);

    // For tracking the last known mouse position. (The function Gdk::Window::get_device_position cannot be used because of slow X11 round-trips. Remove this workaround when X11 dies.)
    std::optional<Geom::IntPoint> last_mouse;

    // Auto-scrolling.
    std::optional<guint> tick_callback;
    std::optional<gint64> last_time;
    Geom::IntPoint strain;
    Geom::Point displacement, velocity;
    void autoscroll_begin(Geom::IntPoint const &to);
    void autoscroll_end();
};

/*
 * Lifecycle
 */

Canvas::Canvas()
    : d(std::make_unique<CanvasPrivate>(this))
{
    set_name("InkscapeCanvas");

    // Events
    add_events(Gdk::BUTTON_PRESS_MASK   |
               Gdk::BUTTON_RELEASE_MASK |
               Gdk::ENTER_NOTIFY_MASK   |
               Gdk::LEAVE_NOTIFY_MASK   |
               Gdk::FOCUS_CHANGE_MASK   |
               Gdk::KEY_PRESS_MASK      |
               Gdk::KEY_RELEASE_MASK    |
               Gdk::POINTER_MOTION_MASK |
               Gdk::SCROLL_MASK         |
               Gdk::SMOOTH_SCROLL_MASK  );

    // Updater
    d->updater = Updater::create(pref_to_updater(d->prefs.update_strategy));
    d->updater->reset();
    d->invalidated = Cairo::Region::create();

    // Preferences
    d->prefs.grabsize.action = [=] { d->canvasitem_ctx->root()->update_canvas_item_ctrl_sizes(d->prefs.grabsize); };
    d->prefs.debug_show_unclean.action = [=] { queue_draw(); };
    d->prefs.debug_show_clean.action = [=] { queue_draw(); };
    d->prefs.debug_disable_redraw.action = [=] { d->schedule_redraw(); };
    d->prefs.debug_sticky_decoupled.action = [=] { d->schedule_redraw(); };
    d->prefs.debug_animate.action = [=] { queue_draw(); };
    d->prefs.outline_overlay_opacity.action = [=] { queue_draw(); };
    d->prefs.softproof.action = [=] { redraw_all(); };
    d->prefs.displayprofile.action = [=] { redraw_all(); };
    d->prefs.request_opengl.action = [=] {
        if (get_realized()) {
            d->deactivate();
            d->deactivate_graphics();
            set_opengl_enabled(d->prefs.request_opengl);
            d->updater->reset();
            d->activate_graphics();
            d->activate();
        }
    };
    d->prefs.pixelstreamer_method.action = [=] {
        if (get_realized() && get_opengl_enabled()) {
            d->deactivate();
            d->deactivate_graphics();
            d->activate_graphics();
            d->activate();
        }
    };
    d->prefs.numthreads.action = [=] {
        if (!d->active) return;
        int const new_numthreads = d->get_numthreads();
        if (d->numthreads == new_numthreads) return;
        d->numthreads = new_numthreads;
        d->deactivate();
        d->deactivate_graphics();
        d->pool.emplace(d->numthreads);
        d->activate_graphics();
        d->activate();
    };

    // Canvas item tree
    d->canvasitem_ctx.emplace(this);

    // Split view.
    _split_direction = SplitDirection::EAST;
    _split_frac = {0.5, 0.5};

    // Recreate stores on HiDPI change.
    property_scale_factor().signal_changed().connect([this] { d->schedule_redraw(); });

    // OpenGL switch.
    set_opengl_enabled(d->prefs.request_opengl);

    // Async redraw process.
    d->numthreads = d->get_numthreads();
    d->pool.emplace(d->numthreads);

    d->sync.connectExit([this] { d->after_redraw(); });
}

int CanvasPrivate::get_numthreads() const
{
    if (int n = prefs.numthreads; n > 0) {
        // First choice is the value set in preferences.
        return n;
    } else if (int n = std::thread::hardware_concurrency(); n > 0) {
        // If not set, use the number of processors minus one. (Using all of them causes stuttering.)
        return n == 1 ? 1 : n - 1;
    } else {
        // If not reported, use a sensible fallback.
        return 4;
    }
}

// Graphics becomes active when the widget is realized.
void CanvasPrivate::activate_graphics()
{
    if (q->get_opengl_enabled()) {
        q->make_current();
        graphics = Graphics::create_gl(prefs, stores, pi);
    } else {
        graphics = Graphics::create_cairo(prefs, stores, pi);
    }
    stores.set_graphics(graphics.get());
    stores.reset();
}

// After graphics becomes active, the canvas becomes active when additionally a drawing is set.
void CanvasPrivate::activate()
{
    // Event handling/item picking
    q->_pick_event.type = GDK_LEAVE_NOTIFY;
    q->_pick_event.crossing.x = 0;
    q->_pick_event.crossing.y = 0;

    q->_in_repick         = false;
    q->_left_grabbed_item = false;
    q->_all_enter_events  = false;
    q->_is_dragging       = false;
    q->_state             = 0;

    q->_current_canvas_item     = nullptr;
    q->_current_canvas_item_new = nullptr;
    q->_grabbed_canvas_item     = nullptr;
    q->_grabbed_event_mask = (Gdk::EventMask)0;
    pre_scroll_grabbed_item = nullptr;

    // Drawing
    q->_need_update = true;

    // Split view
    q->_split_dragging = false;

    // Todo: Disable GTK event compression again when doing so is no longer buggy.
    // Note: ToolBase::set_high_motion_precision() will keep turning it back on.
    // q->get_window()->set_event_compression(false);

    active = true;

    // Run the first redraw at high priority so it happens before the first call to paint_widget().
    schedule_redraw(Glib::PRIORITY_HIGH);
}

void CanvasPrivate::deactivate()
{
    active = false;

    if (redraw_active) {
        if (schedule_redraw_conn.connected()) {
            // In first link in chain, from schedule_redraw() to launch_redraw(). Break the link and exit.
            schedule_redraw_conn.disconnect();
        } else {
            // Otherwise, the background process is running. Interrupt the signal chain at exit.
            abort_flags.store((int)AbortFlags::Hard, std::memory_order_relaxed);
            if (prefs.debug_logging) std::cout << "Hard exit request" << std::endl;
            sync.waitForExit();

            // Unsnapshot the CanvasItems and DrawingItems.
            canvasitem_ctx->unsnapshot();
            q->_drawing->unsnapshot();
        }

        redraw_active = false;
        redraw_requested = false;
        assert(!schedule_redraw_conn.connected());
    }
}

void CanvasPrivate::deactivate_graphics()
{
    if (q->get_opengl_enabled()) q->make_current();
    commit_tiles();
    stores.set_graphics(nullptr);
    graphics.reset();
}

Canvas::~Canvas()
{
    // Remove entire CanvasItem tree.
    d->canvasitem_ctx.reset();
}

void Canvas::set_drawing(Drawing *drawing)
{
    if (d->active && !drawing) d->deactivate();
    _drawing = drawing;
    if (_drawing) {
        _drawing->setRenderMode(_render_mode == RenderMode::OUTLINE_OVERLAY ? RenderMode::NORMAL : _render_mode);
        _drawing->setColorMode(_color_mode);
        _drawing->setOutlineOverlay(d->outlines_required());
    }
    if (!d->active && get_realized() && drawing) d->activate();
}

CanvasItemGroup *Canvas::get_canvas_item_root() const
{
    return d->canvasitem_ctx->root();
}

void Canvas::on_realize()
{
    parent_type::on_realize();
    d->activate_graphics();
    if (_drawing) d->activate();
}

void Canvas::on_unrealize()
{
    if (_drawing) d->deactivate();
    d->deactivate_graphics();
    parent_type::on_unrealize();
}

/*
 * Redraw process managment
 */

// Schedule another redraw iteration to take place, waiting for the current one to finish if necessary.
void CanvasPrivate::schedule_redraw(int priority)
{
    if (!active) {
        // We can safely discard calls until active, because we will run an iteration on activation later in initialisation.
        return;
    }

    // Ensure another iteration is performed if one is in progress.
    redraw_requested = true;

    if (redraw_active) {
        return;
    }

    redraw_active = true;

    // Call run_redraw() as soon as possible on the main loop. (Cannot run now since CanvasItem tree could be in an invalid intermediate state.)
    assert(!schedule_redraw_conn.connected());
    schedule_redraw_conn = Glib::signal_idle().connect([this] {
        if (q->get_opengl_enabled()) {
            q->make_current();
        }
        if (prefs.debug_logging) std::cout << "Redraw start" << std::endl;
        launch_redraw();
        return false;
    }, priority); // Usually default priority; any higher results in competition with other idle callbacks => flickering snap indicators.
}

// Update state and launch redraw process in background. Requires a current OpenGL context.
void CanvasPrivate::launch_redraw()
{
    assert(redraw_active);

    if (q->_render_mode != render_mode) {
        if ((render_mode == RenderMode::OUTLINE_OVERLAY) != (q->_render_mode == RenderMode::OUTLINE_OVERLAY) && !q->get_opengl_enabled()) {
            q->queue_draw(); // Clear the whitewash effect, an artifact of cairo mode.
        }
        render_mode = q->_render_mode;
        q->_drawing->setRenderMode(render_mode == RenderMode::OUTLINE_OVERLAY ? RenderMode::NORMAL : render_mode);
        q->_drawing->setOutlineOverlay(outlines_required());
    }

    if (q->_split_mode != split_mode) {
        q->queue_draw(); // Clear the splitter overlay.
        split_mode = q->_split_mode;
        q->_drawing->setOutlineOverlay(outlines_required());
    }

    // Determine whether the rendering parameters have changed, and trigger full store recreation if so.
    if ((outlines_required() && !outlines_enabled) || scale_factor != q->get_scale_factor()) {
        stores.reset();
    }

    outlines_enabled = outlines_required();
    scale_factor = q->get_scale_factor();

    graphics->set_outlines_enabled(outlines_enabled);
    graphics->set_scale_factor(scale_factor);

    /*
     * Update state.
     */

    // Page information.
    pi.pages.clear();
    canvasitem_ctx->root()->visit_page_rects([this] (auto &rect) {
        pi.pages.emplace_back(rect);
    });

    graphics->set_colours(page, desk, border);
    graphics->set_background_in_stores(background_in_stores_required());

    q->_drawing->setClip(calc_page_clip());

    // Stores.
    handle_stores_action(stores.update(Fragment{ q->_affine, q->get_area_world() }));

    // Geometry.
    bool const affine_changed = canvasitem_ctx->affine() != stores.store().affine;
    if (q->_need_update || affine_changed) {
        FrameCheck::Event fc;
        if (prefs.debug_framecheck) fc = FrameCheck::Event("update");
        q->_need_update = false;
        canvasitem_ctx->setAffine(stores.store().affine);
        canvasitem_ctx->root()->update(affine_changed);
    }

    // Update strategy.
    auto const strategy = pref_to_updater(prefs.update_strategy);
    if (updater->get_strategy() != strategy) {
        auto new_updater = Updater::create(strategy);
        new_updater->clean_region = std::move(updater->clean_region);
        updater = std::move(new_updater);
    }

    updater->mark_dirty(invalidated);
    invalidated = Cairo::Region::create();

    updater->next_frame();

    /*
     * Launch redraw process in background.
     */

    // If asked to, don't paint anything and instead halt the redraw process.
    if (prefs.debug_disable_redraw) {
        redraw_active = false;
        return;
    }

    // Snapshot the CanvasItems and DrawingItems.
    canvasitem_ctx->snapshot();
    q->_drawing->snapshot();

    // Get the mouse position in screen space.
    rd.mouse_loc = last_mouse.value_or((Geom::Point(q->get_dimensions()) / 2).round());

    // Map the mouse to canvas space.
    rd.mouse_loc += q->_pos;
    if (stores.mode() == Stores::Mode::Decoupled) {
        rd.mouse_loc = (Geom::Point(rd.mouse_loc) * q->_affine.inverse() * stores.store().affine).round();
    }

    // Get the visible rect.
    rd.visible = q->get_area_world();
    if (stores.mode() == Stores::Mode::Decoupled) {
        rd.visible = (Geom::Parallelogram(rd.visible) * q->_affine.inverse() * stores.store().affine).bounds().roundOutwards();
    }

    // Get other misc data.
    rd.store = Fragment{ stores.store().affine, stores.store().rect };
    rd.decoupled_mode = stores.mode() == Stores::Mode::Decoupled;
    rd.coarsener_min_size = prefs.coarsener_min_size;
    rd.coarsener_glue_size = prefs.coarsener_glue_size;
    rd.coarsener_min_fullness = prefs.coarsener_min_fullness;
    rd.tile_size = prefs.tile_size;
    rd.preempt = prefs.preempt;
    rd.margin = prefs.prerender;
    rd.redraw_delay = prefs.debug_delay_redraw ? std::make_optional<int>(prefs.debug_delay_redraw_time) : std::nullopt;
    rd.render_time_limit = prefs.render_time_limit;
    rd.numthreads = get_numthreads();
    rd.background_in_stores_required = background_in_stores_required();
    rd.page = page;
    rd.desk = desk;
    rd.debug_framecheck = prefs.debug_framecheck;
    rd.debug_show_redraw = prefs.debug_show_redraw;

    rd.snapshot_drawn = stores.snapshot().drawn ? stores.snapshot().drawn->copy() : Cairo::RefPtr<Cairo::Region>();
    rd.grabbed = q->_grabbed_canvas_item && prefs.block_updates ? (roundedOutwards(q->_grabbed_canvas_item->get_bounds()) & rd.visible & rd.store.rect).regularized() : Geom::OptIntRect();

    abort_flags.store((int)AbortFlags::None, std::memory_order_relaxed);

    boost::asio::post(*pool, [this] { init_tiler(); });
}

void CanvasPrivate::after_redraw()
{
    assert(redraw_active);

    // Unsnapshot the CanvasItems and DrawingItems.
    canvasitem_ctx->unsnapshot();
    q->_drawing->unsnapshot();

    // OpenGL context needed for commit_tiles(), stores.finished_draw(), and launch_redraw().
    if (q->get_opengl_enabled()) {
        q->make_current();
    }

    // Commit tiles before stores.finished_draw() to avoid changing stores while tiles are still pending.
    commit_tiles();

    // Handle any pending stores action.
    bool stores_changed = false;
    if (!rd.timeoutflag) {
        auto const ret = stores.finished_draw(Fragment{ q->_affine, q->get_area_world() });
        handle_stores_action(ret);
        if (ret != Stores::Action::None) {
            stores_changed = true;
        }
    }

    // Relaunch or stop as necessary.
    if (rd.timeoutflag || redraw_requested || stores_changed) {
        if (prefs.debug_logging) std::cout << "Continuing redrawing" << std::endl;
        redraw_requested = false;
        launch_redraw();
    } else {
        if (prefs.debug_logging) std::cout << "Redraw exit" << std::endl;
        redraw_active = false;
    }
}

void CanvasPrivate::handle_stores_action(Stores::Action action)
{
    switch (action) {
        case Stores::Action::Recreated:
            // Set everything as needing redraw.
            invalidated->do_union(geom_to_cairo(stores.store().rect));
            updater->reset();

            if (prefs.debug_show_unclean) q->queue_draw();
            break;

        case Stores::Action::Shifted:
            invalidated->intersect(geom_to_cairo(stores.store().rect));
            updater->intersect(stores.store().rect);

            if (prefs.debug_show_unclean) q->queue_draw();
            break;

        default:
            break;
    }

    if (action != Stores::Action::None) {
        q->_drawing->setCacheLimit(stores.store().rect);
    }
}

// Commit all in-flight tiles to the stores. Requires a current OpenGL context (for graphics->draw_tile).
void CanvasPrivate::commit_tiles()
{
    framecheck_whole_function(this)

    decltype(rd.tiles) tiles;

    {
        auto lock = std::lock_guard(rd.tiles_mutex);
        tiles = std::move(rd.tiles);
    }

    for (auto &tile : tiles) {
        // Todo: Make CMS system thread-safe, then move this to render thread too.
        if (q->_cms_active) {
            auto transf = prefs.from_display
                        ? Inkscape::CMSSystem::getDisplayPer(q->_cms_key)
                        : Inkscape::CMSSystem::getDisplayTransform();
            if (transf) {
                tile.surface->flush();
                auto px = tile.surface->get_data();
                int stride = tile.surface->get_stride();
                for (int i = 0; i < tile.surface->get_height(); i++) {
                    auto row = px + i * stride;
                    Inkscape::CMSSystem::doTransform(transf, row, row, tile.surface->get_width());
                }
                tile.surface->mark_dirty();
            }
        }

        // Paste tile content onto stores.
        graphics->draw_tile(tile.fragment, std::move(tile.surface), std::move(tile.outline_surface));

        // Add to drawn region.
        assert(stores.store().rect.contains(tile.fragment.rect));
        stores.mark_drawn(tile.fragment.rect);

        // Get the rectangle of screen-space needing repaint.
        Geom::IntRect repaint_rect;
        if (stores.mode() == Stores::Mode::Normal) {
            // Simply translate to get back to screen space.
            repaint_rect = tile.fragment.rect - q->_pos;
        } else {
            // Transform into screen space, take bounding box, and round outwards.
            auto pl = Geom::Parallelogram(tile.fragment.rect);
            pl *= stores.store().affine.inverse() * q->_affine;
            pl *= Geom::Translate(-q->_pos);
            repaint_rect = pl.bounds().roundOutwards();
        }

        // Check if repaint is necessary - some rectangles could be entirely off-screen.
        auto screen_rect = Geom::IntRect({0, 0}, q->get_dimensions());
        if ((repaint_rect & screen_rect).regularized()) {
            // Schedule repaint.
            queue_draw_area(repaint_rect);
        }
    }
}

/*
 * Auto-scrolling
 */

static Geom::Point cap_length(Geom::Point const &pt, double max)
{
    auto const r = pt.length();
    return r <= max ? pt : pt * (max / r);
}

static double profile(double r)
{
    constexpr double max_speed = 30.0;
    constexpr double max_distance = 25.0;
    return std::clamp(Geom::sqr(r / max_distance) * max_speed, 1.0, max_speed);
}

static Geom::Point apply_profile(Geom::Point const &pt)
{
    auto const r = pt.length();
    if (r <= Geom::EPSILON) return {};
    return pt * profile(r) / r;
}

void CanvasPrivate::autoscroll_begin(Geom::IntPoint const &to)
{
    if (!q->_desktop) {
        return;
    }

    auto const rect = expandedBy(Geom::IntRect({}, q->get_dimensions()), -(int)prefs.autoscrolldistance);
    strain = to - rect.clamp(to);

    if (strain == Geom::IntPoint(0, 0) || tick_callback) {
        return;
    }

    tick_callback = q->add_tick_callback([this] (Glib::RefPtr<Gdk::FrameClock> const &clock) {
        auto timings = clock->get_current_timings();
        auto const t = timings->get_frame_time();
        double dt;
        if (last_time) {
            dt = t - *last_time;
        } else {
            dt = timings->get_refresh_interval();
        }
        last_time = t;
        dt *= 60.0 / 1e6 * prefs.autoscrollspeed;

        bool const strain_zero = strain == Geom::IntPoint(0, 0);

        if (strain.x() * velocity.x() < 0) velocity.x() = 0;
        if (strain.y() * velocity.y() < 0) velocity.y() = 0;
        auto const tgtvel = apply_profile(strain);
        auto const max_accel = strain_zero ? 3 : 2;
        velocity += cap_length(tgtvel - velocity, max_accel * dt);
        displacement += velocity * dt;
        auto const dpos = displacement.round();
        q->_desktop->scroll_relative(-dpos);
        displacement -= dpos;

        if (last_mouse) {
            GdkEventMotion event;
            memset(&event, 0, sizeof(GdkEventMotion));
            event.type = GDK_MOTION_NOTIFY;
            event.x = last_mouse->x();
            event.y = last_mouse->y();
            event.state = q->_state;
            emit_event(reinterpret_cast<GdkEvent*>(&event));
        }

        if (strain_zero && velocity.length() <= 0.1) {
            tick_callback = {};
            last_time = {};
            displacement = velocity = {};
            return false;
        }

        q->queue_draw();

        return true;
    });
}

void CanvasPrivate::autoscroll_end()
{
    strain = {};
}

// Allow auto-scrolling to take place if the mouse reaches the edge.
// The effect wears off when the mouse is next released.
void Canvas::enable_autoscroll()
{
    if (d->last_mouse) {
        d->autoscroll_begin(*d->last_mouse);
    } else {
        d->autoscroll_end();
    }
}

/*
 * Event handling
 */

bool Canvas::on_scroll_event(GdkEventScroll *scroll_event)
{
    return d->process_event(reinterpret_cast<GdkEvent*>(scroll_event));
}

bool Canvas::on_button_press_event(GdkEventButton *button_event)
{
    return on_button_event(button_event);
}

bool Canvas::on_button_release_event(GdkEventButton *button_event)
{
    if (button_event->button == 1) {
        d->autoscroll_end();
    }

    return on_button_event(button_event);
}

// Unified handler for press and release events.
bool Canvas::on_button_event(GdkEventButton *button_event)
{
    // Sanity-check event type.
    switch (button_event->type) {
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
            break; // Good
        default:
            std::cerr << "Canvas::on_button_event: illegal event type!" << std::endl;
            return false;
    }

    // Drag the split view controller.
    if (_split_mode == SplitMode::SPLIT) {
        auto cursor_position = Geom::IntPoint(button_event->x, button_event->y);
        switch (button_event->type) {
            case GDK_BUTTON_PRESS:
                if (_hover_direction != SplitDirection::NONE) {
                    _split_dragging = true;
                    _split_drag_start = cursor_position;
                    return true;
                }
                break;
            case GDK_2BUTTON_PRESS:
                if (_hover_direction != Inkscape::SplitDirection::NONE) {
                    _split_direction = _hover_direction;
                    _split_dragging = false;
                    queue_draw();
                    return true;
                }
                break;
            case GDK_BUTTON_RELEASE:
                if (!_split_dragging) break;
                _split_dragging = false;

                // Check if we are near the edge. If so, revert to normal mode.
                if (cursor_position.x() < 5                                 ||
                    cursor_position.y() < 5                                 ||
                    cursor_position.x() > get_allocation().get_width()  - 5 ||
                    cursor_position.y() > get_allocation().get_height() - 5)
                {
                    // Reset everything.
                    set_cursor();
                    set_split_mode(SplitMode::NORMAL);

                    // Update action (turn into utility function?).
                    auto window = dynamic_cast<Gtk::ApplicationWindow*>(get_toplevel());
                    if (!window) {
                        std::cerr << "Canvas::on_motion_notify_event: window missing!" << std::endl;
                        return true;
                    }

                    auto action = window->lookup_action("canvas-split-mode");
                    if (!action) {
                        std::cerr << "Canvas::on_motion_notify_event: action 'canvas-split-mode' missing!" << std::endl;
                        return true;
                    }

                    auto saction = Glib::RefPtr<Gio::SimpleAction>::cast_dynamic(action);
                    if (!saction) {
                        std::cerr << "Canvas::on_motion_notify_event: action 'canvas-split-mode' not SimpleAction!" << std::endl;
                        return true;
                    }

                    saction->change_state(static_cast<int>(SplitMode::NORMAL));
                }

                break;

            default:
                break;
        }
    }

    return d->process_event(reinterpret_cast<GdkEvent*>(button_event));
}

bool Canvas::on_enter_notify_event(GdkEventCrossing *crossing_event)
{
    if (crossing_event->window != get_window()->gobj()) {
        return false;
    }
    return d->process_event(reinterpret_cast<GdkEvent*>(crossing_event));
}

bool Canvas::on_leave_notify_event(GdkEventCrossing *crossing_event)
{
    if (crossing_event->window != get_window()->gobj()) {
        return false;
    }
    d->last_mouse = {};
    return d->process_event(reinterpret_cast<GdkEvent*>(crossing_event));
}

bool Canvas::on_focus_in_event(GdkEventFocus *focus_event)
{
    grab_focus();
    return false;
}

bool Canvas::on_key_press_event(GdkEventKey *key_event)
{
    return d->process_event(reinterpret_cast<GdkEvent*>(key_event));
}

bool Canvas::on_key_release_event(GdkEventKey *key_event)
{
    return d->process_event(reinterpret_cast<GdkEvent*>(key_event));
}

bool Canvas::on_motion_notify_event(GdkEventMotion *motion_event)
{
    // Record the last mouse position.
    d->last_mouse = Geom::IntPoint(motion_event->x, motion_event->y);

    // Handle interactions with the split view controller.
    if (_split_mode == SplitMode::XRAY) {
        queue_draw();
    } else if (_split_mode == SplitMode::SPLIT) {
        auto cursor_position = Geom::IntPoint(motion_event->x, motion_event->y);

        // Move controller.
        if (_split_dragging) {
            auto delta = cursor_position - _split_drag_start;
            if (_hover_direction == SplitDirection::HORIZONTAL) {
                delta.x() = 0;
            } else if (_hover_direction == SplitDirection::VERTICAL) {
                delta.y() = 0;
            }
            _split_frac += Geom::Point(delta) / get_dimensions();
            _split_drag_start = cursor_position;
            queue_draw();
            return true;
        }

        auto split_position = (_split_frac * get_dimensions()).round();
        auto diff = cursor_position - split_position;
        auto hover_direction = SplitDirection::NONE;
        if (Geom::Point(diff).length() < 20.0) {
            // We're hovering over circle, figure out which direction we are in.
            if (diff.y() - diff.x() > 0) {
                if (diff.y() + diff.x() > 0) {
                    hover_direction = SplitDirection::SOUTH;
                } else {
                    hover_direction = SplitDirection::WEST;
                }
            } else {
                if (diff.y() + diff.x() > 0) {
                    hover_direction = SplitDirection::EAST;
                } else {
                    hover_direction = SplitDirection::NORTH;
                }
            }
        } else if (_split_direction == SplitDirection::NORTH ||
                   _split_direction == SplitDirection::SOUTH)
        {
            if (std::abs(diff.y()) < 3) {
                // We're hovering over the horizontal line.
                hover_direction = SplitDirection::HORIZONTAL;
            }
        } else {
            if (std::abs(diff.x()) < 3) {
                // We're hovering over the vertical line.
                hover_direction = SplitDirection::VERTICAL;
            }
        }

        if (_hover_direction != hover_direction) {
            _hover_direction = hover_direction;
            set_cursor();
            queue_draw();
        }

        if (_hover_direction != SplitDirection::NONE) {
            // We're hovering, don't pick or emit event.
            return true;
        }
    }

    // Avoid embarrassing neverending autoscroll in case the button-released handler somehow doesn't fire.
    if (!(motion_event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))) {
        d->autoscroll_end();
    }

    return d->process_event(reinterpret_cast<GdkEvent*>(motion_event));
}

// Unified handler for all events.
bool CanvasPrivate::process_event(const GdkEvent *event)
{
    framecheck_whole_function(this)

    if (!active) {
        std::cerr << "Canvas::process_event: Called while not active!" << std::endl;
        return false;
    }

    auto calc_button_mask = [&] () -> int {
        switch (event->button.button) {
            case 1:  return GDK_BUTTON1_MASK; break;
            case 2:  return GDK_BUTTON2_MASK; break;
            case 3:  return GDK_BUTTON3_MASK; break;
            case 4:  return GDK_BUTTON4_MASK; break;
            case 5:  return GDK_BUTTON5_MASK; break;
            default: return 0; // Buttons can range at least to 9 but mask defined only to 5.
        }
    };

    // Do event-specific processing.
    switch (event->type) {
        case GDK_SCROLL:
        {
            // Save the current event-receiving item just before scrolling starts. It will continue to receive scroll events until the mouse is moved.
            if (!pre_scroll_grabbed_item) {
                pre_scroll_grabbed_item = q->_current_canvas_item;
                if (q->_grabbed_canvas_item && !q->_current_canvas_item->is_descendant_of(q->_grabbed_canvas_item)) {
                    pre_scroll_grabbed_item = q->_grabbed_canvas_item;
                }
            }

            // Process the scroll event...
            bool retval = emit_event(event);

            // ...then repick.
            q->_state = event->scroll.state;
            pick_current_item(event);

            return retval;
        }

        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        {
            pre_scroll_grabbed_item = nullptr;

            // Pick the current item as if the button were not pressed...
            q->_state = event->button.state;
            pick_current_item(event);

            // ...then process the event.
            q->_state ^= calc_button_mask();
            return emit_event(event);
        }

        case GDK_BUTTON_RELEASE:
        {
            pre_scroll_grabbed_item = nullptr;

            // Process the event as if the button were pressed...
            q->_state = event->button.state;
            bool retval = emit_event(event);

            // ...then repick after the button has been released.
            auto event_copy = make_unique_copy(event);
            event_copy->button.state ^= calc_button_mask();
            q->_state = event_copy->button.state;
            pick_current_item(event_copy.get());

            return retval;
        }

        case GDK_ENTER_NOTIFY:
            pre_scroll_grabbed_item = nullptr;
            q->_state = event->crossing.state;
            return pick_current_item(event);

        case GDK_LEAVE_NOTIFY:
            pre_scroll_grabbed_item = nullptr;
            q->_state = event->crossing.state;
            // This is needed to remove alignment or distribution snap indicators.
            if (q->_desktop) {
                q->_desktop->snapindicator->remove_snaptarget();
            }
            return pick_current_item(event);

        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE:
            return emit_event(event);

        case GDK_MOTION_NOTIFY:
            pre_scroll_grabbed_item = nullptr;
            q->_state = event->motion.state;
            pick_current_item(event);
            return emit_event(event);

        default:
            return false;
    }
}

// This function is called by 'process_event' to manipulate the state variables relating
// to the current object under the mouse, for example, to generate enter and leave events.
//
// This routine reacts to events from the canvas. Its main purpose is to find the canvas item
// closest to the cursor where the event occurred and then send the event (sometimes modified) to
// that item. The event then bubbles up the canvas item tree until an object handles it. If the
// widget is redrawn, this routine may be called again for the same event.
//
// Canvas items register their interest by connecting to the "event" signal.
// Example in desktop.cpp:
//   canvas_catchall->connect_event(sigc::bind(sigc::ptr_fun(sp_desktop_root_handler), this));
bool CanvasPrivate::pick_current_item(const GdkEvent *event)
{
    // Ensure requested geometry updates are performed first.
    if (q->_need_update && !q->_drawing->snapshotted() && !canvasitem_ctx->snapshotted()) {
        FrameCheck::Event fc;
        if (prefs.debug_framecheck) fc = FrameCheck::Event("update", 1);
        q->_need_update = false;
        canvasitem_ctx->root()->update(false);
    }

    int button_down = 0;
    if (!q->_all_enter_events) {
        // Only set true in connector-tool.cpp.

        // If a button is down, we'll perform enter and leave events on the
        // current item, but not enter on any other item.  This is more or
        // less like X pointer grabbing for canvas items.
        button_down = q->_state & (GDK_BUTTON1_MASK |
                                   GDK_BUTTON2_MASK |
                                   GDK_BUTTON3_MASK |
                                   GDK_BUTTON4_MASK |
                                   GDK_BUTTON5_MASK);
        if (!button_down) q->_left_grabbed_item = false;
    }

    // Save the event in the canvas.  This is used to synthesize enter and
    // leave events in case the current item changes.  It is also used to
    // re-pick the current item if the current one gets deleted.  Also,
    // synthesize an enter event.
    if (event != &q->_pick_event) {
        if (event->type == GDK_MOTION_NOTIFY || event->type == GDK_SCROLL || event->type == GDK_BUTTON_RELEASE) {
            // Convert to GDK_ENTER_NOTIFY

            // These fields have the same offsets in all types of events.
            q->_pick_event.crossing.type       = GDK_ENTER_NOTIFY;
            q->_pick_event.crossing.window     = event->motion.window;
            q->_pick_event.crossing.send_event = event->motion.send_event;
            q->_pick_event.crossing.subwindow  = nullptr;
            q->_pick_event.crossing.x          = event->motion.x;
            q->_pick_event.crossing.y          = event->motion.y;
            q->_pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
            q->_pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
            q->_pick_event.crossing.focus      = false;

            // These fields don't have the same offsets in all types of events.
            switch (event->type)
            {
                case GDK_MOTION_NOTIFY:
                    q->_pick_event.crossing.state  = event->motion.state;
                    q->_pick_event.crossing.x_root = event->motion.x_root;
                    q->_pick_event.crossing.y_root = event->motion.y_root;
                    break;
                case GDK_SCROLL:
                    q->_pick_event.crossing.state  = event->scroll.state;
                    q->_pick_event.crossing.x_root = event->scroll.x_root;
                    q->_pick_event.crossing.y_root = event->scroll.y_root;
                    break;
                case GDK_BUTTON_RELEASE:
                    q->_pick_event.crossing.state  = event->button.state;
                    q->_pick_event.crossing.x_root = event->button.x_root;
                    q->_pick_event.crossing.y_root = event->button.y_root;
                    break;
                default:
                    assert(false);
            }

        } else {
            q->_pick_event = *event;
        }
    }

    if (q->_in_repick) {
        // Don't do anything else if this is a recursive call.
        return false;
    }

    // Find new item
    q->_current_canvas_item_new = nullptr;

    if (q->_pick_event.type != GDK_LEAVE_NOTIFY && canvasitem_ctx->root()->is_visible()) {
        // Leave notify means there is no current item.
        // Find closest item.
        double x = 0.0;
        double y = 0.0;

        if (q->_pick_event.type == GDK_ENTER_NOTIFY) {
            x = q->_pick_event.crossing.x;
            y = q->_pick_event.crossing.y;
        } else {
            x = q->_pick_event.motion.x;
            y = q->_pick_event.motion.y;
        }

        // Look at where the cursor is to see if one should pick with outline mode.
        bool outline = q->canvas_point_in_outline_zone({ x, y });

        // Convert to world coordinates.
        auto p = Geom::Point(x, y) + q->_pos;
        if (stores.mode() == Stores::Mode::Decoupled) {
            p *= q->_affine.inverse() * canvasitem_ctx->affine();
        }

        q->_drawing->getCanvasItemDrawing()->set_pick_outline(outline);
        q->_current_canvas_item_new = canvasitem_ctx->root()->pick_item(p);
        // if (q->_current_canvas_item_new) {
        //     std::cout << "  PICKING: FOUND ITEM: " << q->_current_canvas_item_new->get_name() << std::endl;
        // } else {
        //     std::cout << "  PICKING: DID NOT FIND ITEM" << std::endl;
        // }
    }

    if (q->_current_canvas_item_new == q->_current_canvas_item && !q->_left_grabbed_item) {
        // Current item did not change!
        return false;
    }

    // Synthesize events for old and new current items.
    bool retval = false;
    if (q->_current_canvas_item_new != q->_current_canvas_item &&
        q->_current_canvas_item != nullptr                     &&
        !q->_left_grabbed_item                                 ) {

        GdkEvent new_event;
        new_event = q->_pick_event;
        new_event.type = GDK_LEAVE_NOTIFY;
        new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
        new_event.crossing.subwindow = nullptr;
        q->_in_repick = true;
        retval = emit_event(&new_event);
        q->_in_repick = false;
    }

    if (q->_all_enter_events == false) {
        // new_current_item may have been set to nullptr during the call to emitEvent() above.
        if (q->_current_canvas_item_new != q->_current_canvas_item && button_down) {
            q->_left_grabbed_item = true;
            return retval;
        }
    }

    // Handle the rest of cases
    q->_left_grabbed_item = false;
    q->_current_canvas_item = q->_current_canvas_item_new;

    if (q->_current_canvas_item != nullptr) {
        GdkEvent new_event;
        new_event = q->_pick_event;
        new_event.type = GDK_ENTER_NOTIFY;
        new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
        new_event.crossing.subwindow = nullptr;
        retval = emit_event(&new_event);
    }

    return retval;
}

// Fires an event at the canvas, after a little pre-processing. Returns true if handled.
bool CanvasPrivate::emit_event(const GdkEvent *event)
{
    // Handle grabbed items.
    if (q->_grabbed_canvas_item) {
        auto mask = (Gdk::EventMask)0;

        switch (event->type) {
            case GDK_ENTER_NOTIFY:
                mask = Gdk::ENTER_NOTIFY_MASK;
                break;
            case GDK_LEAVE_NOTIFY:
                mask = Gdk::LEAVE_NOTIFY_MASK;
                break;
            case GDK_MOTION_NOTIFY:
                mask = Gdk::POINTER_MOTION_MASK;
                break;
            case GDK_BUTTON_PRESS:
            case GDK_2BUTTON_PRESS:
            case GDK_3BUTTON_PRESS:
                mask = Gdk::BUTTON_PRESS_MASK;
                break;
            case GDK_BUTTON_RELEASE:
                mask = Gdk::BUTTON_RELEASE_MASK;
                break;
            case GDK_KEY_PRESS:
                mask = Gdk::KEY_PRESS_MASK;
                break;
            case GDK_KEY_RELEASE:
                mask = Gdk::KEY_RELEASE_MASK;
                break;
            case GDK_SCROLL:
                mask = Gdk::SCROLL_MASK;
                mask |= Gdk::SMOOTH_SCROLL_MASK;
                break;
            default:
                break;
        }

        if (!(mask & q->_grabbed_event_mask)) {
            return false;
        }
    }

    // Convert to world coordinates. We have two different cases due to different event structures.
    auto conv = [&, this] (double &x, double &y) {
        auto p = Geom::Point(x, y) + q->_pos;
        if (stores.mode() == Stores::Mode::Decoupled) {
            p *= q->_affine.inverse() * canvasitem_ctx->affine();
        }
        x = p.x();
        y = p.y();
    };

    auto event_copy = make_unique_copy(event);

    switch (event->type) {
        case GDK_ENTER_NOTIFY:
        case GDK_LEAVE_NOTIFY:
            conv(event_copy->crossing.x, event_copy->crossing.y);
            break;
        case GDK_MOTION_NOTIFY:
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
        case GDK_BUTTON_RELEASE:
            conv(event_copy->motion.x, event_copy->motion.y);
            break;
        default:
            break;
    }

    // Block undo/redo while anything is dragged.
    if (event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
        q->_is_dragging = true;
    } else if (event->type == GDK_BUTTON_RELEASE) {
        q->_is_dragging = false;
    }

    if (q->_current_canvas_item) {
        // Choose where to send event.
        auto item = q->_current_canvas_item;

        if (q->_grabbed_canvas_item && !q->_current_canvas_item->is_descendant_of(q->_grabbed_canvas_item)) {
            item = q->_grabbed_canvas_item;
        }

        if (pre_scroll_grabbed_item && event->type == GDK_SCROLL) {
            item = pre_scroll_grabbed_item;
        }

        // Propagate the event up the canvas item hierarchy until handled.
        while (item) {
            if (item->handle_event(event_copy.get())) return true;
            item = item->get_parent();
        }
    }

    return false;
}

/*
 * Protected functions
 */

Geom::IntPoint Canvas::get_dimensions() const
{
    return dimensions(get_allocation());
}

/**
 * Is world point inside canvas area?
 */
bool Canvas::world_point_inside_canvas(Geom::Point const &world) const
{
    return get_area_world().contains(world.floor());
}

/**
 * Translate point in canvas to world coordinates.
 */
Geom::Point Canvas::canvas_to_world(Geom::Point const &point) const
{
    return point + _pos;
}

/**
 * Return the area shown in the canvas in world coordinates.
 */
Geom::IntRect Canvas::get_area_world() const
{
    return Geom::IntRect(_pos, _pos + get_dimensions());
}

/**
 * Return whether a point in screen space / canvas coordinates is inside the region
 * of the canvas where things respond to mouse clicks as if they are in outline mode.
 */
bool Canvas::canvas_point_in_outline_zone(Geom::Point const &p) const
{
    if (_render_mode == RenderMode::OUTLINE || _render_mode == RenderMode::OUTLINE_OVERLAY) {
        return true;
    } else if (_split_mode == SplitMode::SPLIT) {
        auto split_position = _split_frac * get_dimensions();
        switch (_split_direction) {
            case SplitDirection::NORTH: return p.y() > split_position.y();
            case SplitDirection::SOUTH: return p.y() < split_position.y();
            case SplitDirection::WEST:  return p.x() > split_position.x();
            case SplitDirection::EAST:  return p.x() < split_position.x();
            default: return false;
        }
    } else {
        return false;
    }
}

/**
 * Return the last known mouse position of center if off-canvas.
 */
std::optional<Geom::Point> Canvas::get_last_mouse() const
{
    return d->last_mouse;
}

const Geom::Affine &Canvas::get_geom_affine() const
{
    return d->canvasitem_ctx->affine();
}

void CanvasPrivate::queue_draw_area(const Geom::IntRect &rect)
{
    if (q->get_opengl_enabled()) {
        // Note: GTK glitches out when you use queue_draw_area in OpenGL mode.
        // It's also pointless, because it seems to just call queue_draw anyway.
        q->queue_draw();
    } else {
        q->queue_draw_area(rect.left(), rect.top(), rect.width(), rect.height());
    }
}

/**
 * Invalidate drawing and redraw during idle.
 */
void Canvas::redraw_all()
{
    if (!d->active) {
        // CanvasItems redraw their area when being deleted... which happens when the Canvas is destroyed.
        // We need to ignore their requests!
        return;
    }
    d->invalidated->do_union(geom_to_cairo(d->stores.store().rect));
    d->schedule_redraw();
    if (d->prefs.debug_show_unclean) queue_draw();
}

/**
 * Redraw the given area during idle.
 */
void Canvas::redraw_area(int x0, int y0, int x1, int y1)
{
    if (!d->active) {
        // CanvasItems redraw their area when being deleted... which happens when the Canvas is destroyed.
        // We need to ignore their requests!
        return;
    }

    // Clamp area to Cairo's technically supported max size (-2^30..+2^30-1).
    // This ensures that the rectangle dimensions don't overflow and wrap around.
    constexpr int min_coord = -(1 << 30);
    constexpr int max_coord = (1 << 30) - 1;

    x0 = std::clamp(x0, min_coord, max_coord);
    y0 = std::clamp(y0, min_coord, max_coord);
    x1 = std::clamp(x1, min_coord, max_coord);
    y1 = std::clamp(y1, min_coord, max_coord);

    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    if (d->redraw_active && d->invalidated->empty()) {
        d->abort_flags.store((int)AbortFlags::Soft, std::memory_order_relaxed); // responding to partial invalidations takes priority over prerendering
        if (d->prefs.debug_logging) std::cout << "Soft exit request" << std::endl;
    }

    auto const rect = Geom::IntRect(x0, y0, x1, y1);
    d->invalidated->do_union(geom_to_cairo(rect));
    d->schedule_redraw();
    if (d->prefs.debug_show_unclean) queue_draw();
}

void Canvas::redraw_area(Geom::Coord x0, Geom::Coord y0, Geom::Coord x1, Geom::Coord y1)
{
    // Handle overflow during conversion gracefully.
    // Round outward to make sure integral coordinates cover the entire area.
    constexpr Geom::Coord min_int = std::numeric_limits<int>::min();
    constexpr Geom::Coord max_int = std::numeric_limits<int>::max();

    redraw_area(
        (int)std::floor(std::clamp(x0, min_int, max_int)),
        (int)std::floor(std::clamp(y0, min_int, max_int)),
        (int)std::ceil (std::clamp(x1, min_int, max_int)),
        (int)std::ceil (std::clamp(y1, min_int, max_int))
    );
}

void Canvas::redraw_area(Geom::Rect const &area)
{
    redraw_area(area.left(), area.top(), area.right(), area.bottom());
}

/**
 * Redraw after changing canvas item geometry.
 */
void Canvas::request_update()
{
    // Flag geometry as needing update.
    _need_update = true;

    // Trigger the redraw process to perform the update.
    d->schedule_redraw();
}

/**
 * Scroll window so drawing point 'pos' is at upper left corner of canvas.
 */
void Canvas::set_pos(Geom::IntPoint const &pos)
{
    if (pos == _pos) {
        return;
    }

    _pos = pos;

    d->schedule_redraw();
    queue_draw();
}

/**
 * Set the affine for the canvas.
 */
void Canvas::set_affine(Geom::Affine const &affine)
{
    if (_affine == affine) {
        return;
    }

    _affine = affine;

    d->schedule_redraw();
    queue_draw();
}

/**
 * Set the desk colour. Transparency is interpreted as amount of checkerboard.
 */
void Canvas::set_desk(uint32_t rgba)
{
    if (d->desk == rgba) return;
    bool invalidated = d->background_in_stores_enabled;
    d->desk = rgba;
    invalidated |= d->background_in_stores_enabled = d->background_in_stores_required();
    if (get_realized() && invalidated) redraw_all();
    queue_draw();
}

/**
 * Set the page border colour. Although we don't draw the borders, this colour affects the shadows which we do draw (in OpenGL mode).
 */
void Canvas::set_border(uint32_t rgba)
{
    if (d->border == rgba) return;
    d->border = rgba;
    if (get_realized() && get_opengl_enabled()) queue_draw();
}

/**
 * Set the page colour. Like the desk colour, transparency is interpreted as checkerboard.
 */
void Canvas::set_page(uint32_t rgba)
{
    if (d->page == rgba) return;
    bool invalidated = d->background_in_stores_enabled;
    d->page = rgba;
    invalidated |= d->background_in_stores_enabled = d->background_in_stores_required();
    if (get_realized() && invalidated) redraw_all();
    queue_draw();
}

/**
 * Gets the average desk color when desk is a checkerboard
 */
uint32_t Canvas::get_effective_background(const Geom::Point &point) const
{
    auto color = d->is_point_on_page(point) ? d->rd.page : d->rd.desk;
    auto arr = checkerboard_darken(rgb_to_array(color), 1.0f - 0.5f * SP_RGBA32_A_U(color) / 255.0f);
    return SP_RGBA32_F_COMPOSE(arr[0], arr[1], arr[2], 1.0);
}

/**
 * Returns true if this canvas is painted using stores
 */
bool Canvas::background_in_stores() const
{
    return d->rd.background_in_stores_required;
}

void Canvas::set_render_mode(RenderMode mode)
{
    if (mode == _render_mode) return;
    _render_mode = mode;
    d->schedule_redraw();
    if (_desktop) {
        _desktop->setWindowTitle(); // Mode is listed in title.
    }
}

void Canvas::set_color_mode(ColorMode mode)
{
    _color_mode = mode;
    if (_drawing) {
        _drawing->setColorMode(_color_mode);
    }
    if (_desktop) {
        _desktop->setWindowTitle(); // Mode is listed in title.
    }
}

void Canvas::set_split_mode(SplitMode mode)
{
    if (mode == _split_mode) return;
    _split_mode = mode;
    d->schedule_redraw();
    if (_split_mode == SplitMode::SPLIT) {
        _hover_direction = SplitDirection::NONE;
        _split_frac = {0.5, 0.5};
    }
}

void Canvas::set_clip_to_page_mode(bool clip)
{
    if (clip != d->clip_to_page) {
        d->clip_to_page = clip;
        d->schedule_redraw();
    }
}

void Canvas::set_cms_key(std::string key)
{
    _cms_key = std::move(key);
    _cms_active = !_cms_key.empty();
    redraw_all();
}

/**
 * Clear current and grabbed items.
 */
void Canvas::canvas_item_destructed(CanvasItem *item)
{
    if (!d->active) {
        return;
    }

    if (item == _current_canvas_item) {
        _current_canvas_item = nullptr;
    }

    if (item == _current_canvas_item_new) {
        _current_canvas_item_new = nullptr;
    }

    if (item == _grabbed_canvas_item) {
        _grabbed_canvas_item = nullptr;
        auto const display = Gdk::Display::get_default();
        auto const seat    = display->get_default_seat();
        seat->ungrab();
    }

    if (item == d->pre_scroll_grabbed_item) {
        d->pre_scroll_grabbed_item = nullptr;
    }
}

std::optional<Geom::PathVector> CanvasPrivate::calc_page_clip() const
{
    if (!clip_to_page) {
        return {};
    }

    Geom::PathVector pv;
    for (auto &rect : pi.pages) {
        pv.push_back(Geom::Path(rect));
    }
    return pv;
}

bool CanvasPrivate::is_point_on_page(const Geom::Point &point) const
{
    for (auto &rect : pi.pages) {
        if (rect.contains(point)) {
            return true;
        }
    }
    return false;
}

// Change cursor
void Canvas::set_cursor()
{
    if (!_desktop) {
        return;
    }

    auto display = Gdk::Display::get_default();

    switch (_hover_direction) {
        case SplitDirection::NONE:
            _desktop->event_context->use_tool_cursor();
            break;

        case SplitDirection::NORTH:
        case SplitDirection::EAST:
        case SplitDirection::SOUTH:
        case SplitDirection::WEST:
        {
            auto cursor = Gdk::Cursor::create(display, "pointer");
            get_window()->set_cursor(cursor);
            break;
        }

        case SplitDirection::HORIZONTAL:
        {
            auto cursor = Gdk::Cursor::create(display, "ns-resize");
            get_window()->set_cursor(cursor);
            break;
        }

        case SplitDirection::VERTICAL:
        {
            auto cursor = Gdk::Cursor::create(display, "ew-resize");
            get_window()->set_cursor(cursor);
            break;
        }

        default:
            // Shouldn't reach.
            std::cerr << "Canvas::set_cursor: Unknown hover direction!" << std::endl;
    }
}

void Canvas::get_preferred_width_vfunc(int &minimum_width, int &natural_width) const
{
    minimum_width = natural_width = 256;
}

void Canvas::get_preferred_height_vfunc(int &minimum_height, int &natural_height) const
{
    minimum_height = natural_height = 256;
}

void Canvas::on_size_allocate(Gtk::Allocation &allocation)
{
    auto const old_dimensions = get_dimensions();
    parent_type::on_size_allocate(allocation);
    auto const new_dimensions = get_dimensions();

    // Necessary as GTK seems to somehow invalidate the current pipeline state upon resize.
    if (d->active) {
        d->graphics->invalidated_glstate();
    }

    // Trigger the size update to be applied to the stores before the next redraw of the window.
    d->schedule_redraw();

    // Keep canvas centered and optionally zoomed in.
    if (_desktop && new_dimensions != old_dimensions) {
        auto const midpoint = _desktop->w2d(_pos + Geom::Point(old_dimensions) * 0.5);
        double zoom = _desktop->current_zoom();

        auto prefs = Preferences::get();
        if (prefs->getBool("/options/stickyzoom/value", false)) {
            // Calculate adjusted zoom.
            auto const old_minextent = min(old_dimensions);
            auto const new_minextent = min(new_dimensions);
            if (old_minextent != 0) {
                zoom *= (double)new_minextent / old_minextent;
            }
        }

        _desktop->zoom_absolute(midpoint, zoom, false);
    }
}

Glib::RefPtr<Gdk::GLContext> Canvas::create_context()
{
    Glib::RefPtr<Gdk::GLContext> result;

    try {
        result = get_window()->create_gl_context();
    } catch (const Gdk::GLError &e) {
        std::cerr << "Failed to create OpenGL context: " << e.what().raw() << std::endl;
        return {};
    }

    try {
        result->realize();
    } catch (const Glib::Error &e) {
        std::cerr << "Failed to realize OpenGL context: " << e.what().raw() << std::endl;
        return {};
    }

    return result;
}

void Canvas::paint_widget(Cairo::RefPtr<Cairo::Context> const &cr)
{
    framecheck_whole_function(d)

    if (!d->active) {
        std::cerr << "Canvas::paint_widget: Called while not active!" << std::endl;
        return;
    }

    if constexpr (false) d->canvasitem_ctx->root()->canvas_item_print_tree();

    // On activation, launch_redraw() is scheduled at a priority much higher than draw, so it
    // should have been called at least one before this point to perform vital initialisation
    // (needed not to crash). However, we don't want to rely on that, hence the following check.
    if (d->stores.mode() == Stores::Mode::None) {
        std::cerr << "Canvas::paint_widget: Called while active but uninitialised!" << std::endl;
        return;
    }

    // Commit pending tiles in case GTK called on_draw even though after_redraw() is scheduled at higher priority.
    if (!d->redraw_active) {
        d->commit_tiles();
    }

    if (get_opengl_enabled()) {
        bind_framebuffer();
    }

    Graphics::PaintArgs args;
    args.mouse = d->last_mouse;
    args.render_mode = d->render_mode;
    args.splitmode = d->split_mode;
    args.splitfrac = _split_frac;
    args.splitdir = _split_direction;
    args.hoverdir = _hover_direction;
    args.yaxisdir = _desktop ? _desktop->yaxisdir() : 1.0;

    d->graphics->paint_widget(Fragment{ _affine, get_area_world() }, args, cr);

    // If asked, run an animation loop.
    if (d->prefs.debug_animate) {
        auto t = g_get_monotonic_time() / 1700000.0;
        auto affine = Geom::Rotate(t * 5) * Geom::Scale(1.0 + 0.6 * cos(t * 2));
        set_affine(affine);
        auto dim = _desktop && _desktop->doc() ? _desktop->doc()->getDimensions() : Geom::Point();
        set_pos(Geom::Point((0.5 + 0.3 * cos(t * 2)) * dim.x(), (0.5 + 0.3 * sin(t * 3)) * dim.y()) * affine - Geom::Point(get_dimensions()) * 0.5);
    }
}

/*
 * Async redrawing process
 */

// Replace a region with a larger region consisting of fewer, larger rectangles. (Allowed to slightly overlap.)
auto coarsen(const Cairo::RefPtr<Cairo::Region> &region, int min_size, int glue_size, double min_fullness)
{
    // Sort the rects by minExtent.
    struct Compare
    {
        bool operator()(const Geom::IntRect &a, const Geom::IntRect &b) const {
            return a.minExtent() < b.minExtent();
        }
    };
    std::multiset<Geom::IntRect, Compare> rects;
    int nrects = region->get_num_rectangles();
    for (int i = 0; i < nrects; i++) {
        rects.emplace(cairo_to_geom(region->get_rectangle(i)));
    }

    // List of processed rectangles.
    std::vector<Geom::IntRect> processed;
    processed.reserve(nrects);

    // Removal lists.
    std::vector<decltype(rects)::iterator> remove_rects;
    std::vector<int> remove_processed;

    // Repeatedly expand small rectangles by absorbing their nearby small rectangles.
    while (!rects.empty() && rects.begin()->minExtent() < min_size) {
        // Extract the smallest unprocessed rectangle.
        auto rect = *rects.begin();
        rects.erase(rects.begin());

        // Initialise the effective glue size.
        int effective_glue_size = glue_size;

        while (true) {
            // Find the glue zone.
            auto glue_zone = rect;
            glue_zone.expandBy(effective_glue_size);

            // Absorb rectangles in the glue zone. We could do better algorithmically speaking, but in real life it's already plenty fast.
            auto newrect = rect;
            int absorbed_area = 0;

            remove_rects.clear();
            for (auto it = rects.begin(); it != rects.end(); ++it) {
                if (glue_zone.contains(*it)) {
                    newrect.unionWith(*it);
                    absorbed_area += it->area();
                    remove_rects.emplace_back(it);
                }
            }

            remove_processed.clear();
            for (int i = 0; i < processed.size(); i++) {
                auto &r = processed[i];
                if (glue_zone.contains(r)) {
                    newrect.unionWith(r);
                    absorbed_area += r.area();
                    remove_processed.emplace_back(i);
                }
            }

            // If the result was too empty, try again with a smaller glue size.
            double fullness = (double)(rect.area() + absorbed_area) / newrect.area();
            if (fullness < min_fullness) {
                effective_glue_size /= 2;
                continue;
            }

            // Commit the change.
            rect = newrect;

            for (auto &it : remove_rects) {
                rects.erase(it);
            }

            for (int j = (int)remove_processed.size() - 1; j >= 0; j--) {
                int i = remove_processed[j];
                processed[i] = processed.back();
                processed.pop_back();
            }

            // Stop growing if not changed or now big enough.
            bool finished = absorbed_area == 0 || rect.minExtent() >= min_size;
            if (finished) {
                break;
            }

            // Otherwise, continue normally.
            effective_glue_size = glue_size;
        }

        // Put the finished rectangle in processed.
        processed.emplace_back(rect);
    }

    // Put any remaining rectangles in processed.
    for (auto &rect : rects) {
        processed.emplace_back(rect);
    }

    return processed;
}

static std::optional<Geom::Dim2> bisect(Geom::IntRect const &rect, int tile_size)
{
    int bw = rect.width();
    int bh = rect.height();

    // Chop in half along the bigger dimension if the bigger dimension is too big.
    if (bw > bh) {
        if (bw > tile_size) {
            return Geom::X;
        }
    } else {
        if (bh > tile_size) {
            return Geom::Y;
        }
    }

    return {};
}

void CanvasPrivate::init_tiler()
{
    // Begin processing redraws.
    rd.start_time = g_get_monotonic_time();
    rd.phase = 0;
    rd.vis_store = (rd.visible & rd.store.rect).regularized();

    if (!init_redraw()) {
        sync.signalExit();
        return;
    }

    // Launch render threads to process tiles.
    rd.timeoutflag = false;

    rd.numactive = rd.numthreads;

    for (int i = 0; i < rd.numthreads - 1; i++) {
        boost::asio::post(*pool, [=] { render_tile(i); });
    }

    render_tile(rd.numthreads - 1);
}

bool CanvasPrivate::init_redraw()
{
    assert(rd.rects.empty());

    switch (rd.phase) {
        case 0:
            if (rd.vis_store && rd.decoupled_mode) {
                // The highest priority to redraw is the region that is visible but not covered by either clean or snapshot content, if in decoupled mode.
                // If this is not rendered immediately, it will be perceived as edge flicker, most noticeably on zooming out, but also on rotation too.
                process_redraw(*rd.vis_store, unioned(updater->clean_region->copy(), rd.snapshot_drawn));
                return true;
            } else {
                rd.phase++;
                // fallthrough
            }

        case 1:
            // Another high priority to redraw is the grabbed canvas item, if the user has requested block updates.
            if (rd.grabbed) {
                process_redraw(*rd.grabbed, updater->clean_region, false, false); // non-interruptible, non-preemptible
                return true;
            } else {
                rd.phase++;
                // fallthrough
            }

        case 2:
            if (rd.vis_store) {
                // The main priority to redraw, and the bread and butter of Inkscape's painting, is the visible content that is not clean.
                // This may be done over several cycles, at the direction of the Updater, each outwards from the mouse.
                process_redraw(*rd.vis_store, updater->get_next_clean_region());
                return true;
            } else {
                rd.phase++;
                // fallthrough
            }

        case 3: {
            // The lowest priority to redraw is the prerender margin around the visible rectangle.
            // (This is in addition to any opportunistic prerendering that may have already occurred in the above steps.)
            auto prerender = expandedBy(rd.visible, rd.margin);
            auto prerender_store = (prerender & rd.store.rect).regularized();
            if (prerender_store) {
                process_redraw(*prerender_store, updater->clean_region);
                return true;
            } else {
                return false;
            }
        }

        default:
            assert(false);
            return false;
    }
}

// Paint a given subrectangle of the store given by 'bounds', but avoid painting the part of it within 'clean' if possible.
// Some parts both outside the bounds and inside the clean region may also be painted if it helps reduce fragmentation.
void CanvasPrivate::process_redraw(Geom::IntRect const &bounds, Cairo::RefPtr<Cairo::Region> clean, bool interruptible, bool preemptible)
{
    rd.bounds = bounds;
    rd.clean = std::move(clean);
    rd.interruptible = interruptible;
    rd.preemptible = preemptible;

    // Assert that we do not render outside of store.
    assert(rd.store.rect.contains(rd.bounds));

    // Get the region we are asked to paint.
    auto region = Cairo::Region::create(geom_to_cairo(rd.bounds));
    region->subtract(rd.clean);

    // Get the list of rectangles to paint, coarsened to avoid fragmentation.
    rd.rects = coarsen(region,
                       std::min<int>(rd.coarsener_min_size, rd.tile_size / 2),
                       std::min<int>(rd.coarsener_glue_size, rd.tile_size / 2),
                       rd.coarsener_min_fullness);

    // Put the rectangles into a heap sorted by distance from mouse.
    std::make_heap(rd.rects.begin(), rd.rects.end(), rd.getcmp());

    // Adjust the effective tile size proportional to the painting area.
    double adjust = (double)cairo_to_geom(region->get_extents()).maxExtent() / rd.visible.maxExtent();
    adjust = std::clamp(adjust, 0.3, 1.0);
    rd.effective_tile_size = rd.tile_size * adjust;
}

// Process rectangles until none left or timed out.
void CanvasPrivate::render_tile(int debug_id)
{
    rd.mutex.lock();

    std::string fc_str;
    FrameCheck::Event fc;
    if (rd.debug_framecheck) {
        fc_str = "render_thread_" + std::to_string(debug_id + 1);
        fc = FrameCheck::Event(fc_str.c_str());
    }

    while (true) {
        // If we've run out of rects, try to start a new redraw cycle.
        if (rd.rects.empty()) {
            if (end_redraw()) {
                // More redraw cycles to do.
                continue;
            } else {
                // All finished.
                break;
            }
        }

        // Check for cancellation.
        auto const flags = abort_flags.load(std::memory_order_relaxed);
        bool const soft = flags & (int)AbortFlags::Soft;
        bool const hard = flags & (int)AbortFlags::Hard;
        if (hard || (rd.phase == 3 && soft)) {
            break;
        }

        // Extract the closest rectangle to the mouse.
        std::pop_heap(rd.rects.begin(), rd.rects.end(), rd.getcmp());
        auto rect = rd.rects.back();
        rd.rects.pop_back();

        // Cull empty rectangles.
        if (rect.hasZeroArea()) {
            continue;
        }

        // Cull rectangles that lie entirely inside the clean region.
        // (These can be generated by coarsening; they must be discarded to avoid getting stuck re-rendering the same rectangles.)
        if (rd.clean->contains_rectangle(geom_to_cairo(rect)) == Cairo::REGION_OVERLAP_IN) {
            continue;
        }

        // Lambda to add a rectangle to the heap.
        auto add_rect = [&] (Geom::IntRect const &rect) {
            rd.rects.emplace_back(rect);
            std::push_heap(rd.rects.begin(), rd.rects.end(), rd.getcmp());
        };

        // If the rectangle needs bisecting, bisect it and put it back on the heap.
        if (auto axis = bisect(rect, rd.effective_tile_size)) {
            int mid = rect[*axis].middle();
            auto lo = rect; lo[*axis].setMax(mid); add_rect(lo);
            auto hi = rect; hi[*axis].setMin(mid); add_rect(hi);
            continue;
        }

        // Extend thin rectangles at the edge of the bounds rect to at least some minimum size, being sure to keep them within the store.
        // (This ensures we don't end up rendering one thin rectangle at the edge every frame while the view is moved continuously.)
        if (rd.preemptible) {
            if (rect.width() < rd.preempt) {
                if (rect.left()  == rd.bounds.left() ) rect.setLeft (std::max(rect.right() - rd.preempt, rd.store.rect.left() ));
                if (rect.right() == rd.bounds.right()) rect.setRight(std::min(rect.left()  + rd.preempt, rd.store.rect.right()));
            }
            if (rect.height() < rd.preempt) {
                if (rect.top()    == rd.bounds.top()   ) rect.setTop   (std::max(rect.bottom() - rd.preempt, rd.store.rect.top()   ));
                if (rect.bottom() == rd.bounds.bottom()) rect.setBottom(std::min(rect.top()    + rd.preempt, rd.store.rect.bottom()));
            }
        }

        // Mark the rectangle as clean.
        updater->mark_clean(rect);

        rd.mutex.unlock();

        // Paint the rectangle.
        paint_rect(rect);

        rd.mutex.lock();

        // Check for timeout.
        if (rd.interruptible) {
            auto now = g_get_monotonic_time();
            auto elapsed = now - rd.start_time;
            if (elapsed > rd.render_time_limit * 1000) {
                // Timed out. Temporarily return to GTK main loop, and come back here when next idle.
                rd.timeoutflag = true;
                break;
            }
        }
    }

    if (rd.debug_framecheck && rd.timeoutflag) {
        fc.subtype = 1;
    }

    rd.numactive--;
    bool const done = rd.numactive == 0;

    rd.mutex.unlock();

    if (done) {
        rd.rects.clear();
        sync.signalExit();
    }
}

bool CanvasPrivate::end_redraw()
{
    switch (rd.phase) {
        case 0:
            rd.phase++;
            return init_redraw();

        case 1:
            rd.phase++;
            // Reset timeout to leave the normal amount of time for clearing up artifacts.
            rd.start_time = g_get_monotonic_time();
            return init_redraw();

        case 2:
            if (!updater->report_finished()) {
                rd.phase++;
            }
            return init_redraw();

        case 3:
            return false;

        default:
            assert(false);
            return false;
    }
}

void CanvasPrivate::paint_rect(Geom::IntRect const &rect)
{
    // Make sure the paint rectangle lies within the store.
    assert(rd.store.rect.contains(rect));

    auto paint = [&, this] (bool need_background, bool outline_pass) {

        auto surface = graphics->request_tile_surface(rect, true);
        if (!surface) {
            sync.runInMain([&] {
                if (prefs.debug_logging) std::cout << "Blocked - buffer mapping" << std::endl;
                if (q->get_opengl_enabled()) q->make_current();
                surface = graphics->request_tile_surface(rect, false);
            });
        }

        try {

            paint_single_buffer(surface, rect, need_background, outline_pass);

        } catch (std::bad_alloc const &) {
            // Note: std::bad_alloc actually indicates a Cairo error that occurs regularly at high zoom, and we must handle it.
            // See https://gitlab.com/inkscape/inkscape/-/issues/3975
            sync.runInMain([&] {
                std::cerr << "Rendering failure. You probably need to zoom out!" << std::endl;
                if (q->get_opengl_enabled()) q->make_current();
                graphics->junk_tile_surface(std::move(surface));
                surface = graphics->request_tile_surface(rect, false);
                paint_error_buffer(surface);
            });
        }

        return surface;
    };

    // Create and render the tile.
    Tile tile;
    tile.fragment.affine = rd.store.affine;
    tile.fragment.rect = rect;
    tile.surface = paint(background_in_stores_required(), false);
    if (outlines_enabled) {
        tile.outline_surface = paint(false, true);
    }

    // Introduce an artificial delay for each rectangle.
    if (rd.redraw_delay) g_usleep(*rd.redraw_delay);

    // Stick the tile on the list of tiles to reap.
    {
        auto g = std::lock_guard(rd.tiles_mutex);
        rd.tiles.emplace_back(std::move(tile));
    }
}

void CanvasPrivate::paint_single_buffer(Cairo::RefPtr<Cairo::ImageSurface> const &surface, Geom::IntRect const &rect, bool need_background, bool outline_pass)
{
    // Create Cairo context.
    auto cr = Cairo::Context::create(surface);

    // Clear background.
    cr->save();
    if (need_background) {
        Graphics::paint_background(Fragment{ rd.store.affine, rect }, pi, rd.page, rd.desk, cr);
    } else {
        cr->set_operator(Cairo::OPERATOR_CLEAR);
        cr->paint();
    }
    cr->restore();

    // Render drawing on top of background.
    auto buf = CanvasItemBuffer{ rect, scale_factor, cr, outline_pass };
    canvasitem_ctx->root()->render(buf);

    // Paint over newly drawn content with a translucent random colour.
    if (rd.debug_show_redraw) {
        cr->set_source_rgba((rand() % 256) / 255.0, (rand() % 256) / 255.0, (rand() % 256) / 255.0, 0.2);
        cr->set_operator(Cairo::OPERATOR_OVER);
        cr->paint();
    }
}

void CanvasPrivate::paint_error_buffer(Cairo::RefPtr<Cairo::ImageSurface> const &surface)
{
    // Paint something into surface to represent an "error" state for that tile.
    // Currently just paints solid black.
    auto cr = Cairo::Context::create(surface);
    cr->set_source_rgb(0, 0, 0);
    cr->paint();
}

} // namespace Inkscape::UI::Widget

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
