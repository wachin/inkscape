// SPDX-License-Identifier: GPL-2.0-or-later
#include "color-item.h"

#include <cstdint>
#include <cairomm/cairomm.h>
#include <glibmm/convert.h>
#include <glibmm/i18n.h>
#include <gdkmm/general.h>

#include "helper/sigc-track-obj.h"
#include "inkscape-preferences.h"
#include "io/resource.h"
#include "io/sys.h"
#include "object/sp-gradient.h"
#include "svg/svg-color.h"
#include "hsluv.h"
#include "display/cairo-utils.h"
#include "desktop-style.h"
#include "actions/actions-tools.h"
#include "message-context.h"
#include "ui/dialog/dialog-base.h"
#include "ui/dialog/dialog-container.h"
#include "ui/icon-names.h"

namespace {

class Globals
{
    Globals()
    {
        load_removecolor();
        load_mimetargets();
    }

    void load_removecolor()
    {
        auto path_utf8 = (Glib::ustring)Inkscape::IO::Resource::get_path(Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::PIXMAPS, "remove-color.png");
        auto path = Glib::filename_from_utf8(path_utf8);
        auto pixbuf = Gdk::Pixbuf::create_from_file(path);
        if (!pixbuf) {
            g_warning("Null pixbuf for %p [%s]", path.c_str(), path.c_str());
        }
        removecolor = Gdk::Cairo::create_surface_from_pixbuf(pixbuf, 1);
    }

    void load_mimetargets()
    {
        auto &mimetypes = PaintDef::getMIMETypes();
        mimetargets.reserve(mimetypes.size());
        for (int i = 0; i < mimetypes.size(); i++) {
            mimetargets.emplace_back(mimetypes[i], (Gtk::TargetFlags)0, i);
        }
    }

public:
    static Globals &get()
    {
        static Globals instance;
        return instance;
    }

    // The "remove-color" image.
    Cairo::RefPtr<Cairo::ImageSurface> removecolor;

    // The MIME targets for drag and drop, in the format expected by GTK.
    std::vector<Gtk::TargetEntry> mimetargets;
};

} // namespace

namespace Inkscape {
namespace UI {
namespace Dialog {

ColorItem::ColorItem(PaintDef const &paintdef, DialogBase *dialog)
    : dialog(dialog)
{
    if (paintdef.get_type() == PaintDef::RGB) {
        pinned_default = false;
        data = RGBData{paintdef.get_rgb()};
    } else {
        pinned_default = true;
        data = NoneData{};
    }
    description = paintdef.get_description();
    color_id = paintdef.get_color_id();

    common_setup();
}

ColorItem::ColorItem(SPGradient *gradient, DialogBase *dialog)
    : dialog(dialog)
{
    data = GradientData{gradient};
    description = gradient->defaultLabel();
    color_id = gradient->getId();

    gradient->connectRelease(SIGC_TRACKING_ADAPTOR([this] (SPObject*) {
        boost::get<GradientData>(data).gradient = nullptr;
    }, *this));

    gradient->connectModified(SIGC_TRACKING_ADAPTOR([this] (SPObject *obj, unsigned flags) {
        if (flags & SP_OBJECT_STYLE_MODIFIED_FLAG) {
            cache_dirty = true;
            queue_draw();
        }
        description = obj->defaultLabel();
        _signal_modified.emit();
        if (is_pinned() != was_grad_pinned) {
            was_grad_pinned = is_pinned();
            _signal_pinned.emit();
        }
    }, *this));

    was_grad_pinned = is_pinned();
    common_setup();
}

void ColorItem::common_setup()
{
    set_name("ColorItem");
    set_tooltip_text(description);
    add_events(Gdk::ENTER_NOTIFY_MASK |
               Gdk::LEAVE_NOTIFY_MASK |
               Gdk::BUTTON_PRESS_MASK |
               Gdk::BUTTON_RELEASE_MASK);
    drag_source_set(Globals::get().mimetargets, Gdk::BUTTON1_MASK, Gdk::ACTION_MOVE | Gdk::ACTION_COPY);
}

void ColorItem::set_pinned_pref(const std::string &path)
{
    pinned_pref = path + "/pinned/" + color_id;
}

void ColorItem::draw_color(Cairo::RefPtr<Cairo::Context> const &cr, int w, int h) const
{
    if (boost::get<NoneData>(&data)) {
        if (auto surface = Globals::get().removecolor) {
            const auto device_scale = get_scale_factor();
            cr->save();
            cr->scale((double)w / surface->get_width() / device_scale, (double)h / surface->get_height() / device_scale);
            cr->set_source(surface, 0, 0);
            cr->paint();
            cr->restore();
        }
    } else if (auto rgbdata = boost::get<RGBData>(&data)) {
        auto [r, g, b] = rgbdata->rgb;
        cr->set_source_rgb(r / 255.0, g / 255.0, b / 255.0);
        cr->paint();
    } else if (auto graddata = boost::get<GradientData>(&data)) {
        // Gradient pointer may be null if the gradient was destroyed.
        auto grad = graddata->gradient;
        if (!grad) return;

        auto pat_checkerboard = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(ink_cairo_pattern_create_checkerboard(), true));
        auto pat_gradient     = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(grad->create_preview_pattern(w),         true));

