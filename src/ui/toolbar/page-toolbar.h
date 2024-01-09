// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_PAGE_TOOLBAR_H
#define SEEN_PAGE_TOOLBAR_H

/**
 * @file
 * Page toolbar
 */
/* Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm.h>
#include <gtkmm/spinbutton.h>

#include "toolbar.h"

#include "ui/widget/spinbutton.h"
#include "helper/auto-connection.h"

class SPDesktop;
class SPDocument;
class SPPage;

namespace Inkscape {
class PaperSize;
namespace UI {
namespace Tools {
class ToolBase;
}
namespace Toolbar {

class PageToolbar : public Gtk::Toolbar
{
public:
    PageToolbar(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &builder, SPDesktop *desktop);
    ~PageToolbar() override;

    static GtkWidget *create(SPDesktop *desktop);

protected:
    void labelEdited();
    void bleedsEdited();
    void marginsEdited();
    void marginTopEdited();
    void marginRightEdited();
    void marginBottomEdited();
    void marginLeftEdited();
    void marginSideEdited(int side, const Glib::ustring &value);
    void sizeChoose(const std::string &preset_key);
    void sizeChanged();
    void setSizeText(SPPage *page = nullptr, bool display_only = true);
    void setMarginText(SPPage *page = nullptr);

private:
    SPDesktop *_desktop;
    SPDocument *_document;

    void toolChanged(SPDesktop *desktop, Inkscape::UI::Tools::ToolBase *ec);
    void pagesChanged();
    void selectionChanged(SPPage *page);
    void on_parent_changed(Gtk::Widget *prev) override;
    void populate_sizes();

    Inkscape::auto_connection _ec_connection;
    Inkscape::auto_connection _doc_connection;
    Inkscape::auto_connection _pages_changed;
    Inkscape::auto_connection _page_selected;
    Inkscape::auto_connection _page_modified;
    Inkscape::auto_connection _label_edited;
    Inkscape::auto_connection _size_edited;

    bool was_referenced;
    Gtk::ComboBoxText *combo_page_sizes;
    Gtk::Entry *entry_page_sizes;
    Gtk::Entry *text_page_margins;
    Gtk::Entry *text_page_bleeds;
    Gtk::Entry *text_page_label;
    Gtk::Entry *text_page_width;
    Gtk::Entry *text_page_height;
    Gtk::Label *label_page_pos;
    Gtk::ToolButton *btn_page_backward;
    Gtk::ToolButton *btn_page_foreward;
    Gtk::ToolButton *btn_page_delete;
    Gtk::ToolButton *btn_move_toggle;
    Gtk::SeparatorToolItem *sep1;

    Glib::RefPtr<Gtk::ListStore> sizes_list;
    Glib::RefPtr<Gtk::ListStore> sizes_search;
    Glib::RefPtr<Gtk::EntryCompletion> sizes_searcher;

    Gtk::Popover *margin_popover;

    Inkscape::UI::Widget::MathSpinButton *margin_top;
    Inkscape::UI::Widget::MathSpinButton *margin_right;
    Inkscape::UI::Widget::MathSpinButton *margin_bottom;
    Inkscape::UI::Widget::MathSpinButton *margin_left;

    double _unit_to_size(std::string number, std::string unit_str, std::string backup);
};

} // namespace Toolbar
} // namespace UI
} // namespace Inkscape

#endif /* !SEEN_PAGE_TOOLBAR_H */

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
