// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <set>
#include <utility>

#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <glibmm/i18n.h>

#include "colorspace.h"
#include "document.h"
#include "inkscape.h"
#include "profile-manager.h"

#include "svg/svg-icc-color.h"

#include "ui/dialog-events.h"
#include "ui/util.h"
#include "ui/widget/color-icc-selector.h"
#include "ui/widget/color-scales.h"
#include "ui/widget/color-slider.h"
#include "ui/widget/scrollprotected.h"

#define noDEBUG_LCMS

#include "object/color-profile.h"
#include "cms-system.h"
#include "color-profile-cms-fns.h"

#ifdef DEBUG_LCMS
#include "preferences.h"
#endif // DEBUG_LCMS

#ifdef DEBUG_LCMS
extern guint update_in_progress;
#define DEBUG_MESSAGE(key, ...)                                                                                        \
    {                                                                                                                  \
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();                                                   \
        bool dump = prefs->getBool("/options/scislac/" #key);                                                          \
        bool dumpD = prefs->getBool("/options/scislac/" #key "D");                                                     \
        bool dumpD2 = prefs->getBool("/options/scislac/" #key "D2");                                                   \
        dumpD && = ((update_in_progress == 0) || dumpD2);                                                              \
        if (dump) {                                                                                                    \
            g_message(__VA_ARGS__);                                                                                    \
        }                                                                                                              \
        if (dumpD) {                                                                                                   \
            GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,         \
                                                       GTK_BUTTONS_OK, __VA_ARGS__);                                   \
            g_signal_connect_swapped(dialog, "response", G_CALLBACK(gtk_widget_destroy), dialog);                      \
            gtk_widget_show_all(dialog);                                                                               \
        }                                                                                                              \
    }
#endif // DEBUG_LCMS


#define XPAD 4
#define YPAD 1

namespace {

GtkWidget *_scrollprotected_combo_box_new_with_model(GtkTreeModel *model)
{
    auto combobox = Gtk::manage(new Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBox>());
    gtk_combo_box_set_model(combobox->gobj(), model);
    return GTK_WIDGET(combobox->gobj());
}

size_t maxColorspaceComponentCount = 0;


/**
 * Internal variable to track all known colorspaces.
 */
std::set<cmsUInt32Number> knownColorspaces;

/**
 * Helper function to handle GTK2/GTK3 attachment #ifdef code.
 */
void attachToGridOrTable(GtkWidget *parent, GtkWidget *child, guint left, guint top, guint width, guint height,
                         bool hexpand = false, bool centered = false, guint xpadding = XPAD, guint ypadding = YPAD)
{
    gtk_widget_set_margin_start(child, xpadding);
    gtk_widget_set_margin_end(child, xpadding);
    gtk_widget_set_margin_top(child, ypadding);
    gtk_widget_set_margin_bottom(child, ypadding);

    if (hexpand) {
        gtk_widget_set_hexpand(child, TRUE);
    }

    if (centered) {
        gtk_widget_set_halign(child, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
    }

    gtk_grid_attach(GTK_GRID(parent), child, left, top, width, height);
}

} // namespace

/*
icSigRgbData
icSigCmykData
icSigCmyData
*/
#define SPACE_ID_RGB 0
#define SPACE_ID_CMY 1
#define SPACE_ID_CMYK 2


colorspace::Component::Component()
    : name()
    , tip()
    , scale(1)
{
}

colorspace::Component::Component(std::string name, std::string tip, guint scale)
    : name(std::move(name))
    , tip(std::move(tip))
    , scale(scale)
{
}

static cmsUInt16Number *getScratch()
{
    // bytes per pixel * input channels * width
    static cmsUInt16Number *scritch = static_cast<cmsUInt16Number *>(g_new(cmsUInt16Number, 4 * 1024));

    return scritch;
}

std::vector<colorspace::Component> colorspace::getColorSpaceInfo(uint32_t space)
{
    static std::map<cmsUInt32Number, std::vector<Component> > sets;
    if (sets.empty()) {
        sets[cmsSigXYZData].push_back(Component("_X", "X", 2)); //  TYPE_XYZ_16
        sets[cmsSigXYZData].push_back(Component("_Y", "Y", 1));
        sets[cmsSigXYZData].push_back(Component("_Z", "Z", 2));

        sets[cmsSigLabData].push_back(Component("_L", "L", 100)); // TYPE_Lab_16
        sets[cmsSigLabData].push_back(Component("_a", "a", 256));
        sets[cmsSigLabData].push_back(Component("_b", "b", 256));

        // cmsSigLuvData

        sets[cmsSigYCbCrData].push_back(Component("_Y", "Y", 1)); // TYPE_YCbCr_16
        sets[cmsSigYCbCrData].push_back(Component("C_b", "Cb", 1));
        sets[cmsSigYCbCrData].push_back(Component("C_r", "Cr", 1));

        sets[cmsSigYxyData].push_back(Component("_Y", "Y", 1)); // TYPE_Yxy_16
        sets[cmsSigYxyData].push_back(Component("_x", "x", 1));
        sets[cmsSigYxyData].push_back(Component("y", "y", 1));

        sets[cmsSigRgbData].push_back(Component(_("_R:"), _("Red"), 1)); // TYPE_RGB_16
        sets[cmsSigRgbData].push_back(Component(_("_G:"), _("Green"), 1));
        sets[cmsSigRgbData].push_back(Component(_("_B:"), _("Blue"), 1));

        sets[cmsSigGrayData].push_back(Component(_("G:"), _("Gray"), 1)); // TYPE_GRAY_16

        sets[cmsSigHsvData].push_back(Component(_("_H:"), _("Hue"), 360)); // TYPE_HSV_16
        sets[cmsSigHsvData].push_back(Component(_("_S:"), _("Saturation"), 1));
        sets[cmsSigHsvData].push_back(Component("_V:", "Value", 1));

        sets[cmsSigHlsData].push_back(Component(_("_H:"), _("Hue"), 360)); // TYPE_HLS_16
        sets[cmsSigHlsData].push_back(Component(_("_L:"), _("Lightness"), 1));
        sets[cmsSigHlsData].push_back(Component(_("_S:"), _("Saturation"), 1));

        sets[cmsSigCmykData].push_back(Component(_("_C:"), _("Cyan"), 1)); // TYPE_CMYK_16
        sets[cmsSigCmykData].push_back(Component(_("_M:"), _("Magenta"), 1));
        sets[cmsSigCmykData].push_back(Component(_("_Y:"), _("Yellow"), 1));
        sets[cmsSigCmykData].push_back(Component(_("_K:"), _("Black"), 1));

        sets[cmsSigCmyData].push_back(Component(_("_C:"), _("Cyan"), 1)); // TYPE_CMY_16
        sets[cmsSigCmyData].push_back(Component(_("_M:"), _("Magenta"), 1));
        sets[cmsSigCmyData].push_back(Component(_("_Y:"), _("Yellow"), 1));

        for (auto & set : sets) {
            knownColorspaces.insert(set.first);
            maxColorspaceComponentCount = std::max(maxColorspaceComponentCount, set.second.size());
        }
    }

    std::vector<Component> target;

    if (sets.find(space) != sets.end()) {
        target = sets[space];
    }
    return target;
}


std::vector<colorspace::Component> colorspace::getColorSpaceInfo(Inkscape::ColorProfile *prof)
{
    return getColorSpaceInfo(asICColorSpaceSig(prof->getColorSpace()));
}

namespace Inkscape {
namespace UI {
namespace Widget {

/**
 * Class containing the parts for a single color component's UI presence.
 */
class ComponentUI {
  public:
    ComponentUI()
        : _component()
        , _adj(nullptr)
        , _slider(nullptr)
        , _btn(nullptr)
        , _label(nullptr)
        , _map(nullptr)
    {
    }

    ComponentUI(colorspace::Component component)
        : _component(std::move(component))
        , _adj(nullptr)
        , _slider(nullptr)
        , _btn(nullptr)
        , _label(nullptr)
        , _map(nullptr)
    {
    }

    colorspace::Component _component;
    Glib::RefPtr<Gtk::Adjustment> _adj; // Component adjustment
    Inkscape::UI::Widget::ColorSlider *_slider;
    GtkWidget *_btn;   // spinbutton
    GtkWidget *_label; // Label
    guchar *_map;
};

/**
 * Class that implements the internals of the selector.
 */
class ColorICCSelectorImpl {
  public:
    ColorICCSelectorImpl(ColorICCSelector *owner, SelectedColor &color);

    ~ColorICCSelectorImpl();

    void _adjustmentChanged(Glib::RefPtr<Gtk::Adjustment> &adjustment);

    void _sliderGrabbed();
    void _sliderReleased();
    void _sliderChanged();

    static void _profileSelected(GtkWidget *src, gpointer data);
    static void _fixupHit(GtkWidget *src, gpointer data);

    void _setProfile(SVGICCColor *profile);
    void _switchToProfile(gchar const *name);
    
    void _updateSliders(gint ignore);
    void _profilesChanged(std::string const &name);

    ColorICCSelector *_owner;
    SelectedColor &_color;

    gboolean _updating : 1;
    gboolean _dragging : 1;

    guint32 _fixupNeeded;
    GtkWidget *_fixupBtn;
    GtkWidget *_profileSel;

    std::vector<ComponentUI> _compUI;

    Glib::RefPtr<Gtk::Adjustment> _adj; // Channel adjustment
    Inkscape::UI::Widget::ColorSlider *_slider;
    GtkWidget *_sbtn;  // Spinbutton
    GtkWidget *_label; // Label

    std::string _profileName;
    Inkscape::ColorProfile *_prof;
    guint _profChannelCount;
    gulong _profChangedID;
};



const gchar *ColorICCSelector::MODE_NAME = N_("CMS");

ColorICCSelector::ColorICCSelector(SelectedColor &color)
    : _impl(nullptr)
{
    _impl = new ColorICCSelectorImpl(this, color);
    init();
    color.signal_changed.connect(sigc::mem_fun(this, &ColorICCSelector::_colorChanged));
    // color.signal_dragged.connect(sigc::mem_fun(this, &ColorICCSelector::_colorChanged));
}

ColorICCSelector::~ColorICCSelector()
{
    if (_impl) {
        delete _impl;
        _impl = nullptr;
    }
}



ColorICCSelectorImpl::ColorICCSelectorImpl(ColorICCSelector *owner, SelectedColor &color)
    : _owner(owner)
    , _color(color)
    , _updating(FALSE)
    , _dragging(FALSE)
    , _fixupNeeded(0)
    , _fixupBtn(nullptr)
    , _profileSel(nullptr)
    , _compUI()
    , _adj(nullptr)
    , _slider(nullptr)
    , _sbtn(nullptr)
    , _label(nullptr)
    , _profileName()
    , _prof(nullptr)
    , _profChannelCount(0)
    , _profChangedID(0)
{
}

ColorICCSelectorImpl::~ColorICCSelectorImpl()
{
    _sbtn = nullptr;
    _label = nullptr;
}

void ColorICCSelector::init()
{
    gint row = 0;

    _impl->_updating = FALSE;
    _impl->_dragging = FALSE;

    GtkWidget *t = GTK_WIDGET(gobj());

    _impl->_compUI.clear();

    // Create components
    row = 0;


    _impl->_fixupBtn = gtk_button_new_with_label(_("Fix"));
    g_signal_connect(G_OBJECT(_impl->_fixupBtn), "clicked", G_CALLBACK(ColorICCSelectorImpl::_fixupHit),
                     (gpointer)_impl);
    gtk_widget_set_sensitive(_impl->_fixupBtn, FALSE);
    gtk_widget_set_tooltip_text(_impl->_fixupBtn, _("Fix RGB fallback to match icc-color() value."));
    gtk_widget_show(_impl->_fixupBtn);

    attachToGridOrTable(t, _impl->_fixupBtn, 0, row, 1, 1);

    // Combobox and store with 2 columns : label (0) and full name (1)
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    _impl->_profileSel = _scrollprotected_combo_box_new_with_model(GTK_TREE_MODEL(store));

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(_impl->_profileSel), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(_impl->_profileSel), renderer, "text", 0, nullptr);

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, _("<none>"), 1, _("<none>"), -1);

    gtk_widget_show(_impl->_profileSel);
    gtk_combo_box_set_active(GTK_COMBO_BOX(_impl->_profileSel), 0);

    attachToGridOrTable(t, _impl->_profileSel, 1, row, 1, 1);

    _impl->_profChangedID = g_signal_connect(G_OBJECT(_impl->_profileSel), "changed",
                                             G_CALLBACK(ColorICCSelectorImpl::_profileSelected), (gpointer)_impl);

    row++;

// populate the data for colorspaces and channels:
    std::vector<colorspace::Component> things = colorspace::getColorSpaceInfo(cmsSigRgbData);

    for (size_t i = 0; i < maxColorspaceComponentCount; i++) {
        if (i < things.size()) {
            _impl->_compUI.emplace_back(things[i]);
        }
        else {
            _impl->_compUI.emplace_back();
        }

        std::string labelStr = (i < things.size()) ? things[i].name.c_str() : "";
        
        _impl->_compUI[i]._label = gtk_label_new_with_mnemonic(labelStr.c_str());

        gtk_widget_set_halign(_impl->_compUI[i]._label, GTK_ALIGN_END);
        gtk_widget_show(_impl->_compUI[i]._label);
        gtk_widget_set_no_show_all(_impl->_compUI[i]._label, TRUE);

        attachToGridOrTable(t, _impl->_compUI[i]._label, 0, row, 1, 1);

        // Adjustment
        guint scaleValue = _impl->_compUI[i]._component.scale;
        gdouble step = static_cast<gdouble>(scaleValue) / 100.0;
        gdouble page = static_cast<gdouble>(scaleValue) / 10.0;
        gint digits = (step > 0.9) ? 0 : 2;
        _impl->_compUI[i]._adj = Gtk::Adjustment::create(0.0, 0.0, scaleValue, step, page, page);

        // Slider
        _impl->_compUI[i]._slider =
            Gtk::manage(new Inkscape::UI::Widget::ColorSlider(_impl->_compUI[i]._adj));
        _impl->_compUI[i]._slider->set_tooltip_text((i < things.size()) ? things[i].tip.c_str() : "");
        _impl->_compUI[i]._slider->show();
        _impl->_compUI[i]._slider->set_no_show_all();

        attachToGridOrTable(t, _impl->_compUI[i]._slider->gobj(), 1, row, 1, 1, true);

        auto spinbutton = Gtk::manage(new ScrollProtected<Gtk::SpinButton>(_impl->_compUI[i]._adj, step, digits));
        _impl->_compUI[i]._btn = GTK_WIDGET(spinbutton->gobj());
        gtk_widget_set_tooltip_text(_impl->_compUI[i]._btn, (i < things.size()) ? things[i].tip.c_str() : "");
        sp_dialog_defocus_on_enter(_impl->_compUI[i]._btn);
        gtk_label_set_mnemonic_widget(GTK_LABEL(_impl->_compUI[i]._label), _impl->_compUI[i]._btn);
        gtk_widget_show(_impl->_compUI[i]._btn);
        gtk_widget_set_no_show_all(_impl->_compUI[i]._btn, TRUE);

        attachToGridOrTable(t, _impl->_compUI[i]._btn, 2, row, 1, 1, false, true);

        _impl->_compUI[i]._map = g_new(guchar, 4 * 1024);
        memset(_impl->_compUI[i]._map, 0x0ff, 1024 * 4);


        // Signals
        _impl->_compUI[i]._adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_adjustmentChanged), _impl->_compUI[i]._adj));

        _impl->_compUI[i]._slider->signal_grabbed.connect(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderGrabbed));
        _impl->_compUI[i]._slider->signal_released.connect(
            sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderReleased));
        _impl->_compUI[i]._slider->signal_value_changed.connect(
            sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderChanged));

        row++;
    }

    // Label
    _impl->_label = gtk_label_new_with_mnemonic(_("_A:"));

    gtk_widget_set_halign(_impl->_label, GTK_ALIGN_END);
    gtk_widget_show(_impl->_label);

    attachToGridOrTable(t, _impl->_label, 0, row, 1, 1);

    // Adjustment
    _impl->_adj = Gtk::Adjustment::create(0.0, 0.0, 100.0, 1.0, 10.0, 10.0);

    // Slider
    _impl->_slider = Gtk::manage(new Inkscape::UI::Widget::ColorSlider(_impl->_adj));
    _impl->_slider->set_tooltip_text(_("Alpha (opacity)"));
    _impl->_slider->show();

    attachToGridOrTable(t, _impl->_slider->gobj(), 1, row, 1, 1, true);

    _impl->_slider->setColors(SP_RGBA32_F_COMPOSE(1.0, 1.0, 1.0, 0.0), SP_RGBA32_F_COMPOSE(1.0, 1.0, 1.0, 0.5),
                              SP_RGBA32_F_COMPOSE(1.0, 1.0, 1.0, 1.0));


    // Spinbutton
    auto spinbuttonalpha = Gtk::manage(new ScrollProtected<Gtk::SpinButton>(_impl->_adj, 1.0));
    _impl->_sbtn = GTK_WIDGET(spinbuttonalpha->gobj());
    gtk_widget_set_tooltip_text(_impl->_sbtn, _("Alpha (opacity)"));
    sp_dialog_defocus_on_enter(_impl->_sbtn);
    gtk_label_set_mnemonic_widget(GTK_LABEL(_impl->_label), _impl->_sbtn);
    gtk_widget_show(_impl->_sbtn);

    attachToGridOrTable(t, _impl->_sbtn, 2, row, 1, 1, false, true);

    // Signals
    _impl->_adj->signal_value_changed().connect(sigc::bind(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_adjustmentChanged), _impl->_adj));

    _impl->_slider->signal_grabbed.connect(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderGrabbed));
    _impl->_slider->signal_released.connect(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderReleased));
    _impl->_slider->signal_value_changed.connect(sigc::mem_fun(_impl, &ColorICCSelectorImpl::_sliderChanged));

    gtk_widget_show(t);
}

