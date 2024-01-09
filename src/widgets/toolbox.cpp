// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * @file
 * Inkscape toolbar definitions and general utility functions.
 * Each tool should have its own xxx-toolbar implementation file
 */
/* Authors:
 *   MenTaLguY <mental@rydia.net>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Frank Felfe <innerspace@iname.com>
 *   John Cliff <simarilius@yahoo.com>
 *   David Turner <novalis@gnu.org>
 *   Josh Andler <scislac@scislac.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *   Jabiertxo Arraiza <jabier.arraiza@marker.es>
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2003 MenTaLguY
 * Copyright (C) 1999-2015 authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "toolbox.h"

#include <gtkmm.h>
#include <glibmm/i18n.h>

#include "actions/actions-canvas-snapping.h"
#include "actions/actions-tools.h"
#include "io/resource.h"
#include "ui/util.h"
#include "ui/builder-utils.h"
#include "ui/widget/style-swatch.h"
#include "widgets/widget-sizes.h"

#include "ui/toolbar/arc-toolbar.h"
#include "ui/toolbar/box3d-toolbar.h"
#include "ui/toolbar/calligraphy-toolbar.h"
#include "ui/toolbar/connector-toolbar.h"
#include "ui/toolbar/dropper-toolbar.h"
#include "ui/toolbar/eraser-toolbar.h"
#include "ui/toolbar/gradient-toolbar.h"
#include "ui/toolbar/lpe-toolbar.h"
#include "ui/toolbar/mesh-toolbar.h"
#include "ui/toolbar/measure-toolbar.h"
#include "ui/toolbar/node-toolbar.h"
#include "ui/toolbar/booleans-toolbar.h"
#include "ui/toolbar/rect-toolbar.h"
#include "ui/toolbar/marker-toolbar.h"
#include "ui/toolbar/page-toolbar.h"
#include "ui/toolbar/paintbucket-toolbar.h"
#include "ui/toolbar/pencil-toolbar.h"
#include "ui/toolbar/select-toolbar.h"
#include "ui/toolbar/spray-toolbar.h"
#include "ui/toolbar/spiral-toolbar.h"
#include "ui/toolbar/star-toolbar.h"
#include "ui/toolbar/tweak-toolbar.h"
#include "ui/toolbar/text-toolbar.h"
#include "ui/toolbar/zoom-toolbar.h"

#include "ui/tools/tool-base.h"

//#define DEBUG_TEXT

using Inkscape::UI::ToolboxFactory;
using Inkscape::UI::Tools::ToolBase;

using Inkscape::IO::Resource::get_filename;
using Inkscape::IO::Resource::UIS;

typedef void (*SetupFunction)(GtkWidget *toolbox, SPDesktop *desktop);
typedef void (*UpdateFunction)(SPDesktop *desktop, ToolBase *eventcontext, GtkWidget *toolbox);

enum BarId {
    BAR_TOOL = 0,
    BAR_AUX,
    BAR_COMMANDS,
    BAR_SNAP,
};

#define BAR_ID_KEY "BarIdValue"
#define HANDLE_POS_MARK "x-inkscape-pos"

int ToolboxFactory::prefToPixelSize(Glib::ustring const& path) {
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int size = prefs->getIntLimited(path, 16, 16, 48);
    return size;
}

Gtk::IconSize ToolboxFactory::prefToSize_mm(Glib::ustring const &path, int base)
{
    static Gtk::IconSize sizeChoices[] = { Gtk::ICON_SIZE_LARGE_TOOLBAR, Gtk::ICON_SIZE_SMALL_TOOLBAR,
                                           Gtk::ICON_SIZE_DND, Gtk::ICON_SIZE_DIALOG };
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int index = prefs->getIntLimited(path, base, 0, G_N_ELEMENTS(sizeChoices));
    return sizeChoices[index];
}

static struct {
    gchar const *type_name;
    Glib::ustring const tool_name;
    GtkWidget *(*create_func)(SPDesktop *desktop);
    gchar const *swatch_tip;
} const aux_toolboxes[] = {
    // If you change the tool_name for Measure or Text here, change it also in desktop-widget.cpp.
    // clang-format off
    { "/tools/select",          "Select",       Inkscape::UI::Toolbar::SelectToolbar::create,        nullptr},
    { "/tools/nodes",           "Node",         Inkscape::UI::Toolbar::NodeToolbar::create,          nullptr},
    { "/tools/booleans",        "Booleans",     Inkscape::UI::Toolbar::BooleansToolbar::create,      nullptr},
    { "/tools/marker",          "Marker",       Inkscape::UI::Toolbar::MarkerToolbar::create,        nullptr},
    { "/tools/shapes/rect",     "Rect",         Inkscape::UI::Toolbar::RectToolbar::create,          N_("Style of new rectangles")},
    { "/tools/shapes/arc",      "Arc",          Inkscape::UI::Toolbar::ArcToolbar::create,           N_("Style of new ellipses")},
    { "/tools/shapes/star",     "Star",         Inkscape::UI::Toolbar::StarToolbar::create,          N_("Style of new stars")},
    { "/tools/shapes/3dbox",    "3DBox",        Inkscape::UI::Toolbar::Box3DToolbar::create,         N_("Style of new 3D boxes")},
    { "/tools/shapes/spiral",   "Spiral",       Inkscape::UI::Toolbar::SpiralToolbar::create,        N_("Style of new spirals")},
    { "/tools/freehand/pencil", "Pencil",       Inkscape::UI::Toolbar::PencilToolbar::create_pencil, N_("Style of new paths created by Pencil")},
    { "/tools/freehand/pen",    "Pen",          Inkscape::UI::Toolbar::PencilToolbar::create_pen,    N_("Style of new paths created by Pen")},
    { "/tools/calligraphic",    "Calligraphic", Inkscape::UI::Toolbar::CalligraphyToolbar::create,   N_("Style of new calligraphic strokes")},
    { "/tools/text",            "Text",         Inkscape::UI::Toolbar::TextToolbar::create,          nullptr},
    { "/tools/gradient",        "Gradient",     Inkscape::UI::Toolbar::GradientToolbar::create,      nullptr},
    { "/tools/mesh",            "Mesh",         Inkscape::UI::Toolbar::MeshToolbar::create,          nullptr},
    { "/tools/zoom",            "Zoom",         Inkscape::UI::Toolbar::ZoomToolbar::create,          nullptr},
    { "/tools/measure",         "Measure",      Inkscape::UI::Toolbar::MeasureToolbar::create,       nullptr},
    { "/tools/dropper",         "Dropper",      Inkscape::UI::Toolbar::DropperToolbar::create,       nullptr},
    { "/tools/tweak",           "Tweak",        Inkscape::UI::Toolbar::TweakToolbar::create,         N_("Color/opacity used for color tweaking")},
    { "/tools/spray",           "Spray",        Inkscape::UI::Toolbar::SprayToolbar::create,         nullptr},
    { "/tools/connector",       "Connector",    Inkscape::UI::Toolbar::ConnectorToolbar::create,     nullptr},
    { "/tools/pages",           "Pages",        Inkscape::UI::Toolbar::PageToolbar::create,          nullptr},
    { "/tools/paintbucket",     "Paintbucket",  Inkscape::UI::Toolbar::PaintbucketToolbar::create,   N_("Style of Paint Bucket fill objects")},
    { "/tools/eraser",          "Eraser",       Inkscape::UI::Toolbar::EraserToolbar::create,        _("TBD")},
    { "/tools/lpetool",         "LPETool",      Inkscape::UI::Toolbar::LPEToolbar::create,           _("TBD")},
    { nullptr,                  "",             nullptr,                                             nullptr }
    // clang-format on
};


static void setup_aux_toolbox(GtkWidget *toolbox, SPDesktop *desktop);
static void update_aux_toolbox(SPDesktop *desktop, ToolBase *eventcontext, GtkWidget *toolbox);

static GtkWidget* toolboxNewCommon( GtkWidget* tb, BarId id, GtkPositionType /*handlePos*/ )
{
    g_object_set_data(G_OBJECT(tb), "desktop", nullptr);

    gtk_widget_set_sensitive(tb, TRUE);

    GtkWidget *hb = gtk_event_box_new(); // A simple, neutral container.
    gtk_widget_set_name(hb, "ToolboxCommon");

    gtk_container_add(GTK_CONTAINER(hb), tb);
    gtk_widget_show(GTK_WIDGET(tb));

    sigc::connection* conn = new sigc::connection;
    g_object_set_data(G_OBJECT(hb), "event_context_connection", conn);

    gpointer val = GINT_TO_POINTER(id);
    g_object_set_data(G_OBJECT(hb), BAR_ID_KEY, val);

    return hb;
}

