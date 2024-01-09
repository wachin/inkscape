// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SP_EXPORT_HELPER_H
#define SP_EXPORT_HELPER_H

#include "2geom/rect.h"
#include "preferences.h"
#include "ui/widget/scrollprotected.h"

class SPDocument;
class SPItem;
class SPPage;

namespace Inkscape {
    namespace Util {
        class Unit;
    }
    namespace Extension {
        class Output;
    }
namespace UI {
namespace Dialog {

#define EXPORT_COORD_PRECISION 3
#define SP_EXPORT_MIN_SIZE 1.0
#define DPI_BASE Inkscape::Util::Quantity::convert(1, "in", "px")

// Class for storing and manipulating extensions
class ExtensionList : public Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBoxText>
{
public:
    ExtensionList();
    ExtensionList(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade);
    ~ExtensionList() override;

    void setup();
    Glib::ustring getFileExtension();
    void setExtensionFromFilename(Glib::ustring const &filename);
    void removeExtension(Glib::ustring &filename);
    void createList();
    Gtk::MenuButton *getPrefButton() const { return _pref_button; }
    Inkscape::Extension::Output *getExtension();

private:
    void init();
    void on_changed() override;

    PrefObserver _watch_pref;
    std::map<std::string, Inkscape::Extension::Output *> ext_to_mod;

    sigc::connection _popover_signal;
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::MenuButton *_pref_button = nullptr;
    Gtk::Popover *_pref_popover = nullptr;
    Gtk::Viewport *_pref_holder = nullptr;
};

class ExportList : public Gtk::Grid
{
public:
    ExportList() = default;
    ExportList(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &)
        : Gtk::Grid(cobject)
    {
    }
    ~ExportList() override = default;

public:
    void setup();
    void append_row();
    void delete_row(Gtk::Widget *widget);
    Glib::ustring get_suffix(int row);
    Inkscape::Extension::Output *getExtension(int row);
    void removeExtension(Glib::ustring &filename);
    double get_dpi(int row);
    int get_rows() { return _num_rows; }

private:
    typedef Inkscape::UI::Widget::ScrollProtected<Gtk::SpinButton> SpinButton;
    Inkscape::Preferences *prefs = nullptr;
    double default_dpi = 96.00;

private:
    bool _initialised = false;
    int _num_rows = 0;
    int _suffix_col = 0;
    int _extension_col = 1;
    int _prefs_col = 2;
    int _dpi_col = 3;
    int _delete_col = 4;
};

} // namespace Dialog
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