void ColorICCSelectorImpl::_fixupHit(GtkWidget * /*src*/, gpointer data)
{
    ColorICCSelectorImpl *self = reinterpret_cast<ColorICCSelectorImpl *>(data);
    gtk_widget_set_sensitive(self->_fixupBtn, FALSE);
    self->_adjustmentChanged(self->_compUI[0]._adj);
}

void ColorICCSelectorImpl::_profileSelected(GtkWidget * /*src*/, gpointer data)
{
    ColorICCSelectorImpl *self = reinterpret_cast<ColorICCSelectorImpl *>(data);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(self->_profileSel), &iter)) {
        GtkTreeModel *store = gtk_combo_box_get_model(GTK_COMBO_BOX(self->_profileSel));
        gchar *name = nullptr;

        gtk_tree_model_get(store, &iter, 1, &name, -1);
        self->_switchToProfile(name);
        gtk_widget_set_tooltip_text(self->_profileSel, name);

        g_free(name);
    }
}

void ColorICCSelectorImpl::_switchToProfile(gchar const *name)
{
    bool dirty = false;
    SPColor tmp(_color.color());

    if (name) {
        if (tmp.icc && tmp.icc->colorProfile == name) {
#ifdef DEBUG_LCMS
            g_message("Already at name [%s]", name);
#endif // DEBUG_LCMS
        }
        else {
#ifdef DEBUG_LCMS
            g_message("Need to switch to profile [%s]", name);
#endif // DEBUG_LCMS
            if (tmp.icc) {
                tmp.icc->colors.clear();
            }
            else {
                tmp.icc = new SVGICCColor();
            }
            tmp.icc->colorProfile = name;
            Inkscape::ColorProfile *newProf = SP_ACTIVE_DOCUMENT->getProfileManager()->find(name);
            if (newProf) {
                cmsHTRANSFORM trans = newProf->getTransfFromSRGB8();
                if (trans) {
                    guint32 val = _color.color().toRGBA32(0);
                    guchar pre[4] = {
                        static_cast<guchar>(SP_RGBA32_R_U(val)),
                        static_cast<guchar>(SP_RGBA32_G_U(val)),
                        static_cast<guchar>(SP_RGBA32_B_U(val)),
                        255};
#ifdef DEBUG_LCMS
                    g_message("Shoving in [%02x] [%02x] [%02x]", pre[0], pre[1], pre[2]);
#endif // DEBUG_LCMS
                    cmsUInt16Number post[4] = { 0, 0, 0, 0 };
                    cmsDoTransform(trans, pre, post, 1);
#ifdef DEBUG_LCMS
                    g_message("got on out [%04x] [%04x] [%04x] [%04x]", post[0], post[1], post[2], post[3]);
#endif // DEBUG_LCMS
                    guint count = cmsChannelsOf(asICColorSpaceSig(newProf->getColorSpace()));

                    std::vector<colorspace::Component> things =
                        colorspace::getColorSpaceInfo(asICColorSpaceSig(newProf->getColorSpace()));

                    for (guint i = 0; i < count; i++) {
                        gdouble val =
                            (((gdouble)post[i]) / 65535.0) * (gdouble)((i < things.size()) ? things[i].scale : 1);
#ifdef DEBUG_LCMS
                        g_message("     scaled %d by %d to be %f", i, ((i < things.size()) ? things[i].scale : 1), val);
#endif // DEBUG_LCMS
                        tmp.icc->colors.push_back(val);
                    }
                    cmsHTRANSFORM retrans = newProf->getTransfToSRGB8();
                    if (retrans) {
                        cmsDoTransform(retrans, post, pre, 1);
#ifdef DEBUG_LCMS
                        g_message("  back out [%02x] [%02x] [%02x]", pre[0], pre[1], pre[2]);
#endif // DEBUG_LCMS
                        tmp.set(SP_RGBA32_U_COMPOSE(pre[0], pre[1], pre[2], 0xff));
                    }

                    dirty = true;
                }
            }
        }
    }
    else {
#ifdef DEBUG_LCMS
        g_message("NUKE THE ICC");
#endif // DEBUG_LCMS
        if (tmp.icc) {
            delete tmp.icc;
            tmp.icc = nullptr;
            dirty = true;
            _fixupHit(nullptr, this);
        }
        else {
#ifdef DEBUG_LCMS
            g_message("No icc to nuke");
#endif // DEBUG_LCMS
        }
    }

    if (dirty) {
#ifdef DEBUG_LCMS
        g_message("+----------------");
        g_message("+   new color is [%s]", tmp.toString().c_str());
#endif // DEBUG_LCMS
        _setProfile(tmp.icc);
        //_adjustmentChanged( _compUI[0]._adj, SP_COLOR_ICC_SELECTOR(_csel) );
        _color.setColor(tmp);
#ifdef DEBUG_LCMS
        g_message("+_________________");
#endif // DEBUG_LCMS
    }
}