GtkWidget *ToolboxFactory::createToolToolbox(InkscapeWindow *window)
{
    Gtk::Widget* toolbar = nullptr;

    auto builder = Inkscape::UI::create_builder("toolbar-tool.ui");
    builder->get_widget("tool-toolbar", toolbar);
    if (!toolbar) {
        std::cerr << "InkscapeWindow: Failed to load tool toolbar!" << std::endl;
    }

    _attachHandlers(builder, window);

    return toolboxNewCommon( GTK_WIDGET(toolbar->gobj()), BAR_TOOL, GTK_POS_LEFT );
}

/**
 * @brief Create a context menu for a tool button.
 * @param tool_name The tool name (parameter to the tool-switch action)
 * @param win The Inkscape window which will display the preferences dialog.
 */
Gtk::Menu *ToolboxFactory::_getContextMenu(Glib::ustring tool_name, InkscapeWindow *win)
{
    auto menu = new Gtk::Menu();
    auto gio_menu = Gio::Menu::create();
    auto action_group = Gio::SimpleActionGroup::create();
    menu->insert_action_group("ctx", action_group);
    action_group->add_action("open-tool-preferences", sigc::bind<Glib::ustring, InkscapeWindow *>(
                                                          sigc::ptr_fun(&tool_preferences), tool_name, win));

    auto menu_item = Gio::MenuItem::create(_("Open tool preferences"), "ctx.open-tool-preferences");

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getInt("/theme/menuIcons", true)) {
        auto _icon = Gio::Icon::create("preferences-system");
        menu_item->set_icon(_icon);
    }

    gio_menu->append_item(menu_item);
    menu->bind_model(gio_menu, true);
    menu->show();
    return menu;
}

