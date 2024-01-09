// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_DESKTOP_WIDGET_H
#define SEEN_SP_DESKTOP_WIDGET_H

/** \file
 * SPDesktopWidget: handling Gtk events on a desktop.
 *
 * Authors:
 *      Jon A. Cruz <jon@joncruz.org> (c) 2010
 *      John Bintz <jcoswell@coswellproductions.org> (c) 2006
 *      Ralf Stephan <ralf@ark.in-berlin.de> (c) 2005
 *      Abhishek Sharma
 *      ? -2004
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <cstddef>
#include <2geom/point.h>
#include <sigc++/connection.h>
#include <gtkmm.h>

#include "message.h"
#include "preferences.h"
#include "ui/view/view-widget.h"
#include "display/control/canvas-item-ptr.h"

// forward declaration
typedef struct _EgeColorProfTracker EgeColorProfTracker;

class InkscapeWindow;
struct SPCanvasItem;
class SPDocument;
class SPDesktop;
struct SPDesktopWidget;
class SPObject;

namespace Inkscape {
  class CanvasItemGuideLine;
namespace UI {
namespace Dialog {
class DialogContainer;
class DialogMultipaned;
class SwatchesPanel;
} // namespace Dialog

namespace Widget {
  class Button;
  class Canvas;
  class CanvasGrid;
  class LayerSelector;
  class PageSelector;
  class SelectedStyle;
  class SpinButton;
  class Ruler;
} // namespace Widget
} // namespace UI
} // namespace Inkscape

#define SP_DESKTOP_WIDGET(o) dynamic_cast<SPDesktopWidget*>(o)
#define SP_IS_DESKTOP_WIDGET(o) bool(dynamic_cast<SPDesktopWidget const *>(o))

void sp_desktop_widget_show_decorations(SPDesktopWidget *dtw, gboolean show);
void sp_desktop_widget_update_hruler (SPDesktopWidget *dtw);
void sp_desktop_widget_update_vruler (SPDesktopWidget *dtw);

/* Show/hide rulers & scrollbars */
void sp_desktop_widget_update_scrollbars (SPDesktopWidget *dtw, double scale);

/// A GtkEventBox on an SPDesktop.
class SPDesktopWidget : public SPViewWidget {
    using parent_type = SPViewWidget;

    SPDesktopWidget(InkscapeWindow *inkscape_window);

public:
    SPDesktopWidget(InkscapeWindow *inkscape_window, SPDocument *document);
    ~SPDesktopWidget() override;

    Inkscape::UI::Widget::CanvasGrid *get_canvas_grid() { return _canvas_grid; }  // Temp, I hope!
    Inkscape::UI::Widget::Canvas     *get_canvas()      { return _canvas; }

    Gio::ActionMap* get_action_map();

    void on_realize() override;
    void on_unrealize() override;

    sigc::connection modified_connection;

    SPDesktop *desktop = nullptr;

    InkscapeWindow *window = nullptr;
    Gtk::MenuBar *_menubar;
private:
    // Flags for ruler event handling
    bool _ruler_clicked = false; ///< True if the ruler has been clicked
    bool _ruler_dragged = false; ///< True if a drag on the ruler is occurring

    bool update = false;

    CanvasItemPtr<Inkscape::CanvasItemGuideLine> _active_guide; ///< The guide being handled during a ruler event
    Geom::Point _normal; ///< Normal to the guide currently being handled during ruler event
    int _xp = 0; ///< x coordinate for start of drag
    int _yp = 0; ///< y coordinate for start of drag

    // The root vbox of the window layout.
    Gtk::Box *_vbox;

    Gtk::Paned *_tbbox;
    Gtk::Box *_hbox;
    Inkscape::UI::Dialog::DialogContainer *_container = nullptr;
    Inkscape::UI::Dialog::DialogMultipaned *_columns;
    Gtk::Grid* _top_toolbars;

    Gtk::Box     *_statusbar;

    Inkscape::UI::Dialog::SwatchesPanel *_panels;

    Glib::RefPtr<Gtk::Adjustment> _hadj;
    Glib::RefPtr<Gtk::Adjustment> _vadj;

    Gtk::Grid *_coord_status;

    Gtk::Label *_select_status;
    Gtk::Label *_coord_status_x;
    Gtk::Label *_coord_status_y;

    Gtk::Box* _zoom_status_box;
    Inkscape::UI::Widget::SpinButton *_zoom_status;
    sigc::connection _zoom_status_input_connection;
    sigc::connection _zoom_status_output_connection;
    sigc::connection _zoom_status_value_changed_connection;
    sigc::connection _zoom_status_populate_popup_connection;