struct _cmp {
  bool operator()(const SPObject * const & a, const SPObject * const & b)
  {
    const Inkscape::ColorProfile &a_prof = reinterpret_cast<const Inkscape::ColorProfile &>(*a);
    const Inkscape::ColorProfile &b_prof = reinterpret_cast<const Inkscape::ColorProfile &>(*b);
    gchar *a_name_casefold = g_utf8_casefold(a_prof.name, -1 );
    gchar *b_name_casefold = g_utf8_casefold(b_prof.name, -1 );
    int result = g_strcmp0(a_name_casefold, b_name_casefold);
    g_free(a_name_casefold);
    g_free(b_name_casefold);
    return result < 0;
  }
};

template <typename From, typename To>
struct static_caster { To * operator () (From * value) const { return static_cast<To *>(value); } };

void ColorICCSelectorImpl::_profilesChanged(std::string const &name)
{
    GtkComboBox *combo = GTK_COMBO_BOX(_profileSel);

    g_signal_handler_block(G_OBJECT(_profileSel), _profChangedID);

    GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(combo));
    gtk_list_store_clear(store);

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, _("<none>"), 1, _("<none>"), -1);

    gtk_combo_box_set_active(combo, 0);

    int index = 1;
    std::vector<SPObject *> current = SP_ACTIVE_DOCUMENT->getResourceList("iccprofile");

    std::set<Inkscape::ColorProfile *> _current;
    std::transform(current.begin(),
                   current.end(),
                   std::inserter(_current, _current.begin()),
                   static_caster<SPObject, Inkscape::ColorProfile>());

    for (auto &it: _current) {
        Inkscape::ColorProfile *prof = it;

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, ink_ellipsize_text(prof->name, 25).c_str(), 1, prof->name, -1);

        if (name == prof->name) {
            gtk_combo_box_set_active(combo, index);
            gtk_widget_set_tooltip_text(_profileSel, prof->name);
        }

        index++;
    }

    g_signal_handler_unblock(G_OBJECT(_profileSel), _profChangedID);
}

