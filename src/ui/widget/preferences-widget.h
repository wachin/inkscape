// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Widgets for Inkscape Preferences dialog.
 */
/*
 * Authors:
 *   Marco Scholten
 *   Bruno Dilly <bruno.dilly@gmail.com>
 *
 * Copyright (C) 2004, 2006, 2007  Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_INKSCAPE_PREFERENCES_H
#define INKSCAPE_UI_WIDGET_INKSCAPE_PREFERENCES_H

#include <iostream>
#include <vector>

#include <gtkmm/filechooserbutton.h>
#include "ui/widget/spinbutton.h"
#include <cstddef>
#include <sigc++/sigc++.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/grid.h>

#include "ui/widget/color-picker.h"
#include "ui/widget/unit-menu.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/scalar-unit.h"

namespace Gtk {
class Scale;
}

namespace Inkscape {
namespace UI {
namespace Widget {

class PrefCheckButton : public Gtk::CheckButton
{
public:
    void init(Glib::ustring const &label, Glib::ustring const &prefs_path,
              bool default_value);
    // Allow use with the GtkBuilder get_derived_widget
    PrefCheckButton(BaseObjectType *cobject, const Glib::RefPtr<Gtk::Builder> &refGlade, Glib::ustring pref, bool def)
        : Gtk::CheckButton(cobject)
    {
        init("", pref, def);
    }
    PrefCheckButton() : Gtk::CheckButton() {};
    sigc::signal<void (bool)> changed_signal;
protected:
    Glib::ustring _prefs_path;
    void on_toggled() override;
};

class PrefRadioButton : public Gtk::RadioButton
{
public:
    void init(Glib::ustring const &label, Glib::ustring const &prefs_path,
              int int_value, bool default_value, PrefRadioButton* group_member);
    void init(Glib::ustring const &label, Glib::ustring const &prefs_path,
              Glib::ustring const &string_value, bool default_value, PrefRadioButton* group_member);
    sigc::signal<void (bool)> changed_signal;
protected:
    Glib::ustring _prefs_path;
    Glib::ustring _string_value;
    int _value_type;
    enum
    {
        VAL_INT,
        VAL_STRING
    };
    int _int_value;
    void on_toggled() override;
};

struct PrefItem { Glib::ustring label; int int_value; Glib::ustring tooltip; bool is_default = false; };

class PrefRadioButtons : public Gtk::Box {
public:
    PrefRadioButtons(const std::vector<PrefItem>& buttons, const Glib::ustring& prefs_path);

private:
};

class PrefSpinButton : public SpinButton
{
public:
    void init(Glib::ustring const &prefs_path,
              double lower, double upper, double step_increment, double page_increment,
              double default_value, bool is_int, bool is_percent);
    sigc::signal<void (double)> changed_signal;
protected:
    Glib::ustring _prefs_path;
    bool _is_int;
    bool _is_percent;
    void on_value_changed() override;
};

class PrefSpinUnit : public ScalarUnit
{
public:
    PrefSpinUnit() : ScalarUnit("", "") {};

    void init(Glib::ustring const &prefs_path,
              double lower, double upper, double step_increment,
              double default_value,
              UnitType unit_type, Glib::ustring const &default_unit);
protected:
    Glib::ustring _prefs_path;
    bool _is_percent;
    void on_my_value_changed();
};

class ZoomCorrRuler : public Gtk::DrawingArea {
public:
    ZoomCorrRuler(int width = 100, int height = 20);
    void set_size(int x, int y);
    void set_unit_conversion(double conv) { _unitconv = conv; }

    int width() { return _min_width + _border*2; }

    static const double textsize;
    static const double textpadding;

private:
    bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

    void draw_marks(Cairo::RefPtr<Cairo::Context> cr, double dist, int major_interval);

    double _unitconv;
    int _min_width;
    int _height;
    int _border;
    int _drawing_width;
};

class ZoomCorrRulerSlider : public Gtk::Box
{
public:
    ZoomCorrRulerSlider() : Gtk::Box(Gtk::ORIENTATION_VERTICAL) {}

    void init(int ruler_width, int ruler_height, double lower, double upper,
              double step_increment, double page_increment, double default_value);

private:
    void on_slider_value_changed();
    void on_spinbutton_value_changed();
    void on_unit_changed();
    bool on_mnemonic_activate( bool group_cycling ) override;

    Inkscape::UI::Widget::SpinButton *_sb;
    UnitMenu        _unit;
    Gtk::Scale*      _slider;
    ZoomCorrRuler   _ruler;
    bool freeze; // used to block recursive updates of slider and spinbutton
};

class PrefSlider : public Gtk::Box
{
public:
    PrefSlider(bool spin = true) : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) { _spin = spin; }

    void init(Glib::ustring const &prefs_path,
    		  double lower, double upper, double step_increment, double page_increment, double default_value, int digits);

    Gtk::Scale*  getSlider() {return _slider;};
    Inkscape::UI::Widget::SpinButton * getSpinButton() {return _sb;};
private:
    void on_slider_value_changed();
    void on_spinbutton_value_changed();
    bool on_mnemonic_activate( bool group_cycling ) override;

    Glib::ustring _prefs_path;
    Inkscape::UI::Widget::SpinButton *_sb = nullptr;
    bool _spin;
    Gtk::Scale*     _slider = nullptr;

    bool freeze; // used to block recursive updates of slider and spinbutton
};


class PrefCombo : public Gtk::ComboBoxText
{
public:
    void init(Glib::ustring const &prefs_path,
              Glib::ustring const labels[], int const values[], int num_items, int default_value);

    /**
     * Initialize a combo box.
     * second form uses strings as key values.
     */
    void init(Glib::ustring const &prefs_path,
              Glib::ustring labels[], Glib::ustring values[], int num_items, Glib::ustring default_value);
    /**
     * Initialize a combo box.
     * with vectors.
     */
    void init(Glib::ustring const &prefs_path, std::vector<Glib::ustring> labels, std::vector<int> values,
              int default_value);

    void init(Glib::ustring const &prefs_path, std::vector<Glib::ustring> labels, std::vector<Glib::ustring> values,
              Glib::ustring default_value);

    /**
     * Initialize a combo box with a vector of Glib::ustring pairs.
     */
    void init(Glib::ustring const &prefs_path,
              std::vector<std::pair<Glib::ustring, Glib::ustring>> labels_and_values,
              Glib::ustring default_value);

  protected:
    Glib::ustring _prefs_path;
    std::vector<int> _values;
    std::vector<Glib::ustring> _ustr_values;    ///< string key values used optionally instead of numeric _values
    void on_changed() override;
};

