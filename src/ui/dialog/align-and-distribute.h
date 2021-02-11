// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Align and Distribute dialog
 */
/* Authors:
 *   Bryce W. Harrington <bryce@bryceharrington.org>
 *   Aubanel MONNIER <aubi@libertysurf.fr>
 *   Frank Felfe <innerspace@iname.com>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2004, 2005 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H
#define INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H

#include <list>
#include "ui/widget/panel.h"
#include "ui/widget/frame.h"

#include <gtkmm/frame.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/grid.h>

#include "2geom/rect.h"
#include "ui/dialog/desktop-tracker.h"

class SPItem;

namespace Inkscape {
namespace UI {
namespace Tools{
class NodeTool;
}
namespace Dialog {

class Action;


class AlignAndDistribute : public Widget::Panel {
public:
    AlignAndDistribute();
    ~AlignAndDistribute() override;

    static AlignAndDistribute &getInstance() { return *new AlignAndDistribute(); }

    Gtk::Grid &align_table(){return _alignTable;}
    Gtk::Grid &distribute_table(){return _distributeTable;}
    Gtk::Grid &rearrange_table(){return _rearrangeTable;}
    Gtk::Grid &removeOverlap_table(){return _removeOverlapTable;}
    Gtk::Grid &nodes_table(){return _nodesTable;}

    void setMode(bool nodeEdit);

    Geom::OptRect randomize_bbox;

protected:

    void on_ref_change();
    void on_node_ref_change();
    void on_selgrp_toggled();
    void on_oncanvas_toggled();
    void addDistributeButton(const Glib::ustring &id, const Glib::ustring tiptext, 
                                      guint row, guint col, bool onInterSpace, 
                                      Geom::Dim2 orientation, float kBegin, float kEnd);
    void addAlignButton(const Glib::ustring &id, const Glib::ustring tiptext, 
                        guint row, guint col);
    void addNodeButton(const Glib::ustring &id, const Glib::ustring tiptext, 
                        guint col, Geom::Dim2 orientation, bool distribute);
    void addRemoveOverlapsButton(const Glib::ustring &id,
                        const Glib::ustring tiptext,
                        guint row, guint col);
    void addGraphLayoutButton(const Glib::ustring &id,
                        const Glib::ustring tiptext,
                        guint row, guint col);
    void addExchangePositionsButton(const Glib::ustring &id,
                        const Glib::ustring tiptext,
                        guint row, guint col);
    void addExchangePositionsByZOrderButton(const Glib::ustring &id,
                        const Glib::ustring tiptext,
                        guint row, guint col);
    void addExchangePositionsClockwiseButton(const Glib::ustring &id,
                        const Glib::ustring tiptext,
                        guint row, guint col);
    void addUnclumpButton(const Glib::ustring &id, const Glib::ustring tiptext, 
                        guint row, guint col);
    void addRandomizeButton(const Glib::ustring &id, const Glib::ustring tiptext, 
                        guint row, guint col);
    void addBaselineButton(const Glib::ustring &id, const Glib::ustring tiptext,
                           guint row, guint col, Gtk::Grid &table, Geom::Dim2 orientation, bool distribute);
    void setTargetDesktop(SPDesktop *desktop);

    std::list<Action *> _actionList;
    UI::Widget::Frame _alignFrame, _distributeFrame, _rearrangeFrame, _removeOverlapFrame, _nodesFrame;
    Gtk::Grid _alignTable, _distributeTable, _rearrangeTable, _removeOverlapTable, _nodesTable;
    Gtk::HBox _anchorBox;
    Gtk::HBox _selgrpBox;
    Gtk::VBox _alignBox;
    Gtk::VBox _alignBoxNode;
    Gtk::HBox _alignTableBox;
    Gtk::HBox _distributeTableBox;
    Gtk::HBox _rearrangeTableBox;
    Gtk::HBox _removeOverlapTableBox;
    Gtk::HBox _nodesTableBox;
    Gtk::Label _anchorLabel;
    Gtk::Label _anchorLabelNode;
    Gtk::ToggleButton _selgrp;
    Gtk::ToggleButton _oncanvas;
    Gtk::ComboBoxText _combo;
    Gtk::HBox _anchorBoxNode;
    Gtk::ComboBoxText _comboNode;

    SPDesktop *_desktop;
    DesktopTracker _deskTrack;
    sigc::connection _desktopChangeConn;
    sigc::connection _toolChangeConn;
    sigc::connection _selChangeConn;
private:
    AlignAndDistribute(AlignAndDistribute const &d) = delete;
    AlignAndDistribute& operator=(AlignAndDistribute const &d) = delete;
};


struct BBoxSort
{
    SPItem *item;
    float anchor;
    Geom::Rect bbox;
    BBoxSort(SPItem *pItem, Geom::Rect const &bounds, Geom::Dim2 orientation, double kBegin, double kEnd);
    BBoxSort(const BBoxSort &rhs);
};
bool operator< (const BBoxSort &a, const BBoxSort &b);


class Action {
public :

    enum AlignTarget { LAST=0, FIRST, BIGGEST, SMALLEST, PAGE, DRAWING, SELECTION };
    enum AlignTargetNode { LAST_NODE=0, FIRST_NODE, MID_NODE, MIN_NODE, MAX_NODE };
    Action(Glib::ustring id,
           const Glib::ustring &tiptext,
           guint row, guint column,
	   Gtk::Grid &parent,
           AlignAndDistribute &dialog);

    virtual ~Action()= default;

    AlignAndDistribute &_dialog;

private :
    virtual void on_button_click(){}

    Glib::ustring _id;
    Gtk::Grid &_parent;
};


class ActionAlign : public Action {
public :
    struct Coeffs {
       double mx0, mx1, my0, my1;
       double sx0, sx1, sy0, sy1;
       int verb_id;
    };
    ActionAlign(const Glib::ustring &id,
                const Glib::ustring &tiptext,
                guint row, guint column,
                AlignAndDistribute &dialog,
                guint coeffIndex):
        Action(id, tiptext, row, column,
               dialog.align_table(), dialog),
        _index(coeffIndex),
        _dialog(dialog)
    {}

    /*
     * Static function called to align from a keyboard shortcut
     */
    static void do_verb_action(SPDesktop *desktop, int verb);
    static int verb_to_coeff(int verb);

private :


    void on_button_click() override {
        //Retrieve selected objects
        SPDesktop *desktop = _dialog.getDesktop();
        if (!desktop) return;

        do_action(desktop, _index);
    }

    static void do_action(SPDesktop *desktop, int index);
    static void do_node_action(Inkscape::UI::Tools::NodeTool *nt, int index);

    guint _index;
    AlignAndDistribute &_dialog;

    static const Coeffs _allCoeffs[19];

};


} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H

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
