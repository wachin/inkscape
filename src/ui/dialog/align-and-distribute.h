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

#include <gtkmm/frame.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>

#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/frame.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <list>

#include "2geom/rect.h"
#include "helper/auto-connection.h"
#include "ui/dialog/dialog-base.h"
#include "ui/widget/frame.h"
#include "ui/widget/scrollprotected.h"

class SPItem;

namespace Inkscape {
namespace UI {
namespace Tools{
class NodeTool;
}
namespace Dialog {

class Action;

class AlignAndDistribute : public Gtk::Box
{
public:
    AlignAndDistribute(DialogBase* dlg);
    ~AlignAndDistribute() override;

    void desktopReplaced();
    void selectionChanged(Inkscape::Selection*);
    void toolChanged(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* ec);

    Gtk::Grid &align_table(){return _alignTable;}
    Gtk::Grid &distribute_table(){return _distributeTable;}
    Gtk::Grid &rearrange_table(){return _rearrangeTable;}
    Gtk::Grid &removeOverlap_table(){return _removeOverlapTable;}
    Gtk::Grid &nodes_table(){return _nodesTable;}

    void setMode(bool nodeEdit);

    Geom::OptRect randomize_bbox;

    SPDesktop* getDesktop();
protected:
    DialogBase* _parent;

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

    std::list<Action *> _actionList;
    UI::Widget::Frame _alignFrame, _distributeFrame, _rearrangeFrame, _removeOverlapFrame, _nodesFrame;
    Gtk::Grid _alignTable, _distributeTable, _rearrangeTable, _removeOverlapTable, _nodesTable;
    Gtk::Box _anchorBox;
    Gtk::Box _selgrpBox;
    Gtk::Box _alignBox;
    Gtk::Box _alignBoxNode;
    Gtk::Box _alignTableBox;
    Gtk::Box _distributeTableBox;
    Gtk::Box _rearrangeTableBox;
    Gtk::Box _removeOverlapTableBox;
    Gtk::Box _nodesTableBox;
    Gtk::Label _anchorLabel;
    Gtk::Label _anchorLabelNode;
    Gtk::ToggleButton _selgrp;
    Gtk::ToggleButton _oncanvas;
    Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBoxText> _combo;
    Gtk::Box _anchorBoxNode;
    Inkscape::UI::Widget::ScrollProtected<Gtk::ComboBoxText> _comboNode;

    Inkscape::auto_connection _tool_changed;
private:
    AlignAndDistribute(AlignAndDistribute const &d) = delete;
    AlignAndDistribute& operator=(AlignAndDistribute const &d) = delete;

    friend class Align;
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
    void setDesktop(SPDesktop *desktop) { _desktop = desktop; }

protected:
    SPDesktop *_desktop;
private :
    virtual void on_button_click(){}

    Glib::ustring _id;
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
        if (!_desktop) return;
        do_action(_desktop, _index);
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