        cr->set_source(pat_checkerboard);
        cr->paint();
        cr->set_source(pat_gradient);
        cr->paint();
    }
}

bool ColorItem::on_draw(Cairo::RefPtr<Cairo::Context> const &cr)
{
    auto w = get_width();
    auto h = get_height();

    // Only using caching for none and gradients. None is included because the image is huge.
    bool use_cache = boost::get<NoneData>(&data) || boost::get<GradientData>(&data);

    if (use_cache) {
        auto scale = get_scale_factor();
        // Ensure cache exists and has correct size.
        if (!cache || cache->get_width() != w * scale || cache->get_height() != h * scale) {
            cache = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w * scale, h * scale);
            cairo_surface_set_device_scale(cache->cobj(), scale, scale);
            cache_dirty = true;
        }
        // Ensure cache contents is up-to-date.
        if (cache_dirty) {
            draw_color(Cairo::Context::create(cache), w * scale, h * scale);
            cache_dirty = false;
        }
        // Paint from cache.
        cr->set_source(cache, 0, 0);
        cr->paint();
    } else {
        // Paint directly.
        draw_color(cr, w, h);
    }

    // Draw fill/stroke indicators.
    if (is_fill || is_stroke) {
        double const lightness = Hsluv::rgb_to_perceptual_lightness(average_color());
        auto [gray, alpha] = Hsluv::get_contrasting_color(lightness);
        cr->set_source_rgba(gray, gray, gray, alpha);

        // Scale so that the square -1...1 is the biggest possible square centred in the widget.
        auto minwh = std::min(w, h);
        cr->translate((w - minwh) / 2.0, (h - minwh) / 2.0);
        cr->scale(minwh / 2.0, minwh / 2.0);
        cr->translate(1.0, 1.0);

        if (is_fill) {
            cr->arc(0.0, 0.0, 0.35, 0.0, 2 * M_PI);
            cr->fill();
        }

        if (is_stroke) {
            cr->set_fill_rule(Cairo::FILL_RULE_EVEN_ODD);
            cr->arc(0.0, 0.0, 0.65, 0.0, 2 * M_PI);
            cr->arc(0.0, 0.0, 0.5, 0.0, 2 * M_PI);
            cr->fill();
        }
    }

    return true;
}

void ColorItem::on_size_allocate(Gtk::Allocation &allocation)
{
    Gtk::DrawingArea::on_size_allocate(allocation);
    cache_dirty = true;
}

bool ColorItem::on_enter_notify_event(GdkEventCrossing*)
{
    mouse_inside = true;
    if (auto desktop = dialog->getDesktop()) {
        auto msg = Glib::ustring::compose(_("Color: <b>%1</b>; <b>Click</b> to set fill, <b>Shift+click</b> to set stroke"), description);
        desktop->tipsMessageContext()->set(Inkscape::INFORMATION_MESSAGE, msg.c_str());
    }
    return false;
}

bool ColorItem::on_leave_notify_event(GdkEventCrossing*)
{
    mouse_inside = false;
    if (auto desktop = dialog->getDesktop()) {
        desktop->tipsMessageContext()->clear();
    }
    return false;
}

bool ColorItem::on_button_press_event(GdkEventButton *event)
{
    if (event->button == 3) {
        on_rightclick(event);
        return true;
    }
    // Return true necessary to avoid stealing the canvas focus.
    return true;
}

bool ColorItem::on_button_release_event(GdkEventButton* event)
{
    if (mouse_inside && (event->button == 1 || event->button == 2)) {
        bool stroke = event->button == 2 || (event->state & GDK_SHIFT_MASK);
        on_click(stroke);
        return true;
    }
    return false;
}