/**
 * @brief Attach handlers to all tool buttons, so that double-clicking on a tool
 *        in the toolbar opens up that tool's preferences, and a right click opens a
 *        context menu with the same functionality.
 * @param builder The builder that contains a loaded UI structure containing RadioButton's.
 * @param win The Inkscape window which will display the preferences dialog.
 */
void ToolboxFactory::_attachHandlers(Glib::RefPtr<Gtk::Builder> builder, InkscapeWindow *win)
{
    for (auto &object : builder->get_objects()) {
        if (auto radio = dynamic_cast<Gtk::RadioButton *>(object.get())) {

            Glib::VariantBase action_target;
            radio->get_property("action-target", action_target);
            if (!action_target.is_of_type(Glib::VARIANT_TYPE_STRING)) {
                continue;
            }

            auto tool_name = Glib::ustring((gchar const *)action_target.get_data());

            auto menu = _getContextMenu(tool_name, win);
            menu->attach_to_widget(*radio);

            radio->signal_button_press_event().connect([=](GdkEventButton *ev) -> bool {
                // Open tool preferences upon double click
                if (ev->type == GDK_2BUTTON_PRESS && ev->button == 1) {
                    tool_preferences(tool_name, win);
                    return true;
                }
                if (ev->button == 3) {
                    menu->popup_at_pointer(reinterpret_cast<GdkEvent *>(ev));
                }
                return false;
            });
        }
    }
}