class PrefEntry : public Gtk::Entry
{
public:
    void init(Glib::ustring const &prefs_path, bool mask);
protected:
    Glib::ustring _prefs_path;
    void on_changed() override;
};

class PrefEntryFile : public PrefEntry
{
    void on_changed() override;
};

class PrefMultiEntry : public Gtk::ScrolledWindow
{
public:
    void init(Glib::ustring const &prefs_path, int height);
protected:
    Glib::ustring       _prefs_path;
    Gtk::TextView       _text;
    void on_changed();
};

class PrefEntryButtonHBox : public Gtk::Box
{
public:
    PrefEntryButtonHBox() : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {}

    void init(Glib::ustring const &prefs_path,
            bool mask, Glib::ustring const &default_string);

protected:
    Glib::ustring _prefs_path;
    Glib::ustring _default_string;
    Gtk::Button *relatedButton;
    Gtk::Entry *relatedEntry;
    void onRelatedEntryChangedCallback();
    void onRelatedButtonClickedCallback();
    bool on_mnemonic_activate( bool group_cycling ) override;
};

class PrefEntryFileButtonHBox : public Gtk::Box
{
public:
    PrefEntryFileButtonHBox() : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {}

    void init(Glib::ustring const &prefs_path,
            bool mask);
protected:
    Glib::ustring _prefs_path;
    Gtk::Button *relatedButton;
    Gtk::Entry *relatedEntry;
    void onRelatedEntryChangedCallback();
    void onRelatedButtonClickedCallback();
    bool on_mnemonic_activate( bool group_cycling ) override;
};

class PrefOpenFolder : public Gtk::Box {
  public:
    PrefOpenFolder() : Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) {}

    void init(Glib::ustring const &entry_string, Glib::ustring const &tooltip);

  protected:
    Gtk::Button *relatedButton;
    Gtk::Entry *relatedEntry;
    void onRelatedButtonClickedCallback();
};

class PrefFileButton : public Gtk::FileChooserButton
{
public:
    void init(Glib::ustring const &prefs_path);

protected:
    Glib::ustring _prefs_path;
    void onFileChanged();
};

class PrefColorPicker : public ColorPicker
{
public:
    PrefColorPicker() : ColorPicker("", "", 0, false) {};
    ~PrefColorPicker() override = default;;

    void init(Glib::ustring const &abel, Glib::ustring const &prefs_path,
              guint32 default_rgba);

protected:
    Glib::ustring _prefs_path;
    void on_changed (guint32 rgba) override;
};

class PrefUnit : public UnitMenu
{
public:
    void init(Glib::ustring const &prefs_path);
protected:
    Glib::ustring _prefs_path;
    void on_changed() override;
};

class DialogPage : public Gtk::Grid
{
public:
    DialogPage();
    void add_line(bool indent, Glib::ustring const &label, Gtk::Widget& widget, Glib::ustring const &suffix, Glib::ustring const &tip, bool expand = true, Gtk::Widget *other_widget = nullptr);
    void add_group_header(Glib::ustring name, int columns = 1);
    void add_group_note(Glib::ustring name);
    void set_tip(Gtk::Widget &widget, Glib::ustring const &tip);
};


} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif //INKSCAPE_UI_WIDGET_INKSCAPE_PREFERENCES_H

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