void ColorICCSelector::on_show()
{
    Gtk::Grid::on_show();
    _colorChanged();
}

// Helpers for setting color value

void ColorICCSelector::_colorChanged()
{
    _impl->_updating = TRUE;
// sp_color_icc_set_color( SP_COLOR_ICC( _icc ), &color );

#ifdef DEBUG_LCMS
    g_message("/^^^^^^^^^  %p::_colorChanged(%08x:%s)", this, _impl->_color.color().toRGBA32(_impl->_color.alpha()),
              ((_impl->_color.color().icc) ? _impl->_color.color().icc->colorProfile.c_str() : "<null>"));
#endif // DEBUG_LCMS

#ifdef DEBUG_LCMS
    g_message("FLIPPIES!!!!     %p   '%s'", _impl->_color.color().icc,
              (_impl->_color.color().icc ? _impl->_color.color().icc->colorProfile.c_str() : "<null>"));
#endif // DEBUG_LCMS

    _impl->_profilesChanged((_impl->_color.color().icc) ? _impl->_color.color().icc->colorProfile : std::string(""));
    ColorScales::setScaled(_impl->_adj, _impl->_color.alpha());

    _impl->_setProfile(_impl->_color.color().icc);
    _impl->_fixupNeeded = 0;
    gtk_widget_set_sensitive(_impl->_fixupBtn, FALSE);

    if (_impl->_prof) {
        if (_impl->_prof->getTransfToSRGB8()) {
            cmsUInt16Number tmp[4];
            for (guint i = 0; i < _impl->_profChannelCount; i++) {
                gdouble val = 0.0;
                if (_impl->_color.color().icc->colors.size() > i) {
                    if (_impl->_compUI[i]._component.scale == 256) {
                        val = (_impl->_color.color().icc->colors[i] + 128.0) /
                              static_cast<gdouble>(_impl->_compUI[i]._component.scale);
                    }
                    else {
                        val = _impl->_color.color().icc->colors[i] /
                              static_cast<gdouble>(_impl->_compUI[i]._component.scale);
                    }
                }
                tmp[i] = val * 0x0ffff;
            }
            guchar post[4] = { 0, 0, 0, 0 };
            cmsHTRANSFORM trans = _impl->_prof->getTransfToSRGB8();
            if (trans) {
                cmsDoTransform(trans, tmp, post, 1);
                guint32 other = SP_RGBA32_U_COMPOSE(post[0], post[1], post[2], 255);
                if (other != _impl->_color.color().toRGBA32(255)) {
                    _impl->_fixupNeeded = other;
                    gtk_widget_set_sensitive(_impl->_fixupBtn, TRUE);
#ifdef DEBUG_LCMS
                    g_message("Color needs to change 0x%06x to 0x%06x", _color.toRGBA32(255) >> 8, other >> 8);
#endif // DEBUG_LCMS
                }
            }
        }
    }
    _impl->_updateSliders(-1);


    _impl->_updating = FALSE;
#ifdef DEBUG_LCMS
    g_message("\\_________  %p::_colorChanged()", this);
#endif // DEBUG_LCMS
}

