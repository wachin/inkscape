// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @brief Arranges Objects into a Circle/Ellipse
 */
/* Authors:
 *   Declara Denis
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_POLAR_ARRANGE_TAB_H
#define INKSCAPE_UI_DIALOG_POLAR_ARRANGE_TAB_H

#include "ui/widget/scalar-unit.h"
#include "ui/widget/anchor-selector.h"
#include "ui/dialog/arrange-tab.h"

#include <gtkmm/radiobutton.h>
#include <gtkmm/radiobuttongroup.h>
#include <gtkmm/grid.h>

namespace Inkscape {
namespace UI {
namespace Dialog {

class ArrangeDialog;

/**
 * PolarArrangeTab is a Tab displayed in the Arrange dialog and contains
 * enables the user to arrange objects on a circular or elliptical shape
 */
class PolarArrangeTab : public ArrangeTab {
public:
	PolarArrangeTab(ArrangeDialog *parent_);
    ~PolarArrangeTab() override = default;;

    /**
     * Do the actual arrangement
     */
    void arrange() override;

    /**
     * Respond to selection change
     */
    void updateSelection();

    void on_anchor_radio_changed();
    void on_arrange_radio_changed();

private:
    PolarArrangeTab(PolarArrangeTab const &d) = delete; // no copy
    void operator=(PolarArrangeTab const &d) = delete; // no assign

    ArrangeDialog         *parent;

    Gtk::Label             anchorPointLabel;

    Gtk::RadioButtonGroup  anchorRadioGroup;
    Gtk::RadioButton       anchorBoundingBoxRadio;
    Gtk::RadioButton       anchorObjectPivotRadio;
    Inkscape::UI::Widget::AnchorSelector anchorSelector;

    Gtk::Label             arrangeOnLabel;

    Gtk::RadioButtonGroup  arrangeRadioGroup;
    Gtk::RadioButton       arrangeOnFirstCircleRadio;
    Gtk::RadioButton       arrangeOnLastCircleRadio;
    Gtk::RadioButton       arrangeOnParametersRadio;

    Gtk::Grid              parametersTable;

    Gtk::Label             centerLabel;
    Inkscape::UI::Widget::ScalarUnit centerY;
    Inkscape::UI::Widget::ScalarUnit centerX;

    Gtk::Label             radiusLabel;
    Inkscape::UI::Widget::ScalarUnit radiusY;
    Inkscape::UI::Widget::ScalarUnit radiusX;

    Gtk::Label             angleLabel;
    Inkscape::UI::Widget::ScalarUnit angleY;
    Inkscape::UI::Widget::ScalarUnit angleX;

    Gtk::CheckButton       rotateObjectsCheckBox;


};

} //namespace Dialog
} //namespace UI
} //namespace Inkscape

#endif /* INKSCAPE_UI_DIALOG_POLAR_ARRANGE_TAB_H */

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