void ColorItem::on_click(bool stroke)
{
    auto desktop = dialog->getDesktop();
    if (!desktop) return;

    auto attr_name = stroke ? "stroke" : "fill";
    auto css = std::unique_ptr<SPCSSAttr, void(*)(SPCSSAttr*)>(sp_repr_css_attr_new(), [] (auto p) {sp_repr_css_attr_unref(p);});

    Glib::ustring descr;
    if (boost::get<NoneData>(&data)) {
        sp_repr_css_set_property(css.get(), attr_name, "none");
        descr = stroke ? _("Set stroke color to none") : _("Set fill color to none");
    } else if (auto rgbdata = boost::get<RGBData>(&data)) {
        auto [r, g, b] = rgbdata->rgb;
        uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | 0xff;
        char buf[64];
        sp_svg_write_color(buf, sizeof(buf), rgba);
        sp_repr_css_set_property(css.get(), attr_name, buf);
        descr = stroke ? _("Set stroke color from swatch") : _("Set fill color from swatch");
    } else if (auto graddata = boost::get<GradientData>(&data)) {
        auto grad = graddata->gradient;
        if (!grad) return;
        auto colorspec = "url(#" + Glib::ustring(grad->getId()) + ")";
        sp_repr_css_set_property(css.get(), attr_name, colorspec.c_str());
        descr = stroke ? _("Set stroke color from swatch") : _("Set fill color from swatch");
    }

    sp_desktop_set_style(desktop, css.get());

    DocumentUndo::done(desktop->getDocument(), descr.c_str(), INKSCAPE_ICON("swatches"));
}

void ColorItem::on_rightclick(GdkEventButton *event)
{
    auto menu_gobj = gtk_menu_new(); /* C */
    auto menu = Glib::wrap(GTK_MENU(menu_gobj)); /* C */

    auto additem = [&, this] (Glib::ustring const &name, sigc::slot<void()> slot) {
        auto item = Gtk::make_managed<Gtk::MenuItem>(name);
        menu->append(*item);
        item->signal_activate().connect(SIGC_TRACKING_ADAPTOR(slot, *this));
    };

    // TRANSLATORS: An item in context menu on a colour in the swatches
    additem(_("Set fill"), [this] { on_click(false); });
    additem(_("Set stroke"), [this] { on_click(true); });

    if (boost::get<GradientData>(&data)) {
        menu->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());

        additem(_("Delete"), [this] {
            auto grad = boost::get<GradientData>(data).gradient;
            if (!grad) return;

            grad->setSwatch(false);
            DocumentUndo::done(grad->document, _("Delete swatch"), INKSCAPE_ICON("color-gradient"));
        });

        additem(_("Edit..."), [this] {
            auto grad = boost::get<GradientData>(data).gradient;
            if (!grad) return;

            auto desktop = dialog->getDesktop();
            auto selection = desktop->getSelection();
            auto items = std::vector<SPItem*>(selection->items().begin(), selection->items().end());

            if (!items.empty()) {
                auto query = SPStyle(desktop->doc());
                int result = objects_query_fillstroke(items, &query, true);
                if (result == QUERY_STYLE_MULTIPLE_SAME || result == QUERY_STYLE_SINGLE) {
                    if (query.fill.isPaintserver()) {
                        if (cast<SPGradient>(query.getFillPaintServer()) == grad) {
                            desktop->getContainer()->new_dialog("FillStroke");
                            return;
                        }
                    }
                }
            }

            // Otherwise, invoke the gradient tool.
            set_active_tool(desktop, "Gradient");
        });
    }

    additem(is_pinned() ? _("Unpin Color") : _("Pin Color"), [this] {
        if (boost::get<GradientData>(&data)) {
            auto grad = boost::get<GradientData>(data).gradient;
            if (!grad) return;

            grad->setPinned(!is_pinned());
            DocumentUndo::done(grad->document, is_pinned() ? _("Pin swatch") : _("Unpin swatch"), INKSCAPE_ICON("color-gradient"));
        } else {
            Inkscape::Preferences::get()->setBool(pinned_pref, !is_pinned());
        }
    });

    Gtk::Menu *convert_submenu = nullptr;

    auto create_convert_submenu = [&] {
        menu->append(*Gtk::make_managed<Gtk::SeparatorMenuItem>());

        auto convert_item = Gtk::make_managed<Gtk::MenuItem>(_("Convert"));
        menu->append(*convert_item);

        convert_submenu = Gtk::make_managed<Gtk::Menu>();
        convert_item->set_submenu(*convert_submenu);
    };

    auto add_convert_subitem = [&, this] (Glib::ustring const &name, sigc::slot<void()> slot) {
        if (!convert_submenu) {
            create_convert_submenu();
        }

        auto item = Gtk::make_managed<Gtk::MenuItem>(name);
        convert_submenu->append(*item);
        item->signal_activate().connect(slot);
    };

    auto grads = dialog->getDesktop()->getDocument()->getResourceList("gradient");
    for (auto obj : grads) {
        auto grad = static_cast<SPGradient*>(obj);
        if (grad->hasStops() && !grad->isSwatch()) {
            add_convert_subitem(grad->getId(), [name = grad->getId(), this] {
                auto doc = dialog->getDesktop()->getDocument();
                auto grads = doc->getResourceList("gradient");
                for (auto obj : grads) {
                    auto grad = static_cast<SPGradient*>(obj);
                    if (grad->getId() == name) {
                        grad->setSwatch();
                        DocumentUndo::done(doc, _("Add gradient stop"), INKSCAPE_ICON("color-gradient"));
                    }
                }
            });
        }
    }

    menu->show_all();
    menu->popup_at_pointer(reinterpret_cast<GdkEvent*>(event));

    // Todo: All lines marked /* C */ in this function are required in order for the menu to
    // self-destruct after it has finished. Please replace upon discovery of a better method.
    g_object_ref_sink(menu_gobj); /* C */
    g_object_unref(menu_gobj); /* C */
}