void ColorICCSelectorImpl::_setProfile(SVGICCColor *profile)
{
#ifdef DEBUG_LCMS
    g_message("/^^^^^^^^^  %p::_setProfile(%s)", this, ((profile) ? profile->colorProfile.c_str() : "<null>"));
#endif // DEBUG_LCMS
    bool profChanged = false;
    if (_prof && (!profile || (_profileName != profile->colorProfile))) {
        // Need to clear out the prior one
        profChanged = true;
        _profileName.clear();
        _prof = nullptr;
        _profChannelCount = 0;
    }
    else if (profile && !_prof) {
        profChanged = true;
    }

    for (auto & i : _compUI) {
        gtk_widget_hide(i._label);
        i._slider->hide();
        gtk_widget_hide(i._btn);
    }

    if (profile) {
        _prof = SP_ACTIVE_DOCUMENT->getProfileManager()->find(profile->colorProfile.c_str());
        if (_prof && (asICColorProfileClassSig(_prof->getProfileClass()) != cmsSigNamedColorClass)) {
            _profChannelCount = cmsChannelsOf(asICColorSpaceSig(_prof->getColorSpace()));

            if (profChanged) {
                std::vector<colorspace::Component> things =
                    colorspace::getColorSpaceInfo(asICColorSpaceSig(_prof->getColorSpace()));
                for (size_t i = 0; (i < things.size()) && (i < _profChannelCount); ++i) {
                    _compUI[i]._component = things[i];
                }

                for (guint i = 0; i < _profChannelCount; i++) {
                    gtk_label_set_text_with_mnemonic(GTK_LABEL(_compUI[i]._label),
                                                     (i < things.size()) ? things[i].name.c_str() : "");

                    _compUI[i]._slider->set_tooltip_text((i < things.size()) ? things[i].tip.c_str() : "");
                    gtk_widget_set_tooltip_text(_compUI[i]._btn, (i < things.size()) ? things[i].tip.c_str() : "");

                    _compUI[i]._slider->setColors(SPColor(0.0, 0.0, 0.0).toRGBA32(0xff),
                                                  SPColor(0.5, 0.5, 0.5).toRGBA32(0xff),
                                                  SPColor(1.0, 1.0, 1.0).toRGBA32(0xff));
                    /*
                                        _compUI[i]._adj = GTK_ADJUSTMENT( gtk_adjustment_new( val, 0.0, _fooScales[i],
                       step, page, page ) );
                                        g_signal_connect( G_OBJECT( _compUI[i]._adj ), "value_changed", G_CALLBACK(
                       _adjustmentChanged ), _csel );

                                        sp_color_slider_set_adjustment( SP_COLOR_SLIDER(_compUI[i]._slider),
                       _compUI[i]._adj );
                                        gtk_spin_button_set_adjustment( GTK_SPIN_BUTTON(_compUI[i]._btn),
                       _compUI[i]._adj );
                                        gtk_spin_button_set_digits( GTK_SPIN_BUTTON(_compUI[i]._btn), digits );
                    */
                    gtk_widget_show(_compUI[i]._label);
                    _compUI[i]._slider->show();
                    gtk_widget_show(_compUI[i]._btn);
                    // gtk_adjustment_set_value( _compUI[i]._adj, 0.0 );
                    // gtk_adjustment_set_value( _compUI[i]._adj, val );
                }
                for (size_t i = _profChannelCount; i < _compUI.size(); i++) {
                    gtk_widget_hide(_compUI[i]._label);
                    _compUI[i]._slider->hide();
                    gtk_widget_hide(_compUI[i]._btn);
                }
            }
        }
        else {
            // Give up for now on named colors
            _prof = nullptr;
        }
    }

#ifdef DEBUG_LCMS
    g_message("\\_________  %p::_setProfile()", this);
#endif // DEBUG_LCMS
}