GtkWidget *ToolboxFactory::createAuxToolbox()
{
    auto tb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(tb, "AuxToolbox");
    gtk_box_set_homogeneous(GTK_BOX(tb), FALSE);

    return toolboxNewCommon( tb, BAR_AUX, GTK_POS_LEFT );
}

//####################################
//# Commands Bar
//####################################

GtkWidget *ToolboxFactory::createCommandsToolbox()
{
    auto tb = new Gtk::Box();
    tb->set_name("CommandsToolbox");
    tb->set_orientation(Gtk::ORIENTATION_VERTICAL);
    tb->set_homogeneous(false);

    Gtk::Toolbar* toolbar = nullptr;

    auto builder = Inkscape::UI::create_builder("toolbar-commands.ui");
    builder->get_widget("commands-toolbar", toolbar);
    if (!toolbar) {
        std::cerr << "ToolboxFactory: Failed to load commands toolbar!" << std::endl;
    } else {
        tb->pack_start(*toolbar, false, false);

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        if ( prefs->getBool("/toolbox/icononly", true) ) {
            toolbar->set_toolbar_style( Gtk::TOOLBAR_ICONS );
        }
    }

    return toolboxNewCommon(GTK_WIDGET(tb->gobj()), BAR_COMMANDS, GTK_POS_LEFT);
}

int show_popover(void* button) {
    auto btn = static_cast<Gtk::MenuButton*>(button);
    btn->get_popover()->show();
    return false;
}

class SnapBar : public Gtk::Box {
public:
    SnapBar() = default;
    ~SnapBar() override = default;

    Inkscape::PrefObserver _observer;
};

GtkWidget *ToolboxFactory::createSnapToolbox()
{
    auto tb = new SnapBar();
    tb->set_name("SnapToolbox");
    tb->set_homogeneous(false);

    bool simple_snap = true;
    Gtk::Toolbar* toolbar = nullptr;

    auto builder = Inkscape::UI::create_builder("toolbar-snap.ui");
    builder->get_widget("snap-toolbar", toolbar);
    if (!toolbar) {
        std::cerr << "InkscapeWindow: Failed to load snap toolbar!" << std::endl;
    } else {
        tb->pack_start(*toolbar, false, false);

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        if ( prefs->getBool("/toolbox/icononly", true) ) {
            toolbar->set_toolbar_style( Gtk::TOOLBAR_ICONS );
        }
        simple_snap = prefs->getBool("/toolbox/simplesnap", simple_snap);
    }

    Gtk::ToolItem* item_simple = nullptr;
    Gtk::ToolItem* item_advanced = nullptr;
    Gtk::MenuButton* btn_simple = nullptr;
    Gtk::MenuButton* btn_advanced = nullptr;
    Gtk::LinkButton* simple = nullptr;
    Gtk::LinkButton* advanced = nullptr;
    builder->get_widget("simple-link", simple);
    builder->get_widget("advanced-link", advanced);
    builder->get_widget("tool-item-advanced", item_advanced);
    builder->get_widget("tool-item-simple", item_simple);
    builder->get_widget("btn-simple", btn_simple);
    builder->get_widget("btn-advanced", btn_advanced);
    if (simple && advanced && item_simple && item_advanced && btn_simple && btn_advanced) {
        // keep only one popup button visible
        if (simple_snap) {
            item_simple->show();
            item_advanced->hide();
        }
        else {
            item_advanced->show();
            item_simple->hide();
        }

        // Watch snap bar preferences;
        Inkscape::Preferences* prefs = Inkscape::Preferences::get();
        tb->_observer = prefs->createObserver(ToolboxFactory::snap_bar_simple, [=](const Preferences::Entry& entry) {
            if (entry.getBool(true)) {
                item_advanced->hide();
                item_simple->show();
                // adjust snapping options when transitioning to simple scheme, since most are hidden
                transition_to_simple_snapping();
            }
            else {
                item_simple->hide();
                item_advanced->show();
            }
        });

        // switch to simple mode
        simple->signal_activate_link().connect([=](){
            g_timeout_add(250, &show_popover, btn_simple);
            Inkscape::Preferences::get()->setBool(ToolboxFactory::snap_bar_simple, true);
            return true;
        }, false);

        // switch to advanced mode
        advanced->signal_activate_link().connect([=](){
            g_timeout_add(250, &show_popover, btn_advanced);
            Inkscape::Preferences::get()->setBool(ToolboxFactory::snap_bar_simple, false);
            return true;
        }, false);
    }

    return toolboxNewCommon(GTK_WIDGET(tb->gobj()), BAR_SNAP, GTK_POS_LEFT);
}

