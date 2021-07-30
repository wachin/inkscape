// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Transform dialog
 */
/* Author:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *
 * Copyright (C) 2004, 2005 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_TRANSFORMATION_H
#define INKSCAPE_UI_DIALOG_TRANSFORMATION_H

#include <glibmm/i18n.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/notebook.h>
#include <gtkmm/radiobutton.h>

#include "ui/dialog/dialog-base.h"
#include "ui/widget/notebook-page.h"
#include "ui/widget/scalar-unit.h"

namespace Gtk {
class Button;
}

namespace Inkscape {
namespace UI {
namespace Dialog {


/**
 * Transformation dialog.
 *
 * The transformation dialog allows to modify Inkscape objects.
 * 5 transformation operations are currently possible: move, scale,
 * rotate, skew and matrix.
 */
class Transformation : public DialogBase
{

public:

    /**
     * Constructor for Transformation.
     *
     * This does the initialization
     * and layout of the dialog used for transforming SVG objects.  It
     * consists of 5 pages for the 5 operations it handles:
     * 'Move' allows x,y translation of SVG objects
     * 'Scale' allows linear resizing of SVG objects
     * 'Rotate' allows rotating SVG objects by a degree
     * 'Skew' allows skewing SVG objects
     * 'Matrix' allows applying a generic affine transform on SVG objects,
     *     with the user specifying the 6 degrees of freedom manually.
     *
     * The dialog is implemented as a Gtk::Notebook with five pages.
     * The pages are implemented using Inkscape's NotebookPage which
     * is used to help make sure all of Inkscape's notebooks follow
     * the same style.  We then populate the pages with our widgets,
     * we use the ScalarUnit class for this.
     */
    Transformation();

    /**
     * Cleanup
     */
    ~Transformation() override;

    /**
     * Factory method.  Create an instance of this class/interface
     */
    static Transformation &getInstance()
        { return *new Transformation(); }


    /**
     * Show the Move panel
     */
    void setPageMove()
        { presentPage(PAGE_MOVE);      }


    /**
     * Show the Scale panel
     */
    void setPageScale()
        { presentPage(PAGE_SCALE);     }


    /**
     * Show the Rotate panel
     */
    void setPageRotate()
        { presentPage(PAGE_ROTATE);    }

    /**
     * Show the Skew panel
     */
    void setPageSkew()
        { presentPage(PAGE_SKEW);      }

    /**
     * Show the Transform panel
     */
    void setPageTransform()
        { presentPage(PAGE_TRANSFORM); }


    int getCurrentPage()
        { return _notebook.get_current_page(); }

    enum PageType {
        PAGE_MOVE, PAGE_SCALE, PAGE_ROTATE, PAGE_SKEW, PAGE_TRANSFORM, PAGE_QTY
    };

    void desktopReplaced() override;
    void selectionChanged(Inkscape::Selection *selection) override;
    void selectionModified(Inkscape::Selection *selection, guint flags) override;
    void updateSelection(PageType page, Inkscape::Selection *selection);

protected:

    Gtk::Notebook     _notebook;

    UI::Widget::NotebookPage      _page_move;
    UI::Widget::NotebookPage      _page_scale;
    UI::Widget::NotebookPage      _page_rotate;
    UI::Widget::NotebookPage      _page_skew;
    UI::Widget::NotebookPage      _page_transform;

    UI::Widget::UnitMenu          _units_move;
    UI::Widget::UnitMenu          _units_scale;
    UI::Widget::UnitMenu          _units_rotate;
    UI::Widget::UnitMenu          _units_skew;
    UI::Widget::UnitMenu          _units_transform;

    UI::Widget::ScalarUnit        _scalar_move_horizontal;
    UI::Widget::ScalarUnit        _scalar_move_vertical;
    UI::Widget::ScalarUnit        _scalar_scale_horizontal;
    UI::Widget::ScalarUnit        _scalar_scale_vertical;
    UI::Widget::ScalarUnit        _scalar_rotate;
    UI::Widget::ScalarUnit        _scalar_skew_horizontal;
    UI::Widget::ScalarUnit        _scalar_skew_vertical;

    UI::Widget::Scalar            _scalar_transform_a;
    UI::Widget::Scalar            _scalar_transform_b;
    UI::Widget::Scalar            _scalar_transform_c;
    UI::Widget::Scalar            _scalar_transform_d;
    UI::Widget::ScalarUnit        _scalar_transform_e;
    UI::Widget::ScalarUnit        _scalar_transform_f;

    Gtk::RadioButton         _counterclockwise_rotate;
    Gtk::RadioButton         _clockwise_rotate;

    Gtk::CheckButton  _check_move_relative;
    Gtk::CheckButton  _check_scale_proportional;
    Gtk::CheckButton  _check_apply_separately;
    Gtk::CheckButton  _check_replace_matrix;

    /**
     * Layout the GUI components, and prepare for use
     */
    void layoutPageMove();
    void layoutPageScale();
    void layoutPageRotate();
    void layoutPageSkew();
    void layoutPageTransform();

    void _apply();
    void presentPage(PageType page);

    void onSwitchPage(Gtk::Widget *page, guint pagenum);

    /**
     * Callbacks for when a user changes values on the panels
     */
    void onMoveValueChanged();
    void onMoveRelativeToggled();
    void onScaleXValueChanged();
    void onScaleYValueChanged();
    void onRotateValueChanged();
    void onRotateCounterclockwiseClicked();
    void onRotateClockwiseClicked();
    void onSkewValueChanged();
    void onTransformValueChanged();
    void onReplaceMatrixToggled();
    void onScaleProportionalToggled();

    void onClear();

    void onApplySeparatelyToggled();

    /**
     * Called when the selection is updated, to make
     * the panel(s) show the new values.
     * Editor---->dialog
     */
    void updatePageMove(Inkscape::Selection *);
    void updatePageScale(Inkscape::Selection *);
    void updatePageRotate(Inkscape::Selection *);
    void updatePageSkew(Inkscape::Selection *);
    void updatePageTransform(Inkscape::Selection *);

    /**
     * Called when the Apply button is pushed
     * Dialog---->editor
     */
    void applyPageMove(Inkscape::Selection *);
    void applyPageScale(Inkscape::Selection *);
    void applyPageRotate(Inkscape::Selection *);
    void applyPageSkew(Inkscape::Selection *);
    void applyPageTransform(Inkscape::Selection *);

private:

    /**
     * Copy constructor
     */
    Transformation(Transformation const &d) = delete;

    /**
     * Assignment operator
     */
    Transformation operator=(Transformation const &d) = delete;

    Gtk::Button *applyButton;
    Gtk::Button *resetButton;

    sigc::connection _selChangeConn;
    sigc::connection _selModifyConn;
    sigc::connection _tabSwitchConn;
};




} // namespace Dialog
} // namespace UI
} // namespace Inkscape



#endif //INKSCAPE_UI_DIALOG_TRANSFORMATION_H




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
