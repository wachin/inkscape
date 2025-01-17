// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_DESKTOP_H
#define SEEN_SP_DESKTOP_H

/*
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   John Bintz <jcoswell@coswellproductions.org>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Jon A. Cruz <jon@joncruz.org>get
 *   Abhishek Sharma
 *
 * Copyright (C) 2007 Johan Engelen
 * Copyright (C) 2006 John Bintz
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */

#include <cstddef>
#include <sigc++/sigc++.h>
#include <glibmm/ustring.h>
#include <gtkmm.h> // Many downstream includes

#include <2geom/affine.h>
#include <2geom/transforms.h>
#include <2geom/parallelogram.h>

#include "preferences.h"

#include "display/rendermode.h"

#include "object/sp-gradient.h" // TODO refactor enums out to their own .h file

#include "ui/view/view.h"

namespace Gtk
{
    class Toolbar;
    class Window;
}

// ------- Inkscape --------
class SPCSSAttr;
class SPDesktopWidget;
class SPItem;
class SPNamedView;
class SPObject;
class SPStyle;
class SPStop;

class InkscapeWindow;

struct _GdkEventAny;
typedef struct _GdkEventAny GdkEventAny;
struct _GdkEventWindowState;
typedef struct _GdkEventWindowState GdkEventWindowState;

struct InkscapeApplication;

namespace Inkscape {
    class LayerManager;
    class PageManager;
    class MessageContext;
    class Selection;

    class CanvasItem;
    class CanvasItemCatchall;
    class CanvasItemDrawing;
    class CanvasItemGroup;
    class CanvasItemRotate;

    namespace UI {
        class ControlPointSelection;

        namespace Dialog {
            class DialogContainer;
        }

        namespace Tools {
            class ToolBase;
            class TextTool;
        }

        namespace Widget {
            class Canvas;
            class Dock;
        }
    }

    namespace Display {
        class TemporaryItemList;
        class TemporaryItem;
        class SnapIndicator;
    }
}

#define SP_DESKTOP_ZOOM_MAX 256.0
#define SP_DESKTOP_ZOOM_MIN 0.01

/**
 * SPDesktop is a subclass of View, implementing an editable document
 * canvas.  It is extensively used by many UI controls that need certain
 * visual representations of their own.
 *
 * SPDesktop provides a certain set of CanvasItems, serving as GUI
 * layers of different control objects. The one containing the whole
 * document is the drawing layer. In addition to it, there are grid,
 * guide, sketch and control layers. The sketch layer is used for
 * temporary drawing objects, before the real objects in document are
 * created. The control layer contains editing knots, rubberband and
 * similar non-document UI objects.
 *
 * Each SPDesktop is associated with a SPNamedView node of the document
 * tree.  Currently, all desktops are created from a single main named
 * view, but in the future there may be support for different ones.
 * SPNamedView serves as an in-document container for desktop-related
 * data, like grid and guideline placement, snapping options and so on.
 *
 * Associated with each SPDesktop are the two most important editing
 * related objects - SPSelection and ToolBase.
 *
 * Sodipodi keeps track of the active desktop and invokes notification
 * signals whenever it changes. UI elements can use these to update their
 * display to the selection of the currently active editing window.
 * (Lauris Kaplinski)
 *
 * @see \ref desktop-handles.h for desktop macros.
 */
class SPDesktop : public Inkscape::UI::View::View
{
public:
    SPNamedView               *namedview;
    Inkscape::UI::Tools::ToolBase *event_context = nullptr;
    Inkscape::Display::TemporaryItemList *temporary_item_list;
    Inkscape::Display::SnapIndicator *snapindicator;

    Inkscape::UI::Tools::ToolBase* getEventContext() const { return event_context; }
    Inkscape::Selection* getSelection() const { return _selection.get(); }
    SPDocument* getDocument() const { return doc(); }
    Inkscape::UI::Widget::Canvas* getCanvas() const { return canvas; }
    Inkscape::MessageStack* getMessageStack() const { return messageStack().get(); }
    SPNamedView* getNamedView() const { return namedview; }
    SPDesktopWidget *getDesktopWidget() const { return _widget; }

    // ------- Canvas Items -------
    Inkscape::UI::Widget::Canvas *canvas;