void ColorICCSelectorImpl::_updateSliders(gint ignore)
{
    if (_color.color().icc) {
        for (guint i = 0; i < _profChannelCount; i++) {
            gdouble val = 0.0;
            if (_color.color().icc->colors.size() > i) {
                if (_compUI[i]._component.scale == 256) {
                    val = (_color.color().icc->colors[i] + 128.0) / static_cast<gdouble>(_compUI[i]._component.scale);
                }
                else {
                    val = _color.color().icc->colors[i] / static_cast<gdouble>(_compUI[i]._component.scale);
                }
            }
            _compUI[i]._adj->set_value(val);
        }

        if (_prof) {
            if (_prof->getTransfToSRGB8()) {
                for (guint i = 0; i < _profChannelCount; i++) {
                    if (static_cast<gint>(i) != ignore) {
                        cmsUInt16Number *scratch = getScratch();
                        cmsUInt16Number filler[4] = { 0, 0, 0, 0 };
                        for (guint j = 0; j < _profChannelCount; j++) {
                            filler[j] = 0x0ffff * ColorScales::getScaled(_compUI[j]._adj);
                        }

                        cmsUInt16Number *p = scratch;
                        for (guint x = 0; x < 1024; x++) {
                            for (guint j = 0; j < _profChannelCount; j++) {
                                if (j == i) {
                                    *p++ = x * 0x0ffff / 1024;
                                }
                                else {
                                    *p++ = filler[j];
                                }
                            }
                        }

                        cmsHTRANSFORM trans = _prof->getTransfToSRGB8();
                        if (trans) {
                            cmsDoTransform(trans, scratch, _compUI[i]._map, 1024);
                            if (_compUI[i]._slider)
                            {
                                _compUI[i]._slider->setMap(_compUI[i]._map);
                            }
                        }
                    }
                }
            }
        }
    }

    guint32 start = _color.color().toRGBA32(0x00);
    guint32 mid = _color.color().toRGBA32(0x7f);
    guint32 end = _color.color().toRGBA32(0xff);

    _slider->setColors(start, mid, end);
}