void ToolboxFactory::setToolboxDesktop(GtkWidget *toolbox, SPDesktop *desktop)
{
    sigc::connection *conn = static_cast<sigc::connection*>(g_object_get_data(G_OBJECT(toolbox),
                                                                              "event_context_connection"));

    BarId id = static_cast<BarId>( GPOINTER_TO_INT(g_object_get_data(G_OBJECT(toolbox), BAR_ID_KEY)) );

    SetupFunction setup_func = nullptr;
    UpdateFunction update_func = nullptr;

    switch (id) {
        case BAR_TOOL:
            setup_func = nullptr; // setup_tool_toolbox;
            update_func = nullptr; // update_tool_toolbox;
            break;

        case BAR_AUX:
            toolbox = gtk_bin_get_child(GTK_BIN(toolbox));
            setup_func = setup_aux_toolbox;
            update_func = update_aux_toolbox;
            break;

        case BAR_COMMANDS:
            setup_func = nullptr; // setup_commands_toolbox;
            update_func = nullptr; // update_commands_toolbox;
            break;

        case BAR_SNAP:
            setup_func = nullptr;
            update_func = nullptr;
            break;
        default:
            g_warning("Unexpected toolbox id encountered.");
    }

    gpointer ptr = g_object_get_data(G_OBJECT(toolbox), "desktop");
    SPDesktop *old_desktop = static_cast<SPDesktop*>(ptr);

    if (old_desktop) {
        std::vector<Gtk::Widget*> children = Glib::wrap(GTK_CONTAINER(toolbox))->get_children();
        for ( auto i:children ) {
            gtk_container_remove( GTK_CONTAINER(toolbox), i->gobj() );
        }
    }

    g_object_set_data(G_OBJECT(toolbox), "desktop", (gpointer)desktop);

    if (desktop && setup_func && update_func) {
        gtk_widget_set_sensitive(toolbox, TRUE);
        setup_func(toolbox, desktop);
        update_func(desktop, desktop->event_context, toolbox);
        *conn = desktop->connectEventContextChanged(sigc::bind (sigc::ptr_fun(update_func), toolbox));
    } else {
        gtk_widget_set_sensitive(toolbox, TRUE);
    }

} // end of sp_toolbox_set_desktop()


#define noDUMP_DETAILS 1