     // Move these into UI::Widget::Canvas:
    Inkscape::CanvasItemGroup    *getCanvasControls() const { return canvas_group_controls; }
    Inkscape::CanvasItemGroup    *getCanvasPagesBg()  const { return canvas_group_pages_bg; }
    Inkscape::CanvasItemGroup    *getCanvasPagesFg()  const { return canvas_group_pages_fg; }
    Inkscape::CanvasItemGroup    *getCanvasGrids()    const { return canvas_group_grids; }
    Inkscape::CanvasItemGroup    *getCanvasGuides()   const { return canvas_group_guides; }
    Inkscape::CanvasItemGroup    *getCanvasSketch()   const { return canvas_group_sketch; }
    Inkscape::CanvasItemGroup    *getCanvasTemp()     const { return canvas_group_temp; }

    Inkscape::CanvasItemCatchall *getCanvasCatchall() const { return canvas_catchall; }
    Inkscape::CanvasItemDrawing  *getCanvasDrawing()  const { return canvas_drawing; }

private:
    /// current selection; will never generally be NULL
    std::unique_ptr<Inkscape::Selection> _selection;

    // Groups
    Inkscape::CanvasItemGroup    *canvas_group_controls  = nullptr; ///< Handles, knots, nodes, etc.
    Inkscape::CanvasItemGroup    *canvas_group_drawing   = nullptr; ///< SVG Drawing
    Inkscape::CanvasItemGroup    *canvas_group_grids     = nullptr; ///< Grids.
    Inkscape::CanvasItemGroup    *canvas_group_guides    = nullptr; ///< Guide lines.
    Inkscape::CanvasItemGroup    *canvas_group_sketch    = nullptr; ///< Temporary items before becoming permanent.
    Inkscape::CanvasItemGroup    *canvas_group_temp      = nullptr; ///< Temporary items that self-destruct.
    Inkscape::CanvasItemGroup    *canvas_group_pages_bg  = nullptr; ///< Page background
    Inkscape::CanvasItemGroup    *canvas_group_pages_fg  = nullptr; ///< Page border + shadow.

    // Individual items
    Inkscape::CanvasItemCatchall *canvas_catchall        = nullptr; ///< The bottom item for unclaimed events.
    Inkscape::CanvasItemDrawing  *canvas_drawing         = nullptr; ///< The actual SVG drawing (a.k.a. arena).

public:
    SPCSSAttr     *current;     ///< current style
    bool           _focusMode;  ///< Whether we're focused working or general working

    unsigned int dkey;
    guint window_state;
    unsigned int interaction_disabled_counter;
    bool waiting_cursor;
    bool showing_dialogs;
    bool rotation_locked;
    /// \todo fixme: This has to be implemented in different way */
    guint guides_active : 1;

    // storage for selected dragger used by GrDrag as it's
    // created and deleted by tools
    SPItem *gr_item;
    GrPointType  gr_point_type;
    guint  gr_point_i;
    Inkscape::PaintTarget gr_fill_or_stroke;

    Glib::ustring _reconstruction_old_layer_id;

    sigc::signal<bool (const SPCSSAttr *, bool)>::accumulated<StopOnTrue> _set_style_signal;
    sigc::signal<int (SPStyle *, int)>::accumulated<StopOnNonZero> _query_style_signal;

    /// Emitted when the zoom factor changes (not emitted when scrolling).
    /// The parameter is the new zoom factor
    sigc::signal<void (double)> signal_zoom_changed;

    sigc::connection connectDestroy(const sigc::slot<void (SPDesktop*)> &slot)
    {
        return _destroy_signal.connect(slot);
    }

    sigc::connection connectDocumentReplaced (const sigc::slot<void (SPDesktop*,SPDocument*)> & slot)
    {
        return _document_replaced_signal.connect (slot);
    }

    sigc::connection connectEventContextChanged (const sigc::slot<void (SPDesktop*,Inkscape::UI::Tools::ToolBase*)> & slot)
    {
        return _event_context_changed_signal.connect (slot);
    }
    sigc::connection connectSetStyle (const sigc::slot<bool (const SPCSSAttr *)> & slot)
    {
        return _set_style_signal.connect([=](const SPCSSAttr* css, bool) { return slot(css); });
    }
    sigc::connection connectSetStyleEx(const sigc::slot<bool (const SPCSSAttr *, bool)> & slot)
    {
        return _set_style_signal.connect(slot);
    }
    sigc::connection connectQueryStyle (const sigc::slot<int (SPStyle *, int)> & slot)
    {
        return _query_style_signal.connect (slot);
    }
     // subselection is some sort of selection which is specific to the tool, such as a handle in gradient tool, or a text selection
    sigc::connection connectToolSubselectionChanged(const sigc::slot<void (gpointer)> & slot);
    sigc::connection connectToolSubselectionChangedEx(const sigc::slot<void (gpointer, SPObject*)>& slot);

