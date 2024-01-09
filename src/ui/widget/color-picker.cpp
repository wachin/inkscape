// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Abhishek Sharma
 *
 * Copyright (C) Authors 2000-2005
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "color-picker.h"

#include "inkscape.h"
#include "desktop.h"
#include "document.h"
#include "document-undo.h"

#include "ui/dialog-events.h"
#include "ui/widget/color-notebook.h"


static bool _in_use = false;

namespace Inkscape {
namespace UI {
namespace Widget {

ColorPicker::ColorPicker (const Glib::ustring& title, const Glib::ustring& tip,
                          guint32 rgba, bool undo, Gtk::Button* external_button)
    : _preview(new ColorPreview(rgba))
    , _title(title)
    , _rgba(rgba)
    , _undo(undo)
    , _colorSelectorDialog("dialogs.colorpickerwindow")
{
    Gtk::Button* button = external_button ? external_button : this;
    _color_selector = nullptr;
    setupDialog(title);
    _preview->show();
    button->add(*Gtk::manage(_preview));
    // set tooltip if given, otherwise leave original tooltip in place (from external button)
    if (!tip.empty()) {
        button->set_tooltip_text(tip);
    }
    _selected_color.signal_changed.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));
    _selected_color.signal_dragged.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));
    _selected_color.signal_released.connect(sigc::mem_fun(*this, &ColorPicker::_onSelectedColorChanged));

    if (external_button) {
        external_button->signal_clicked().connect([=](){ on_clicked(); });
    }
}

ColorPicker::~ColorPicker()
{
    closeWindow();
}

void ColorPicker::setupDialog(const Glib::ustring &title)
{
    _colorSelectorDialog.hide();
    _colorSelectorDialog.set_title (title);
    _colorSelectorDialog.set_border_width (4);
}

void ColorPicker::setSensitive(bool sensitive) { set_sensitive(sensitive); }

void ColorPicker::setRgba32 (guint32 rgba)
{
    if (_in_use) return;

    set_preview(rgba);
    _rgba = rgba;
    if (_color_selector)
    {
        _updating = true;
        _selected_color.setValue(rgba);
        _updating = false;
    }
}

void ColorPicker::closeWindow()
{
    _colorSelectorDialog.hide();
}

void ColorPicker::open() {
    on_clicked();
}

void ColorPicker::on_clicked()
{
    if (!_color_selector) {
        auto selector = Gtk::manage(new ColorNotebook(_selected_color, _ignore_transparency));
        selector->set_label(_title);
        _color_selector = selector;
        _colorSelectorDialog.get_content_area()->pack_start(*_color_selector, true, true, 0);
        _color_selector->show();
    }

    _updating = true;
    _selected_color.setValue(_rgba);
    _updating = false;

    // This can cause the window to fire refresh events
    sp_transientize(GTK_WIDGET(_colorSelectorDialog.gobj()));

    _colorSelectorDialog.show();
    Glib::RefPtr<Gdk::Window> window = _colorSelectorDialog.get_parent_window();
    if (window) {
        window->focus(1);
    }
}

void ColorPicker::on_changed (guint32)
{
}

void ColorPicker::_onSelectedColorChanged() {
    if (_updating) {
        return;
    }

    if (_in_use) {
        return;
    } else {
        _in_use = true;
    }

    guint32 rgba = _selected_color.value();
    set_preview(rgba);

    if (_undo && SP_ACTIVE_DESKTOP) {
        DocumentUndo::done(SP_ACTIVE_DESKTOP->getDocument(), /* TODO: annotate */ "color-picker.cpp:129", "");
    }

    on_changed(rgba);
    _in_use = false;
    _rgba = rgba;
    _changed_signal.emit(rgba);
}

void ColorPicker::set_preview(guint32 rgba) {
    _preview->setRgba32(_ignore_transparency ? rgba | 0xff : rgba);
}

void ColorPicker::use_transparency(bool enable) {
    _ignore_transparency = !enable;
    set_preview(_rgba);
}

guint32 ColorPicker::get_current_color() const {
    return _rgba;
}

}//namespace Widget
}//namespace UI
}//namespace Inkscape


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