PaintDef ColorItem::to_paintdef() const
{
    if (boost::get<NoneData>(&data)) {
        return PaintDef();
    } else if (auto rgbdata = boost::get<RGBData>(&data)) {
        return PaintDef(rgbdata->rgb, description);
    } else if (boost::get<GradientData>(&data)) {
        auto grad = boost::get<GradientData>(data).gradient;
        return PaintDef({0, 0, 0}, grad->getId());
    }

    // unreachable
    return {};
}

void ColorItem::on_drag_data_get(Glib::RefPtr<Gdk::DragContext> const &context, Gtk::SelectionData &selection_data, guint info, guint time)
{
    auto &mimetypes = PaintDef::getMIMETypes();
    if (info < 0 || info >= mimetypes.size()) {
        g_warning("ERROR: unknown value (%d)", info);
        return;
    }
    auto &key = mimetypes[info];

    auto def = to_paintdef();
    auto [vec, format] = def.getMIMEData(key);
    if (vec.empty()) return;

    selection_data.set(key, format, reinterpret_cast<guint8 const*>(vec.data()), vec.size());
}

void ColorItem::on_drag_begin(Glib::RefPtr<Gdk::DragContext> const &context)
{
    constexpr int w = 32;
    constexpr int h = 24;

    auto surface = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, w, h);
    draw_color(Cairo::Context::create(surface), w, h);

    context->set_icon(Gdk::Pixbuf::create(surface, 0, 0, w, h), 0, 0);
}

void ColorItem::set_fill(bool b)
{
    is_fill = b;
    queue_draw();
}

void ColorItem::set_stroke(bool b)
{
    is_stroke = b;
    queue_draw();
}

bool ColorItem::is_pinned() const
{
    if (boost::get<GradientData>(&data)) {
        auto grad = boost::get<GradientData>(data).gradient;
        if (!grad) {
            return false;
        }
        return grad->isPinned();
    } else {
        return Inkscape::Preferences::get()->getBool(pinned_pref, pinned_default);
    }
}

std::array<double, 3> ColorItem::average_color() const
{
    if (boost::get<NoneData>(&data)) {
        return {1.0, 1.0, 1.0};
    } else if (auto rgbdata = boost::get<RGBData>(&data)) {
        auto [r, g, b] = rgbdata->rgb;
        return {r / 255.0, g / 255.0, b / 255.0};
    } else if (auto graddata = boost::get<GradientData>(&data)) {
        auto grad = graddata->gradient;
        auto pat = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(grad->create_preview_pattern(1), true));
        auto img = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, 1, 1);
        auto cr = Cairo::Context::create(img);
        cr->set_source_rgb(196.0 / 255.0, 196.0 / 255.0, 196.0 / 255.0);
        cr->paint();
        cr->set_source(pat);
        cr->paint();
        cr.clear();
        auto rgb = img->get_data();
        return {rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0};
    }

    // unreachable
    return {1.0, 1.0, 1.0};
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape
