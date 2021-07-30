// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 3D box drawing context
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007      Maximilian Albert <Anhalter42@gmx.de>
 * Copyright (C) 2006      Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) 2000-2005 authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>


#include "context-fns.h"
#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "message-context.h"
#include "perspective-line.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "verbs.h"

#include "include/macros.h"

#include "object/box3d-side.h"
#include "object/box3d.h"
#include "object/sp-defs.h"
#include "object/sp-namedview.h"

#include "ui/shape-editor.h"
#include "ui/tools/box3d-tool.h"
#include "ui/widget/canvas.h"  // Forced redraw

#include "xml/node-event-vector.h"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Tools {

const std::string& Box3dTool::getPrefsPath() {
	return Box3dTool::prefsPath;
}

const std::string Box3dTool::prefsPath = "/tools/shapes/3dbox";

Box3dTool::Box3dTool()
    : ToolBase("box.svg")
    , _vpdrag(nullptr)
    , box3d(nullptr)
    , ctrl_dragged(false)
    , extruded(false)
{
}

void Box3dTool::finish() {
    ungrabCanvasEvents();
    this->finishItem();
    this->sel_changed_connection.disconnect();

    ToolBase::finish();
}


Box3dTool::~Box3dTool() {
    this->enableGrDrag(false);

    delete (this->_vpdrag);
    this->_vpdrag = nullptr;

    this->sel_changed_connection.disconnect();

    delete this->shape_editor;
    this->shape_editor = nullptr;

    /* fixme: This is necessary because we do not grab */
    if (this->box3d) {
        this->finishItem();
    }
}

/**
 * Callback that processes the "changed" signal on the selection;
 * destroys old and creates new knotholder.
 */
void Box3dTool::selection_changed(Inkscape::Selection* selection) {
    this->shape_editor->unset_item();
    this->shape_editor->set_item(selection->singleItem());

    if (selection->perspList().size() == 1) {
        // selecting a single box changes the current perspective
        this->desktop->doc()->setCurrentPersp3D(selection->perspList().front());
    }
}

/* Create a default perspective in document defs if none is present (which can happen, among other
 * circumstances, after 'vacuum defs' or when a pre-0.46 file is opened).
 */
static void sp_box3d_context_ensure_persp_in_defs(SPDocument *document) {
    SPDefs *defs = document->getDefs();

    bool has_persp = false;
    for (auto& child: defs->children) {
        if (SP_IS_PERSP3D(&child)) {
            has_persp = true;
            break;
        }
    }

    if (!has_persp) {
        document->setCurrentPersp3D(Persp3D::create_xml_element (document));
    }
}

void Box3dTool::setup() {
    ToolBase::setup();

    this->shape_editor = new ShapeEditor(this->desktop);

    SPItem *item = this->desktop->getSelection()->singleItem();
    if (item) {
        this->shape_editor->set_item(item);
    }

    this->sel_changed_connection.disconnect();
    this->sel_changed_connection = this->desktop->getSelection()->connectChanged(
    	sigc::mem_fun(this, &Box3dTool::selection_changed)
    );

    this->_vpdrag = new Box3D::VPDrag(this->desktop->getDocument());

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/tools/shapes/selcue")) {
        this->enableSelectionCue();
    }

    if (prefs->getBool("/tools/shapes/gradientdrag")) {
        this->enableGrDrag();
    }
}

bool Box3dTool::item_handler(SPItem* item, GdkEvent* event) {
    gint ret = FALSE;

    switch (event->type) {
    case GDK_BUTTON_PRESS:
        if ( event->button.button == 1) {
            Inkscape::setup_for_drag_start(desktop, this, event);
            //ret = TRUE;
        }
        break;
        // motion and release are always on root (why?)
    default:
        break;
    }

//    if (((ToolBaseClass *) sp_box3d_context_parent_class)->item_handler) {
//        ret = ((ToolBaseClass *) sp_box3d_context_parent_class)->item_handler(event_context, item, event);
//    }
    // CPPIFY: ret is always overwritten...
    ret = ToolBase::item_handler(item, event);

    return ret;
}

bool Box3dTool::root_handler(GdkEvent* event) {
    static bool dragging;

    SPDocument *document = desktop->getDocument();
    auto const y_dir = desktop->yaxisdir();
    Inkscape::Selection *selection = desktop->getSelection();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int const snaps = prefs->getInt("/options/rotationsnapsperpi/value", 12);

    Persp3D *cur_persp = document->getCurrentPersp3D();

    this->tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

    gint ret = FALSE;
    switch (event->type) {
    case GDK_BUTTON_PRESS:
        if ( event->button.button == 1) {
            Geom::Point const button_w(event->button.x, event->button.y);
            Geom::Point button_dt(desktop->w2d(button_w));

            // save drag origin
            this->xp = (gint) button_w[Geom::X];
            this->yp = (gint) button_w[Geom::Y];
            this->within_tolerance = true;

            // remember clicked box3d, *not* disregarding groups (since a 3D box is a group), honoring Alt
            this->item_to_select = sp_event_context_find_item (desktop, button_w, event->button.state & GDK_MOD1_MASK, event->button.state & GDK_CONTROL_MASK);

            dragging = true;

            SnapManager &m = desktop->namedview->snap_manager;
            m.setup(desktop, true, this->box3d);
            m.freeSnapReturnByRef(button_dt, Inkscape::SNAPSOURCE_NODE_HANDLE);
            m.unSetup();
            this->center = button_dt;

            this->drag_origin = button_dt;
            this->drag_ptB = button_dt;
            this->drag_ptC = button_dt;

            // This can happen after saving when the last remaining perspective was purged and must be recreated.
            if (!cur_persp) {
                sp_box3d_context_ensure_persp_in_defs(document);
                cur_persp = document->getCurrentPersp3D();
            }

            /* Projective preimages of clicked point under current perspective */
            this->drag_origin_proj = cur_persp->perspective_impl->tmat.preimage (button_dt, 0, Proj::Z);
            this->drag_ptB_proj = this->drag_origin_proj;
            this->drag_ptC_proj = this->drag_origin_proj;
            this->drag_ptC_proj.normalize();
            this->drag_ptC_proj[Proj::Z] = 0.25;

            grabCanvasEvents();
            ret = TRUE;
        }
        break;

    case GDK_MOTION_NOTIFY:
        if (dragging && ( event->motion.state & GDK_BUTTON1_MASK )) {
            if ( this->within_tolerance
                 && ( abs( (gint) event->motion.x - this->xp ) < this->tolerance )
                 && ( abs( (gint) event->motion.y - this->yp ) < this->tolerance ) ) {
                break; // do not drag if we're within tolerance from origin
            }
            // Once the user has moved farther than tolerance from the original location
            // (indicating they intend to draw, not click), then always process the
            // motion notify coordinates as given (no snapping back to origin)
            this->within_tolerance = false;

            Geom::Point const motion_w(event->motion.x,
                                       event->motion.y);
            Geom::Point motion_dt(desktop->w2d(motion_w));

            SnapManager &m = desktop->namedview->snap_manager;
            m.setup(desktop, true, this->box3d);
            m.freeSnapReturnByRef(motion_dt, Inkscape::SNAPSOURCE_NODE_HANDLE);
            this->ctrl_dragged  = event->motion.state & GDK_CONTROL_MASK;

            if ((event->motion.state & GDK_SHIFT_MASK) && !this->extruded && this->box3d) {
                // once shift is pressed, set this->extruded
                this->extruded = true;
            }

            if (!this->extruded) {
                this->drag_ptB = motion_dt;
                this->drag_ptC = motion_dt;

                this->drag_ptB_proj = cur_persp->perspective_impl->tmat.preimage (motion_dt, 0, Proj::Z);
                this->drag_ptC_proj = this->drag_ptB_proj;
                this->drag_ptC_proj.normalize();
                this->drag_ptC_proj[Proj::Z] = 0.25;
            } else {
                // Without Ctrl, motion of the extruded corner is constrained to the
                // perspective line from drag_ptB to vanishing point Y.
                if (!this->ctrl_dragged) {
                    /* snapping */
                    Box3D::PerspectiveLine pline (this->drag_ptB, Proj::Z, document->getCurrentPersp3D());
                    this->drag_ptC = pline.closest_to (motion_dt);

                    this->drag_ptB_proj.normalize();
                    this->drag_ptC_proj = cur_persp->perspective_impl->tmat.preimage (this->drag_ptC, this->drag_ptB_proj[Proj::X], Proj::X);
                } else {
                    this->drag_ptC = motion_dt;

                    this->drag_ptB_proj.normalize();
                    this->drag_ptC_proj = cur_persp->perspective_impl->tmat.preimage (motion_dt, this->drag_ptB_proj[Proj::X], Proj::X);
                }

                m.freeSnapReturnByRef(this->drag_ptC, Inkscape::SNAPSOURCE_NODE_HANDLE);
            }

            m.unSetup();

            this->drag(event->motion.state);

            ret = TRUE;
        } else if (!this->sp_event_context_knot_mouseover()) {
            SnapManager &m = desktop->namedview->snap_manager;
            m.setup(desktop);

            Geom::Point const motion_w(event->motion.x, event->motion.y);
            Geom::Point motion_dt(desktop->w2d(motion_w));
            m.preSnap(Inkscape::SnapCandidatePoint(motion_dt, Inkscape::SNAPSOURCE_NODE_HANDLE));
            m.unSetup();
        }
        break;

    case GDK_BUTTON_RELEASE:
        this->xp = this->yp = 0;

        if (event->button.button == 1) {
            dragging = false;
            sp_event_context_discard_delayed_snap_event(this);

            if (!this->within_tolerance) {
                // we've been dragging, finish the box
                this->finishItem();
            } else if (this->item_to_select) {
                // no dragging, select clicked box3d if any
                if (event->button.state & GDK_SHIFT_MASK) {
                    selection->toggle(this->item_to_select);
                } else {
                    selection->set(this->item_to_select);
                }
            } else {
                // click in an empty space
                selection->clear();
            }

            this->item_to_select = nullptr;
            ret = TRUE;
            ungrabCanvasEvents();
        }
        break;

    case GDK_KEY_PRESS:
        switch (get_latin_keyval (&event->key)) {
        case GDK_KEY_Up:
        case GDK_KEY_Down:
        case GDK_KEY_KP_Up:
        case GDK_KEY_KP_Down:
            // prevent the zoom field from activation
            if (!MOD__CTRL_ONLY(event))
                ret = TRUE;
            break;

        case GDK_KEY_bracketright:
            document->getCurrentPersp3D()->rotate_VP (Proj::X, 180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_bracketleft:
            document->getCurrentPersp3D()->rotate_VP (Proj::X, -180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_parenright:
            document->getCurrentPersp3D()->rotate_VP (Proj::Y, 180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_parenleft:
            document->getCurrentPersp3D()->rotate_VP (Proj::Y, -180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_braceright:
            document->getCurrentPersp3D()->rotate_VP (Proj::Z, 180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_braceleft:
            document->getCurrentPersp3D()->rotate_VP (Proj::Z, -180 / snaps * y_dir, MOD__ALT(event));
            DocumentUndo::done(document, SP_VERB_CONTEXT_3DBOX,
                             _("Change perspective (angle of PLs)"));
            ret = true;
            break;

        case GDK_KEY_g:
        case GDK_KEY_G:
            if (MOD__SHIFT_ONLY(event)) {
                desktop->selection->toGuides();
                ret = true;
            }
            break;

        case GDK_KEY_p:
        case GDK_KEY_P:
            if (MOD__SHIFT_ONLY(event)) {
                if (document->getCurrentPersp3D()) {
                    document->getCurrentPersp3D()->print_debugging_info();
                }
                ret = true;
            }
            break;

        case GDK_KEY_x:
        case GDK_KEY_X:
            if (MOD__ALT_ONLY(event)) {
                desktop->setToolboxFocusTo ("box3d-angle-x");
                ret = TRUE;
            }
            if (MOD__SHIFT_ONLY(event)) {
                Persp3D::toggle_VPs(selection->perspList(), Proj::X);
                this->_vpdrag->updateLines(); // FIXME: Shouldn't this be done automatically?
                ret = true;
            }
            break;

        case GDK_KEY_y:
        case GDK_KEY_Y:
            if (MOD__SHIFT_ONLY(event)) {
                Persp3D::toggle_VPs(selection->perspList(), Proj::Y);
                this->_vpdrag->updateLines(); // FIXME: Shouldn't this be done automatically?
                ret = true;
            }
            break;

        case GDK_KEY_z:
        case GDK_KEY_Z:
            if (MOD__SHIFT_ONLY(event)) {
                Persp3D::toggle_VPs(selection->perspList(), Proj::Z);
                this->_vpdrag->updateLines(); // FIXME: Shouldn't this be done automatically?
                ret = true;
            }
            break;

        case GDK_KEY_Escape:
            desktop->getSelection()->clear();
            //TODO: make dragging escapable by Esc
            break;

        case GDK_KEY_space:
            if (dragging) {
                ungrabCanvasEvents();
                dragging = false;
                sp_event_context_discard_delayed_snap_event(this);
                if (!this->within_tolerance) {
                    // we've been dragging, finish the box
                    this->finishItem();
                }
                // do not return true, so that space would work switching to selector
            }
            break;

        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
        case GDK_KEY_BackSpace:
            ret = this->deleteSelectedDrag(MOD__CTRL_ONLY(event));
            break;

        default:
            break;
        }
        break;

    default:
        break;
    }

    if (!ret) {
    	ret = ToolBase::root_handler(event);
    }

    return ret;
}

void Box3dTool::drag(guint /*state*/) {
    if (!this->box3d) {
        if (Inkscape::have_viable_layer(desktop, defaultMessageContext()) == false) {
            return;
        }

        // Create object
        SPBox3D *box3d = SPBox3D::createBox3D((SPItem*)desktop->currentLayer());

        // Set style
        desktop->applyCurrentOrToolStyle(box3d, "/tools/shapes/3dbox", false);
        
        this->box3d = box3d;

        // TODO: Incorporate this in box3d-side.cpp!
        for (int i = 0; i < 6; ++i) {
            Box3DSide *side = Box3DSide::createBox3DSide(box3d);
            
            guint desc = Box3D::int_to_face(i);

            Box3D::Axis plane = (Box3D::Axis) (desc & 0x7);
            plane = (Box3D::is_plane(plane) ? plane : Box3D::orth_plane_or_axis(plane));
            side->dir1 = Box3D::extract_first_axis_direction(plane);
            side->dir2 = Box3D::extract_second_axis_direction(plane);
            side->front_or_rear = (Box3D::FrontOrRear) (desc & 0x8);

            // Set style
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();

            Glib::ustring descr = "/desktop/";
            descr += side->axes_string();
            descr += "/style";

            Glib::ustring cur_style = prefs->getString(descr);    
    
            bool use_current = prefs->getBool("/tools/shapes/3dbox/usecurrent", false);

            if (use_current && !cur_style.empty()) {
                // use last used style 
                side->setAttribute("style", cur_style);
            } else {
                // use default style 
                Glib::ustring tool_path = Glib::ustring::compose("/tools/shapes/3dbox/%1",
                        side->axes_string());
                desktop->applyCurrentOrToolStyle (side, tool_path, false);
            }

            side->updateRepr(); // calls Box3DSide::write() and updates, e.g., the axes string description
        }

        this->box3d->set_z_orders();
        this->box3d->updateRepr();

        // TODO: It would be nice to show the VPs during dragging, but since there is no selection
        //       at this point (only after finishing the box), we must do this "manually"
        /* this._vpdrag->updateDraggers(); */

        forced_redraws_start(5);
    }

    g_assert(this->box3d);

    this->box3d->orig_corner0 = this->drag_origin_proj;
    this->box3d->orig_corner7 = this->drag_ptC_proj;

    this->box3d->check_for_swapped_coords();

    /* we need to call this from here (instead of from SPBox3D::position_set(), for example)
       because z-order setting must not interfere with display updates during undo/redo */
    this->box3d->set_z_orders ();

    this->box3d->position_set();

    // status text
    this->message_context->setF(Inkscape::NORMAL_MESSAGE, "%s", _("<b>3D Box</b>; with <b>Shift</b> to extrude along the Z axis"));
}

void Box3dTool::finishItem() {
    this->message_context->clear();
    this->ctrl_dragged = false;
    this->extruded = false;

    if (this->box3d != nullptr) {
        SPDocument *doc = this->desktop->getDocument();

        if (!doc || !doc->getCurrentPersp3D()) {
            return;
        }

        this->box3d->orig_corner0 = this->drag_origin_proj;
        this->box3d->orig_corner7 = this->drag_ptC_proj;

        this->box3d->updateRepr();

        this->box3d->relabel_corners();

        forced_redraws_stop();

        desktop->getSelection()->set(this->box3d);
        DocumentUndo::done(desktop->getDocument(), SP_VERB_CONTEXT_3DBOX,
                         _("Create 3D box"));

        this->box3d = nullptr;
    }
}

}
}
}

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
