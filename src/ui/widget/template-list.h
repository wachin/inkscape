// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef WIDGET_TEMPLATE_LIST_H
#define WIDGET_TEMPLATE_LIST_H

#include <gtkmm.h>
#include "extension/template.h"

class SPDocument;

namespace Inkscape {
namespace UI {
namespace Widget {

class TemplateList : public Gtk::Notebook
{
public:
    TemplateList();
    TemplateList(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade);
    ~TemplateList() override{};

    void init(Extension::TemplateShow mode);
    void reset_selection();
    bool has_selected_preset();
    std::shared_ptr<Extension::TemplatePreset> get_selected_preset();
    SPDocument *new_document();

    sigc::connection connectItemSelected(const sigc::slot<void ()> &slot) { return _item_selected_signal.connect(slot); }
    sigc::connection connectItemActivated(const sigc::slot<void ()> &slot) { return _item_activated_signal.connect(slot); }

private:
    Glib::RefPtr<Gtk::ListStore> generate_category(std::string label);
    Glib::RefPtr<Gdk::Pixbuf> icon_to_pixbuf(std::string name);
    Gtk::IconView *get_iconview(Gtk::Widget *widget);
    std::shared_ptr<Extension::TemplatePreset> get_preset(std::string key);

    sigc::signal<void ()> _item_selected_signal;
    sigc::signal<void ()> _item_activated_signal;
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape
#endif

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
