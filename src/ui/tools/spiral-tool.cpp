// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Spiral drawing context
 *
 * Authors:
 *   Mitsuru Oka
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 1999-2001 Lauris Kaplinski
 * Copyright (C) 2001-2002 Mitsuru Oka
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>
#include <string>

#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>

#include "spiral-tool.h"

#include "context-fns.h"
#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "document.h"
#include "message-context.h"
#include "selection.h"
#include "verbs.h"

#include "include/macros.h"

#include "object/sp-namedview.h"
#include "object/sp-spiral.h"

#include "ui/shape-editor.h"

#include "xml/node-event-vector.h"

using Inkscape::DocumentUndo;

namespace Inkscape {
namespace UI {
namespace Tools {

const std::string& SpiralTool::getPrefsPath() {
	return SpiralTool::prefsPath;
}

const std::string SpiralTool::prefsPath = "/tools/shapes/spiral";

SpiralTool::SpiralTool()
    : ToolBase("spiral.svg")
    , spiral(nullptr)
    , revo(3)
    , exp(1)
    , t0(0)
{
}

void SpiralTool::finish() {
    ungrabCanvasEvents();

    this->finishItem();
    this->sel_changed_connection.disconnect();

    ToolBase::finish();
}

SpiralTool::~SpiralTool() {
    this->enableGrDrag(false);

    this->sel_changed_connection.disconnect();

    delete this->shape_editor;
    this->shape_editor = nullptr;

    /* fixme: This is necessary because we do not grab */
    if (this->spiral) {
    	this->finishItem();
    }
}

/**
 * Callback that processes the "changed" signal on the selection;
 * destroys old and creates new knotholder.
 */
void SpiralTool::selection_changed(Inkscape::Selection *selection) {
    this->shape_editor->unset_item();
    this->shape_editor->set_item(selection->singleItem());
}

void SpiralTool::setup() {
    ToolBase::setup();

    sp_event_context_read(this, "expansion");
    sp_event_context_read(this, "revolution");
    sp_event_context_read(this, "t0");

    this->shape_editor = new ShapeEditor(this->desktop);

    SPItem *item = this->desktop->getSelection()->singleItem();
    if (item) {
        this->shape_editor->set_item(item);
    }

    Inkscape::Selection *selection = this->desktop->getSelection();
    this->sel_changed_connection.disconnect();

    this->sel_changed_connection = selection->connectChanged(sigc::mem_fun(this, &SpiralTool::selection_changed));

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/tools/shapes/selcue")) {
        this->enableSelectionCue();
    }