    void emitToolSubselectionChanged(gpointer data);
    void emitToolSubselectionChangedEx(gpointer data, SPObject* object);

    // there's an object selected and it has a gradient fill and/or stroke; one of the gradient stops has been selected
    // callback receives sender pointer and selected stop pointer
    sigc::connection connect_gradient_stop_selected(const sigc::slot<void (void*, SPStop*)>& slot);
    // a path is being edited and one of its control points has been (de)selected using node tool
    // callback receives sender pointer and control spoints selection pointer
    sigc::connection connect_control_point_selected(const sigc::slot<void (void*, Inkscape::UI::ControlPointSelection*)>& slot);
    // there's an active text frame and user moves or clicks text cursor within it using text tool
    // callback receives sender pointer and text tool pointer
    sigc::connection connect_text_cursor_moved(const sigc::slot<void (void*, Inkscape::UI::Tools::TextTool*)>& slot);

    void emit_gradient_stop_selected(void* sender, SPStop* stop);
    void emit_control_point_selected(void* sender, Inkscape::UI::ControlPointSelection* selection);
    void emit_text_cursor_moved(void* sender, Inkscape::UI::Tools::TextTool* tool);

    /**
     * Return new desktop object.
     * \pre namedview != NULL.
     * \pre canvas != NULL.
     */
    SPDesktop();

    void init (SPNamedView* nv, Inkscape::UI::Widget::Canvas* new_canvas, SPDesktopWidget *widget);
    ~SPDesktop() override;
    void destroy();

    Inkscape::LayerManager& layerManager() { return *_layer_manager; }
    const Inkscape::LayerManager& layerManager() const { return *_layer_manager; }

    Inkscape::MessageContext *guidesMessageContext() const {
        return _guides_message_context.get();
    }

    Inkscape::Display::TemporaryItem * add_temporary_canvasitem (Inkscape::CanvasItem *item, guint lifetime, bool move_to_bottom = true);
    void remove_temporary_canvasitem (Inkscape::Display::TemporaryItem * tempitem);

    Inkscape::UI::Dialog::DialogContainer *getContainer();

    bool isWithinViewport(SPItem *item) const;
    bool itemIsHidden(SPItem const *item) const;

    void activate_guides (bool activate);
    void change_document (SPDocument *document);

    void setEventContext(const std::string& toolName);

    void set_coordinate_status (Geom::Point p);
    SPItem *getItemFromListAtPointBottom(const std::vector<SPItem*> &list, Geom::Point const &p) const;
    SPItem *getItemAtPoint(Geom::Point const &p, bool into_groups, SPItem *upto = nullptr) const;
    SPItem *getGroupAtPoint(Geom::Point const &p) const;
    Geom::Point point() const;

    void prev_transform();
    void next_transform();
    void clear_transform_history();

    void set_display_area (bool log = true);
    void set_display_area (Geom::Point const &c, Geom::Point const &w, bool log = true);
    void set_display_area (Geom::Rect const &a, Geom::Coord border, bool log = true);
    Geom::Parallelogram get_display_area() const;
    void set_display_width(Geom::Rect const &a, Geom::Coord border);
    void set_display_center(Geom::Rect const &a);

    void zoom_absolute (Geom::Point const &c, double const zoom, bool keep_point = true);
    void zoom_relative (Geom::Point const &c, double const zoom, bool keep_point = true);
    void zoom_realworld (Geom::Point const &c, double const ratio);

    void zoom_drawing();
    void zoom_selection();
    void schedule_zoom_from_document();

    double current_zoom() const { return _current_affine.getZoom(); }
    Geom::Point current_center() const;

    void zoom_quick(bool enable = true);
    /** \brief  Returns whether the desktop is in quick zoom mode or not */
    bool quick_zoomed() { return _quick_zoom_enabled; }

    void quick_preview(bool activate);

    void set_rotation_lock(bool lock) { rotation_locked = lock; }
    bool get_rotation_lock() const { return rotation_locked; }

    void zoom_grab_focus();

    void rotate_absolute_keep_point   (Geom::Point const &c, double const rotate);
    void rotate_relative_keep_point   (Geom::Point const &c, double const rotate);
    void rotate_absolute_center_point (Geom::Point const &c, double const rotate);
    void rotate_relative_center_point (Geom::Point const &c, double const rotate);

    enum CanvasFlip {
        FLIP_NONE       = 0,
        FLIP_HORIZONTAL = 1,
        FLIP_VERTICAL   = 2
    };
    void flip_absolute_keep_point   (Geom::Point const &c, CanvasFlip flip);
    void flip_relative_keep_point   (Geom::Point const &c, CanvasFlip flip);
    void flip_absolute_center_point (Geom::Point const &c, CanvasFlip flip);
    void flip_relative_center_point (Geom::Point const &c, CanvasFlip flip);
    bool is_flipped (CanvasFlip flip);

    double current_rotation() const { return _current_affine.getRotation(); }

    void scroll_absolute (Geom::Point const &point);
    void scroll_relative (Geom::Point const &delta);
    void scroll_relative_in_svg_coords (double dx, double dy);
    bool scroll_to_point (Geom::Point const &s_dt, gdouble autoscrollspeed = 0);

    void setWindowTitle();
    void getWindowGeometry (gint &x, gint &y, gint &w, gint &h);
    void setWindowPosition (Geom::Point p);
    void setWindowSize (gint w, gint h);
    void setWindowTransient (void* p, int transient_policy=1);
    Gtk::Window* getToplevel();  // To be removed in favor of getInkscapeWindow
    InkscapeWindow* getInkscapeWindow();
    void presentWindow();
    bool showInfoDialog( Glib::ustring const &message );
    bool warnDialog (Glib::ustring const &text);
    void toggleCommandPalette();
    void toggleRulers();
    void toggleScrollbars();
    void setTempHideOverlays(bool hide);
    void layoutWidget();
    void setToolboxFocusTo (gchar const* label);
    Gtk::Toolbar *get_toolbar_by_name(const Glib::ustring& name);
    Gtk::Widget *get_toolbox() const;
    void setToolboxAdjustmentValue (gchar const* id, double val);
    bool isToolboxButtonActive (gchar const *id);
    void updateDialogs();
    void showNotice(Glib::ustring const &msg, unsigned timeout = 0);

    void enableInteraction();
    void disableInteraction();

    void setWaitingCursor();
    void clearWaitingCursor();
    bool isWaitingCursor() const { return waiting_cursor; };

    void toggleLockGuides();

    void toggleColorProfAdjust();
    bool colorProfAdjustEnabled();

    void toggleToolbar(gchar const *toolbar_name);

    bool is_iconified();
    bool is_darktheme();
    bool is_maximized();
    bool is_fullscreen();
    bool is_focusMode();

    void iconify();
    void maximize();
    void fullscreen();
    void focusMode(bool mode = true);

    // TODO return const ref instead of copy
    Geom::Affine w2d() const; //transformation from window to desktop coordinates (zoom/rotate).
    Geom::Point w2d(Geom::Point const &p) const;
    /// Transformation from desktop to window coordinates
    Geom::Affine d2w() const { return _current_affine.d2w(); }
    Geom::Point d2w(Geom::Point const &p) const;
    const Geom::Affine& doc2dt() const;
    Geom::Affine dt2doc() const;
    Geom::Point doc2dt(Geom::Point const &p) const;
    Geom::Point dt2doc(Geom::Point const &p) const;

    bool is_yaxisdown() const { return doc2dt()[3] > 0; }
    double yaxisdir() const { return doc2dt()[3]; }

    void setDocument (SPDocument* doc) override;

    virtual bool onWindowStateEvent (GdkEventWindowState* event);

    void applyCurrentOrToolStyle(SPObject *obj, Glib::ustring const &tool_path, bool with_text);

private:
    GtkGesture *zoomgesture = nullptr;

    SPDesktopWidget       *_widget;
    std::unique_ptr<Inkscape::MessageContext> _guides_message_context;
    bool _active;