    Gtk::Box* _rotation_status_box;
    Inkscape::UI::Widget::SpinButton *_rotation_status = nullptr;
    sigc::connection _rotation_status_input_connection;
    sigc::connection _rotation_status_output_connection;
    sigc::connection _rotation_status_value_changed_connection;
    sigc::connection _rotation_status_populate_popup_connection;


    Inkscape::UI::Widget::SelectedStyle *_selected_style;

    /** A grid for display the canvas, rulers, and scrollbars. */
    Inkscape::UI::Widget::CanvasGrid *_canvas_grid;

    unsigned int _interaction_disabled_counter = 0;

public:
    double _dt2r;

private:
    Inkscape::UI::Widget::Canvas *_canvas = nullptr;
    std::vector<sigc::connection> _connections;
    Inkscape::PrefObserver _statusbar_preferences_observer;
    Inkscape::UI::Widget::LayerSelector* _layer_selector;
    Inkscape::UI::Widget::PageSelector* _page_selector;

public:
    EgeColorProfTracker* _tracker;

    void setMessage(Inkscape::MessageType type, gchar const *message);
    void viewSetPosition (Geom::Point p);
    void letZoomGrabFocus();
    void getWindowGeometry (gint &x, gint &y, gint &w, gint &h);
    void setWindowPosition (Geom::Point p);
    void setWindowSize (gint w, gint h);
    void setWindowTransient (void *p, int transient_policy);
    void presentWindow();
    bool showInfoDialog( Glib::ustring const &message );
    bool warnDialog (Glib::ustring const &text);
    Gtk::Toolbar* get_toolbar_by_name(const Glib::ustring& name);
    void setToolboxFocusTo (gchar const *);
    void setToolboxAdjustmentValue (gchar const * id, double value);
    bool isToolboxButtonActive (gchar const *id);
    void setCoordinateStatus(Geom::Point p);
    void enableInteraction();
    void disableInteraction();
    void updateTitle(gchar const *uri);
    bool onFocusInEvent(GdkEventFocus *);
    Inkscape::UI::Dialog::DialogContainer *getDialogContainer();
    void showNotice(Glib::ustring const &msg, unsigned timeout = 0);

    Gtk::MenuBar *menubar() { return _menubar; }

    void updateNamedview();
    void update_guides_lock();

    // Canvas Grid Widget
    void cms_adjust_set_sensitive(bool enabled);
    bool get_color_prof_adj_enabled() const;
    void toggle_color_prof_adj();
    void update_zoom();
    void update_rotation();
    void update_rulers();
    void repack_snaptoolbar();

    void iconify();
    void maximize();
    void fullscreen();
    static gint ruler_event(GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw, bool horiz);

    void layoutWidgets();
    void toggle_scrollbars();
    void update_scrollbars(double scale);
    void toggle_command_palette();
    void toggle_rulers();
    void sticky_zoom_toggled();
    void sticky_zoom_updated();

    Gtk::Widget *get_tool_toolbox() const { return Glib::wrap(tool_toolbox); }
private:
    GtkWidget *tool_toolbox;
    GtkWidget *aux_toolbox;
    GtkWidget *commands_toolbox;
    GtkWidget *snap_toolbox;
    Inkscape::PrefObserver _tb_snap_pos;
    Inkscape::PrefObserver _tb_icon_sizes1;
    Inkscape::PrefObserver _tb_icon_sizes2;
    Inkscape::PrefObserver _tb_visible_buttons;
    Inkscape::PrefObserver _ds_sticky_zoom;

    void namedviewModified(SPObject *obj, guint flags);
    int zoom_input(double *new_val);
    bool zoom_output();
    void zoom_value_changed();
    void zoom_menu_handler(double factor);
    void zoom_populate_popup(Gtk::Menu *menu);
    bool rotation_output();
    void rotation_value_changed();
    void rotation_populate_popup(Gtk::Menu *menu);
  //void canvas_tbl_size_allocate(Gtk::Allocation &allocation);
    void update_statusbar_visibility();
    void apply_ctrlbar_settings();

public:
    void cms_adjust_toggled();
private:
    static void color_profile_event(EgeColorProfTracker *tracker, SPDesktopWidget *dtw);
    static void ruler_snap_new_guide(SPDesktop *desktop, Geom::Point &event_dt, Geom::Point &normal);
    static gint event(GtkWidget *widget, GdkEvent *event, SPDesktopWidget *dtw);

public: // Move to CanvasGrid
    bool on_ruler_box_button_press_event(GdkEventButton *event, Gtk::Widget *widget, bool horiz);
    bool on_ruler_box_button_release_event(GdkEventButton *event, Gtk::Widget *widget, bool horiz);
    bool on_ruler_box_motion_notify_event(GdkEventMotion *event, Gtk::Widget *widget, bool horiz);
    void on_adjustment_value_changed();
};

#endif /* !SEEN_SP_DESKTOP_WIDGET_H */

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