    if (prefs->getBool("/tools/shapes/gradientdrag")) {
        this->enableGrDrag();
    }
}

void SpiralTool::set(const Inkscape::Preferences::Entry& val) {
    Glib::ustring name = val.getEntryName();

    if (name == "expansion") {
        this->exp = CLAMP(val.getDouble(), 0.0, 1000.0);
    } else if (name == "revolution") {
        this->revo = CLAMP(val.getDouble(3.0), 0.05, 40.0);
    } else if (name == "t0") {
        this->t0 = CLAMP(val.getDouble(), 0.0, 0.999);
    }
}

bool SpiralTool::root_handler(GdkEvent* event) {
    static gboolean dragging;

    SPDesktop *desktop = this->desktop;
    Inkscape::Selection *selection = desktop->getSelection();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    this->tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

    gint ret = FALSE;

    switch (event->type) {
        case GDK_BUTTON_PRESS:
            if (event->button.button == 1) {
                dragging = TRUE;

                this->center = Inkscape::setup_for_drag_start(desktop, this, event);

                SnapManager &m = desktop->namedview->snap_manager;
                m.setup(desktop);
                m.freeSnapReturnByRef(this->center, Inkscape::SNAPSOURCE_NODE_HANDLE);
                m.unSetup();

                grabCanvasEvents();
                ret = TRUE;
            }
            break;

        case GDK_MOTION_NOTIFY:
            if (dragging && (event->motion.state & GDK_BUTTON1_MASK)) {
                if ( this->within_tolerance
                     && ( abs( (gint) event->motion.x - this->xp ) < this->tolerance )
                     && ( abs( (gint) event->motion.y - this->yp ) < this->tolerance ) ) {
                    break; // do not drag if we're within tolerance from origin
                }
                // Once the user has moved farther than tolerance from the original location
                // (indicating they intend to draw, not click), then always process the
                // motion notify coordinates as given (no snapping back to origin)
                this->within_tolerance = false;

                Geom::Point const motion_w(event->motion.x, event->motion.y);
                Geom::Point motion_dt(this->desktop->w2d(motion_w));

                SnapManager &m = desktop->namedview->snap_manager;
                m.setup(desktop, true, this->spiral);
                m.freeSnapReturnByRef(motion_dt, Inkscape::SNAPSOURCE_NODE_HANDLE);
                m.unSetup();

                this->drag(motion_dt, event->motion.state);

                gobble_motion_events(GDK_BUTTON1_MASK);

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
                dragging = FALSE;
                sp_event_context_discard_delayed_snap_event(this);

                if (!this->within_tolerance) {
                    // we've been dragging, finish the spiral
                    this->finishItem();
                } else if (this->item_to_select) {
                    // no dragging, select clicked item if any
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
            switch (get_latin_keyval(&event->key)) {
                case GDK_KEY_Alt_R:
                case GDK_KEY_Control_L:
                case GDK_KEY_Control_R:
                case GDK_KEY_Shift_L:
                case GDK_KEY_Shift_R:
                case GDK_KEY_Meta_L:  // Meta is when you press Shift+Alt (at least on my machine)
                case GDK_KEY_Meta_R:
                    sp_event_show_modifier_tip(this->defaultMessageContext(), event,
                                               _("<b>Ctrl</b>: snap angle"),
                                               nullptr,
                                               _("<b>Alt</b>: lock spiral radius"));
                    break;

                case GDK_KEY_x:
                case GDK_KEY_X:
                    if (MOD__ALT_ONLY(event)) {
                        desktop->setToolboxFocusTo ("spiral-revolutions");
                        ret = TRUE;
                    }
                    break;

                case GDK_KEY_Escape:
                	if (dragging) {
                		dragging = false;
                		sp_event_context_discard_delayed_snap_event(this);
                		// if drawing, cancel, otherwise pass it up for deselecting
                		this->cancel();
                		ret = TRUE;
                	}
                	break;

                case GDK_KEY_space:
                    if (dragging) {
                        ungrabCanvasEvents();
                        dragging = false;
                        sp_event_context_discard_delayed_snap_event(this);

                        if (!this->within_tolerance) {
                            // we've been dragging, finish the spiral
                            this->finish();
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

        case GDK_KEY_RELEASE:
            switch (get_latin_keyval(&event->key)) {
                case GDK_KEY_Alt_L:
                case GDK_KEY_Alt_R:
                case GDK_KEY_Control_L:
                case GDK_KEY_Control_R:
                case GDK_KEY_Shift_L:
                case GDK_KEY_Shift_R:
                case GDK_KEY_Meta_L:  // Meta is when you press Shift+Alt
                case GDK_KEY_Meta_R:
                    this->defaultMessageContext()->clear();
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

void SpiralTool::drag(Geom::Point const &p, guint state) {

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    int const snaps = prefs->getInt("/options/rotationsnapsperpi/value", 12);

    if (!this->spiral) {
        if (Inkscape::have_viable_layer(desktop, defaultMessageContext()) == false) {
            return;
        }

        // Create object
        Inkscape::XML::Document *xml_doc = this->desktop->doc()->getReprDoc();
        Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");
        repr->setAttribute("sodipodi:type", "spiral");

        // Set style
        sp_desktop_apply_style_tool(desktop, repr, "/tools/shapes/spiral", false);

        this->spiral = SP_SPIRAL(desktop->currentLayer()->appendChildRepr(repr));
        Inkscape::GC::release(repr);
        this->spiral->transform = SP_ITEM(desktop->currentLayer())->i2doc_affine().inverse();
        this->spiral->updateRepr();

        forced_redraws_start(5);
    }

    SnapManager &m = desktop->namedview->snap_manager;
    m.setup(desktop, true, this->spiral);
    Geom::Point pt2g = p;
    m.freeSnapReturnByRef(pt2g, Inkscape::SNAPSOURCE_NODE_HANDLE);
    m.unSetup();
    Geom::Point const p0 = desktop->dt2doc(this->center);
    Geom::Point const p1 = desktop->dt2doc(pt2g);

    Geom::Point const delta = p1 - p0;
    gdouble const rad = Geom::L2(delta);

    // Start angle calculated from end angle and number of revolutions.
    gdouble arg = Geom::atan2(delta) - 2.0*M_PI * spiral->revo;

    if (state & GDK_CONTROL_MASK) {
        /* Snap start angle */
        double snaps_radian = M_PI/snaps;
        arg = std::round(arg/snaps_radian) * snaps_radian;
    }

    /* Fixme: these parameters should be got from dialog box */
    this->spiral->setPosition(p0[Geom::X], p0[Geom::Y],
                           /*expansion*/ this->exp,
                           /*revolution*/ this->revo,
                           rad, arg,
                           /*t0*/ this->t0);

    /* status text */
    Inkscape::Util::Quantity q = Inkscape::Util::Quantity(rad, "px");
    Glib::ustring rads = q.string(desktop->namedview->display_units);
    this->message_context->setF(Inkscape::IMMEDIATE_MESSAGE,
                               _("<b>Spiral</b>: radius %s, angle %.2f&#176;; with <b>Ctrl</b> to snap angle"),
                                rads.c_str(), arg * 180/M_PI + 360*spiral->revo);
}

void SpiralTool::finishItem() {
    this->message_context->clear();

    if (this->spiral != nullptr) {
    	if (this->spiral->rad == 0) {
    		this->cancel(); // Don't allow the creating of zero sized spiral, for example when the start and and point snap to the snap grid point
    		return;
    	}

        spiral->set_shape();
        spiral->updateRepr(SP_OBJECT_WRITE_EXT);
        spiral->doWriteTransform(spiral->transform, nullptr, true);

        forced_redraws_stop();

        this->desktop->getSelection()->set(this->spiral);
        
        DocumentUndo::done(this->desktop->getDocument(), SP_VERB_CONTEXT_SPIRAL, _("Create spiral"));

        this->spiral = nullptr;
    }
}

void SpiralTool::cancel() {
    this->desktop->getSelection()->clear();
    ungrabCanvasEvents();

    if (this->spiral != nullptr) {
    	this->spiral->deleteObject();
    	this->spiral = nullptr;
    }

    this->within_tolerance = false;
    this->xp = 0;
    this->yp = 0;
    this->item_to_select = nullptr;

    forced_redraws_stop();

    DocumentUndo::cancel(this->desktop->getDocument());
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
