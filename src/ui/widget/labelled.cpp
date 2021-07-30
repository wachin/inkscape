// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Carl Hetherington <inkscape@carlh.net>
 *   Derek P. Moore <derekm@hackunix.org>
 *
 * Copyright (C) 2004 Carl Hetherington
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "labelled.h"
#include "ui/icon-loader.h"
#include <gtkmm/image.h>
#include <gtkmm/label.h>

namespace Inkscape {
namespace UI {
namespace Widget {

Labelled::Labelled(Glib::ustring const &label, Glib::ustring const &tooltip,
                   Gtk::Widget *widget,
                   Glib::ustring const &suffix,
                   Glib::ustring const &icon,
                   bool mnemonic)
    : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL),
      _widget(widget),
      _label(new Gtk::Label(label, Gtk::ALIGN_START, Gtk::ALIGN_CENTER, mnemonic)),
      _suffix(nullptr)
{
    g_assert(g_utf8_validate(icon.c_str(), -1, nullptr));
    if (icon != "") {
        _icon = Gtk::manage(sp_get_icon_image(icon, Gtk::ICON_SIZE_LARGE_TOOLBAR));
        pack_start(*_icon, Gtk::PACK_SHRINK);
    }

    set_spacing(6);
    // Setting margins separately allows for more control over them
    set_margin_start(6);
    set_margin_end(6);
    pack_start(*Gtk::manage(_label), Gtk::PACK_SHRINK);
    pack_start(*Gtk::manage(_widget), Gtk::PACK_SHRINK);
    if (mnemonic) {
        _label->set_mnemonic_widget(*_widget);
    }
    widget->set_tooltip_text(tooltip);
}


void Labelled::setWidgetSizeRequest(int width, int height)
{
    if (_widget)
        _widget->set_size_request(width, height);


}

Gtk::Label const *
Labelled::getLabel() const
{
    return _label;
}

void
Labelled::setLabelText(const Glib::ustring &str)
{
    _label->set_text(str);
}

void
Labelled::setTooltipText(const Glib::ustring &tooltip)
{
    _label->set_tooltip_text(tooltip);
    _widget->set_tooltip_text(tooltip);
}

bool Labelled::on_mnemonic_activate ( bool group_cycling )
{
    return _widget->mnemonic_activate ( group_cycling );
}

void
Labelled::set_hexpand(bool expand)
{
    // should only have 2 children, but second child may not be _widget
    child_property_pack_type(*get_children().back()) = expand ? Gtk::PACK_END
                                                              : Gtk::PACK_START;
    
    Gtk::Box::set_hexpand(expand);
}

} // namespace Widget
} // namespace UI
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