void ToolboxFactory::setOrientation(GtkWidget* toolbox, GtkOrientation orientation)
{
#if DUMP_DETAILS
    g_message("Set orientation for %p to be %d", toolbox, orientation);
    GType type = G_OBJECT_TYPE(toolbox);
    g_message("        [%s]", g_type_name(type));
    g_message("             %p", g_object_get_data(G_OBJECT(toolbox), BAR_ID_KEY));
#endif

    GtkPositionType pos = (orientation == GTK_ORIENTATION_HORIZONTAL) ? GTK_POS_LEFT : GTK_POS_TOP;

    if (GTK_IS_BIN(toolbox)) {
#if DUMP_DETAILS
        g_message("            is a BIN");
#endif // DUMP_DETAILS
        GtkWidget* child = gtk_bin_get_child(GTK_BIN(toolbox));
        if (child) {
#if DUMP_DETAILS
            GType type2 = G_OBJECT_TYPE(child);
            g_message("            child    [%s]", g_type_name(type2));
#endif // DUMP_DETAILS

            if (GTK_IS_BOX(child)) {
#if DUMP_DETAILS
                g_message("                is a BOX");
#endif // DUMP_DETAILS

                std::vector<Gtk::Widget*> children = Glib::wrap(GTK_CONTAINER(child))->get_children();
                if (!children.empty()) {
                    for (auto curr:children) {
                        GtkWidget* child2 = curr->gobj();
#if DUMP_DETAILS
                        GType type3 = G_OBJECT_TYPE(child2);
                        g_message("                child2   [%s]", g_type_name(type3));
#endif // DUMP_DETAILS

                        if (GTK_IS_CONTAINER(child2)) {
                            std::vector<Gtk::Widget*> children2 = Glib::wrap(GTK_CONTAINER(child2))->get_children();
                            if (!children2.empty()) {
                                for (auto curr2:children2) {
                                    GtkWidget* child3 = curr2->gobj();
#if DUMP_DETAILS
                                    GType type4 = G_OBJECT_TYPE(child3);
                                    g_message("                    child3   [%s]", g_type_name(type4));
#endif // DUMP_DETAILS
                                    if (GTK_IS_TOOLBAR(child3)) {
                                        GtkToolbar* childBar = GTK_TOOLBAR(child3);
                                        gtk_orientable_set_orientation(GTK_ORIENTABLE(childBar), orientation);
                                    }
                                }
                            }
                        }


                        if (GTK_IS_TOOLBAR(child2)) {
                            GtkToolbar* childBar = GTK_TOOLBAR(child2);
                            gtk_orientable_set_orientation(GTK_ORIENTABLE(childBar), orientation);
                        } else {
                            g_message("need to add dynamic switch");
                        }
                    }
                } else {
                    // The call is being made before the toolbox proper has been setup.
                    g_object_set_data(G_OBJECT(toolbox), HANDLE_POS_MARK, GINT_TO_POINTER(pos));
                }
            } else if (GTK_IS_TOOLBAR(child)) {
                GtkToolbar* toolbar = GTK_TOOLBAR(child);
                gtk_orientable_set_orientation( GTK_ORIENTABLE(toolbar), orientation );
            }
        }
    }
}

/**
 * \brief Generate the auxiliary toolbox
 *
 * \details This is the one that appears below the main menu, and contains
 *          tool-specific toolbars.  Each toolbar is created here, using
 *          its "create" method.
 *
 *          The actual method used for each toolbar is specified in the
 *          "aux_toolboxes" array, defined above.
 */