void ColorICCSelectorImpl::_adjustmentChanged(Glib::RefPtr<Gtk::Adjustment> &adjustment)
{
#ifdef DEBUG_LCMS
    g_message("/^^^^^^^^^  %p::_adjustmentChanged()", this);
#endif // DEBUG_LCMS

    ColorICCSelector *iccSelector = _owner;
    if (iccSelector->_impl->_updating) {
        return;
    }

    iccSelector->_impl->_updating = TRUE;

    gint match = -1;

    SPColor newColor(iccSelector->_impl->_color.color());
    gfloat scaled = ColorScales::getScaled(iccSelector->_impl->_adj);
    if (iccSelector->_impl->_adj == adjustment) {
#ifdef DEBUG_LCMS
        g_message("ALPHA");
#endif // DEBUG_LCMS
    }
    else {
        for (size_t i = 0; i < iccSelector->_impl->_compUI.size(); i++) {
            if (iccSelector->_impl->_compUI[i]._adj == adjustment) {
                match = i;
                break;
            }
        }
        if (match >= 0) {
#ifdef DEBUG_LCMS
            g_message(" channel %d", match);
#endif // DEBUG_LCMS
        }


        cmsUInt16Number tmp[4];
        for (guint i = 0; i < 4; i++) {
            tmp[i] = ColorScales::getScaled(iccSelector->_impl->_compUI[i]._adj) * 0x0ffff;
        }
        guchar post[4] = { 0, 0, 0, 0 };

        cmsHTRANSFORM trans = iccSelector->_impl->_prof->getTransfToSRGB8();
        if (trans) {
            cmsDoTransform(trans, tmp, post, 1);
        }

        SPColor other(SP_RGBA32_U_COMPOSE(post[0], post[1], post[2], 255));
        other.icc = new SVGICCColor();
        if (iccSelector->_impl->_color.color().icc) {
            other.icc->colorProfile = iccSelector->_impl->_color.color().icc->colorProfile;
        }

        guint32 prior = iccSelector->_impl->_color.color().toRGBA32(255);
        guint32 newer = other.toRGBA32(255);

        if (prior != newer) {
#ifdef DEBUG_LCMS
            g_message("Transformed color from 0x%08x to 0x%08x", prior, newer);
            g_message("      ~~~~ FLIP");
#endif // DEBUG_LCMS
            newColor = other;
            newColor.icc->colors.clear();
            for (guint i = 0; i < iccSelector->_impl->_profChannelCount; i++) {
                gdouble val = ColorScales::getScaled(iccSelector->_impl->_compUI[i]._adj);
                val *= iccSelector->_impl->_compUI[i]._component.scale;
                if (iccSelector->_impl->_compUI[i]._component.scale == 256) {
                    val -= 128;
                }
                newColor.icc->colors.push_back(val);
            }
        }
    }
    iccSelector->_impl->_color.setColorAlpha(newColor, scaled);
    // iccSelector->_updateInternals( newColor, scaled, iccSelector->_impl->_dragging );
    iccSelector->_impl->_updateSliders(match);

    iccSelector->_impl->_updating = FALSE;
#ifdef DEBUG_LCMS
    g_message("\\_________  %p::_adjustmentChanged()", cs);
#endif // DEBUG_LCMS
}

