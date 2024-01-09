// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A notebook with RGB, CMYK, CMS, HSL, and Wheel pages
 *//*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Tomasz Boczkowski <penginsbacon@gmail.com> (c++-sification)
 *
 * Copyright (C) 2001-2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_COLOR_NOTEBOOK_H
#define SEEN_SP_COLOR_NOTEBOOK_H

#include <memory>
#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <boost/ptr_container/ptr_vector.hpp>
#include <gtkmm/grid.h>
#include <gtkmm/stack.h>
#include <gtkmm/stackswitcher.h>
#include <glib.h>

#include "color.h"
#include "color-rgba.h"
#include "preferences.h"
#include "ui/selected-color.h"
#include "ui/widget/icon-combobox.h"

namespace Inkscape {
namespace UI {
namespace Widget {

class ColorNotebook
    : public Gtk::Grid
{
public:
    ColorNotebook(SelectedColor &color, bool no_alpha = false);
    ~ColorNotebook() override;

    void set_label(const Glib::ustring& label);

protected:
    struct Page {
        Page(std::unique_ptr<Inkscape::UI::ColorSelectorFactory> selector_factory, const char* icon);

        std::unique_ptr<Inkscape::UI::ColorSelectorFactory> selector_factory;
        Glib::ustring icon_name;
    };

    void _initUI(bool no_alpha);
    void _addPage(Page &page, bool no_alpha, const Glib::ustring vpath);
    void setDocument(SPDocument *document);

    void _pickColor(ColorRGBA *color);
    static void _onPickerClicked(GtkWidget *widget, ColorNotebook *colorbook);
    virtual void _onSelectedColorChanged();
    int getPageIndex(const Glib::ustring &name);
    int getPageIndex(Gtk::Widget *widget);

    void _updateICCButtons();
    void _setCurrentPage(int i, bool sync_combo);

    Inkscape::UI::SelectedColor &_selected_color;
    gulong _entryId;
    Gtk::Stack* _book;
    Gtk::StackSwitcher* _switcher;
    Gtk::Box* _buttonbox;
    Gtk::Label* _label;
    GtkWidget *_rgbal; /* RGBA entry */
    GtkWidget *_box_outofgamut, *_box_colormanaged, *_box_toomuchink;
    GtkWidget *_btn_picker;
    GtkWidget *_p; /* Color preview */
    sigc::connection _onetimepick;
    IconComboBox* _combo = nullptr;

public:
    // By default, disallow copy constructor and assignment operator
    ColorNotebook(const ColorNotebook &obj) = delete;
    ColorNotebook &operator=(const ColorNotebook &obj) = delete;

    PrefObserver _observer;
    std::vector<PrefObserver> _visibility_observers;

    SPDocument *_document = nullptr;
    sigc::connection _doc_replaced_connection;
    sigc::connection _selection_connection;
    sigc::connection _icc_changed_connection;
};

}
}
}
#endif // SEEN_SP_COLOR_NOTEBOOK_H
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