void setup_aux_toolbox(GtkWidget *toolbox, SPDesktop *desktop)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // Loop through all the toolboxes and create them using either
    // their "create" methods.
    for (int i = 0 ; aux_toolboxes[i].type_name ; i++ ) {
        if (aux_toolboxes[i].create_func) {
            GtkWidget *sub_toolbox = aux_toolboxes[i].create_func(desktop);
            // center items vertically/horizontally to prevent stretching;
            // all buttons will look uniform across toolbars if their original size is preserved
            if (auto* tb = dynamic_cast<Gtk::Container*>(Glib::wrap(sub_toolbox))) {
                for (auto&& item : tb->get_children()) {
                    if (dynamic_cast<Gtk::Button*>(item) ||
                        dynamic_cast<Gtk::SpinButton*>(item) ||
                        dynamic_cast<Gtk::ToolButton*>(item)) {
                        item->set_valign(Gtk::ALIGN_CENTER);
                        item->set_halign(Gtk::ALIGN_CENTER);
                    }
                }
            }
            gtk_widget_set_name( sub_toolbox, "SubToolBox" );

            auto holder = gtk_grid_new();
            gtk_grid_attach(GTK_GRID(holder), sub_toolbox, 0, 0, 1, 1);

            // This part is just for styling
            if ( prefs->getBool( "/toolbox/icononly", true) ) {
                gtk_toolbar_set_style( GTK_TOOLBAR(sub_toolbox), GTK_TOOLBAR_ICONS );
            }

            int pixel_size = ToolboxFactory::prefToPixelSize(ToolboxFactory::ctrlbars_icon_size);
            Inkscape::UI::set_icon_sizes(sub_toolbox, pixel_size);
            gtk_widget_set_hexpand(sub_toolbox, TRUE);

            // Add a swatch widget if swatch tooltip is defined.
            if ( aux_toolboxes[i].swatch_tip) {
                auto swatch = new Inkscape::UI::Widget::StyleSwatch( nullptr, _(aux_toolboxes[i].swatch_tip) );
                swatch->setDesktop( desktop );
                swatch->setToolName(aux_toolboxes[i].tool_name);
                // swatch->setClickVerb( aux_toolboxes[i].swatch_verb_id );
                swatch->setWatchedTool( aux_toolboxes[i].type_name, true );
                swatch->set_margin_start(AUX_BETWEEN_BUTTON_GROUPS);
                swatch->set_margin_end(AUX_BETWEEN_BUTTON_GROUPS);
                swatch->set_margin_top(AUX_SPACING);
                swatch->set_margin_bottom(AUX_SPACING);

                auto swatch_ = GTK_WIDGET( swatch->gobj() );
                gtk_grid_attach( GTK_GRID(holder), swatch_, 1, 0, 1, 1);
            }

            // Add the new toolbar into the toolbox (i.e., make it the visible toolbar)
            // and also store a pointer to it inside the toolbox.  This allows the
            // active toolbar to be changed.
            gtk_container_add(GTK_CONTAINER(toolbox), holder);
            Glib::ustring ui_name = aux_toolboxes[i].tool_name + "Toolbar";  // If you change "Toolbar" here, change it also in desktop-widget.cpp.
            gtk_widget_set_name( holder, ui_name.c_str() );

            // TODO: We could make the toolbox a custom subclass of GtkEventBox
            //       so that we can store a list of toolbars, rather than using
            //       GObject data
            g_object_set_data(G_OBJECT(toolbox), aux_toolboxes[i].tool_name.c_str(), holder);
            gtk_widget_show(sub_toolbox);
            gtk_widget_show(holder);
        } else if (aux_toolboxes[i].swatch_tip) {
            g_warning("Could not create toolbox %s", aux_toolboxes[i].tool_name.c_str());
        }
    }
}

void update_aux_toolbox(SPDesktop * /*desktop*/, ToolBase *eventcontext, GtkWidget *toolbox)
{
    for (int i = 0 ; aux_toolboxes[i].type_name ; i++ ) {
        GtkWidget *sub_toolbox = GTK_WIDGET(g_object_get_data(G_OBJECT(toolbox), aux_toolboxes[i].tool_name.c_str()));
        if (eventcontext && eventcontext->getPrefsPath() == aux_toolboxes[i].type_name) {
            gtk_widget_show_now(sub_toolbox);
            g_object_set_data(G_OBJECT(toolbox), "shows", sub_toolbox);
        } else {
            gtk_widget_hide(sub_toolbox);
        }
        //FIX issue #Inkscape686
        GtkAllocation allocation;
        gtk_widget_get_allocation(sub_toolbox, &allocation);
        gtk_widget_size_allocate(sub_toolbox, &allocation);
    }
    //FIX issue #Inkscape125
    GtkAllocation allocation;
    gtk_widget_get_allocation(toolbox, &allocation);
    gtk_widget_size_allocate(toolbox, &allocation);  
}

void ToolboxFactory::showAuxToolbox(GtkWidget *toolbox_toplevel)
{
    gtk_widget_show(toolbox_toplevel);
    GtkWidget *toolbox = gtk_bin_get_child(GTK_BIN(toolbox_toplevel));

    GtkWidget *shown_toolbox = GTK_WIDGET(g_object_get_data(G_OBJECT(toolbox), "shows"));
    if (!shown_toolbox) {
        return;
    }
    gtk_widget_show(toolbox);
}

Glib::ustring ToolboxFactory::get_tool_visible_buttons_path(const Glib::ustring& button_action_name) {
    return Glib::ustring(ToolboxFactory::tools_visible_buttons) + "/show" + button_action_name;
}

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