void ColorICCSelectorImpl::_sliderGrabbed()
{
    //    ColorICCSelector* iccSelector = dynamic_cast<ColorICCSelector*>(SP_COLOR_SELECTOR(cs)->base);
    //     if (!iccSelector->_dragging) {
    //         iccSelector->_dragging = TRUE;
    //         iccSelector->_grabbed();
    //         iccSelector->_updateInternals( iccSelector->_color, ColorScales::getScaled( iccSelector->_impl->_adj ),
    //         iccSelector->_dragging );
    //     }
}

void ColorICCSelectorImpl::_sliderReleased()
{
    //     ColorICCSelector* iccSelector = dynamic_cast<ColorICCSelector*>(SP_COLOR_SELECTOR(cs)->base);
    //     if (iccSelector->_dragging) {
    //         iccSelector->_dragging = FALSE;
    //         iccSelector->_released();
    //         iccSelector->_updateInternals( iccSelector->_color, ColorScales::getScaled( iccSelector->_adj ),
    //         iccSelector->_dragging );
    //     }
}

#ifdef DEBUG_LCMS
void ColorICCSelectorImpl::_sliderChanged(SPColorSlider *slider, SPColorICCSelector *cs)
#else
void ColorICCSelectorImpl::_sliderChanged()
#endif // DEBUG_LCMS
{
#ifdef DEBUG_LCMS
    g_message("Changed  %p and %p", slider, cs);
#endif // DEBUG_LCMS
    //     ColorICCSelector* iccSelector = dynamic_cast<ColorICCSelector*>(SP_COLOR_SELECTOR(cs)->base);

    //     iccSelector->_updateInternals( iccSelector->_color, ColorScales::getScaled( iccSelector->_adj ),
    //     iccSelector->_dragging );
}

Gtk::Widget *ColorICCSelectorFactory::createWidget(Inkscape::UI::SelectedColor &color) const
{
    Gtk::Widget *w = Gtk::manage(new ColorICCSelector(color));
    return w;
}

Glib::ustring ColorICCSelectorFactory::modeName() const { return gettext(ColorICCSelector::MODE_NAME); }
}
}
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