    // This simple class ensures that _w2d is always in sync with _rotation and _scale
    // We keep rotation and scale separate to avoid having to extract them from the affine.
    // With offset, this describes fully how to map the drawing to the window.
    // Future: merge offset as a translation in w2d.
    class DesktopAffine {
      public:
        Geom::Affine w2d() const { return _w2d; };
        Geom::Affine d2w() const { return _d2w; };

        void setScale( Geom::Scale scale ) {
            _scale = scale;
            _update();
        }
        void addScale( Geom::Scale scale) {
            _scale *= scale;
            _update();
        }

        void setRotate( Geom::Rotate rotate ) {
            _rotate = rotate;
            _update();
        }
        void setRotate( double rotate ) {
            _rotate = Geom::Rotate( rotate );
            _update();
        }
        void addRotate( Geom::Rotate rotate ) {
            _rotate *= rotate;
            _update();
        }
        void addRotate( double rotate ) {
            _rotate *= Geom::Rotate( rotate );
            _update();
        }

        void setFlip( CanvasFlip flip ) {
            _flip = Geom::Scale();
            addFlip( flip );
        }

        bool isFlipped( CanvasFlip flip ) {
            if ((flip & FLIP_HORIZONTAL) && Geom::are_near(_flip[0], -1)) {
                return true;
            }
            if ((flip & FLIP_VERTICAL) && Geom::are_near(_flip[1], -1)) {
                return true;
            }
            return false;
        }

        void addFlip( CanvasFlip flip ) {
            if (flip & FLIP_HORIZONTAL) {
                _flip *= Geom::Scale(-1.0, 1.0);
            }
            if (flip & FLIP_VERTICAL) {
                _flip *= Geom::Scale(1.0, -1.0);
            }
            _update();
        }

        double getZoom() const {
            return _d2w.descrim();
        }

        double getRotation() const {
            return _rotate.angle();
        }

        void setOffset( Geom::Point offset ) {
            _offset = offset;
        }
        void addOffset( Geom::Point offset ) {
            _offset += offset;
        }
        Geom::Point getOffset() {
            return _offset;
        }

      private:
        void _update() {
            _d2w = _scale * _rotate * _flip;
            _w2d = _d2w.inverse();
        }
        Geom::Affine  _w2d;      // Window to desktop
        Geom::Affine  _d2w;      // Desktop to window
        Geom::Rotate  _rotate;   // Rotate part of _w2d
        Geom::Scale   _scale;    // Scale part of _w2d, holds y-axis direction
        Geom::Scale   _flip;     // Flip part of _w2d
        Geom::Point   _offset;   // Point on canvas to align to (0,0) of window
    };

    DesktopAffine _current_affine;
    std::list<DesktopAffine> transforms_past;
    std::list<DesktopAffine> transforms_future;
    bool _split_canvas;
    bool _xray;
    bool _quick_zoom_enabled; ///< Signifies that currently we're in quick zoom mode
    DesktopAffine _quick_zoom_affine;  ///< The transform of the screen before quick zoom

    bool _overlays_visible = true; ///< Whether the overlays are temporarily hidden
    bool _saved_guides_visible = false; ///< Remembers guides' visibility when hiding overlays

    std::unique_ptr<Inkscape::LayerManager> _layer_manager;

    sigc::signal<void (SPDesktop*)> _destroy_signal;
    sigc::signal<void (SPDesktop*,SPDocument*)>     _document_replaced_signal;
    sigc::signal<void (SPDesktop*,Inkscape::UI::Tools::ToolBase*)> _event_context_changed_signal;
    sigc::signal<void (gpointer, SPObject*)> _tool_subselection_changed;
    sigc::signal<void (void*, SPStop*)> _gradient_stop_selected;
    sigc::signal<void (void*, Inkscape::UI::ControlPointSelection*)> _control_point_selected;
    sigc::signal<void (void*, Inkscape::UI::Tools::TextTool*)> _text_cursor_moved;

    sigc::connection _reconstruction_start_connection;
    sigc::connection _reconstruction_finish_connection;
    sigc::connection _schedule_zoom_from_document_connection;

    void onStatusMessage (Inkscape::MessageType type, gchar const *message) override;
    void onDocumentFilenameSet(gchar const* filename) override;
};

#endif // SEEN_SP_DESKTOP_H

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
