// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glibmm/i18n.h>

#include "context-fns.h"

#include "desktop.h"
#include "layer-manager.h"
#include "message-context.h"
#include "message-stack.h"
#include "snap.h"

#include "display/control/snap-indicator.h"

#include "object/sp-namedview.h"
#include "object/sp-item-group.h"

#include "ui/modifiers.h"
#include "ui/tools/tool-base.h"

static const double midpt_1_goldenratio = (1 + goldenratio) / 2;
static const double midpt_goldenratio_2 = (goldenratio + 2) / 2;

/* FIXME: could probably use a template here */

/**
 *  Check to see if the current layer is both unhidden and unlocked.  If not,
 *  set a message about it on the given context.
 *
 *  \param desktop Desktop.
 *  \param message Message context to put messages on.
 *  \return true if the current layer is both unhidden and unlocked, otherwise false.
 */

bool Inkscape::have_viable_layer(SPDesktop *desktop, MessageContext *message)
{
    auto layer = desktop->layerManager().currentLayer();

    if ( !layer || desktop->itemIsHidden(layer) ) {
            message->flash(Inkscape::ERROR_MESSAGE,
                         _("<b>Current layer is hidden</b>. Unhide it to be able to draw on it."));
            return false;
    }

    if ( layer->isLocked() ) {
            message->flash(Inkscape::ERROR_MESSAGE,
                         _("<b>Current layer is locked</b>. Unlock it to be able to draw on it."));
            return false;
    }

    return true;
}


/**
 *  Check to see if the current layer is both unhidden and unlocked.  If not,
 *  set a message about it on the given context.
 *
 *  \param desktop Desktop.
 *  \param message Message context to put messages on.
 *  \return true if the current layer is both unhidden and unlocked, otherwise false.
 */

bool Inkscape::have_viable_layer(SPDesktop *desktop, MessageStack *message)
{
    auto layer = desktop->layerManager().currentLayer();

    if ( !layer || desktop->itemIsHidden(layer) ) {
            message->flash(Inkscape::WARNING_MESSAGE,
                         _("<b>Current layer is hidden</b>. Unhide it to be able to draw on it."));
            return false;
    }

    if ( layer->isLocked() ) {
            message->flash(Inkscape::WARNING_MESSAGE,
                         _("<b>Current layer is locked</b>. Unlock it to be able to draw on it."));
            return false;
    }

    return true;
}


Geom::Rect Inkscape::snap_rectangular_box(SPDesktop const *desktop, SPItem *item,
                                        Geom::Point const &pt, Geom::Point const &center, int state)
{
    desktop->snapindicator->remove_snaptarget();
    Geom::Point p[2];

    auto confine = Modifiers::Modifier::get(Modifiers::Type::TRANS_CONFINE)->active(state);
    auto off_center = Modifiers::Modifier::get(Modifiers::Type::TRANS_OFF_CENTER)->active(state);

    SnapManager &m = desktop->namedview->snap_manager;
    m.setup(desktop, false, item);
    Inkscape::SnappedPoint snappoint;

    if (confine) {
        /* We are constrained to producing integer-ratio rectangles */

        /* Vector from the centre of the box to the point we are dragging to */
        Geom::Point delta = pt - center;

        /* Round it so that we have an integer-ratio (or golden ratio) box */
        if (fabs(delta[Geom::X]) > fabs(delta[Geom::Y]) && (delta[Geom::Y] != 0.0)) {
            double ratio = delta[Geom::X] / delta[Geom::Y];
            double ratioabs = fabs (ratio);
            double sign = (ratio < 0 ? -1 : 1);
            if (midpt_1_goldenratio < ratioabs && ratioabs < midpt_goldenratio_2) {
                delta[Geom::X] = sign * goldenratio * delta[Geom::Y];
            } else {
                delta[Geom::X] = floor(ratio + 0.5) * delta[Geom::Y];
            }
        } else if (delta[Geom::X] != 0.0) {
            double ratio = delta[Geom::Y] / delta[Geom::X];
            double ratioabs = fabs (ratio);
            double sign = (ratio < 0 ? -1 : 1);
            if (midpt_1_goldenratio < ratioabs && ratioabs < midpt_goldenratio_2) {
                delta[Geom::Y] = sign * goldenratio * delta[Geom::X];
            } else {
                delta[Geom::Y] = floor(delta[Geom::Y] / delta[Geom::X] + 0.5) * delta[Geom::X];
            }
        }

        /* p[1] is the dragged point with the integer-ratio constraint */
        p[1] = center + delta;

        if (off_center) {

            // Our origin is the centre point rather than the corner point;
            // this means that corner-point movements are bound to each other.

            /* p[0] is the opposite corner of our box */
            p[0] = center - delta;

            Inkscape::SnappedPoint s[2];

            /* Try to snap p[0] (the opposite corner) along the constraint vector */
            s[0] = m.constrainedSnap(Inkscape::SnapCandidatePoint(p[0], Inkscape::SNAPSOURCE_NODE_HANDLE),
                                     Inkscape::Snapper::SnapConstraint(p[0] - p[1]));

            /* Try to snap p[1] (the dragged corner) along the constraint vector */
            s[1] = m.constrainedSnap(Inkscape::SnapCandidatePoint(p[1], Inkscape::SNAPSOURCE_NODE_HANDLE),
                                     Inkscape::Snapper::SnapConstraint(p[1] - p[0]));

            /* Choose the best snap and update points accordingly */
            if (s[0].getSnapDistance() < s[1].getSnapDistance()) {
                if (s[0].getSnapped()) {
                    p[0] = s[0].getPoint();
                    p[1] = 2 * center - s[0].getPoint();
                    snappoint = s[0];
                }
            } else {
                if (s[1].getSnapped()) {
                    p[0] = 2 * center - s[1].getPoint();
                    p[1] = s[1].getPoint();
                    snappoint = s[1];
                }
            }
        } else {

            /* Our origin is the opposite corner.  Snap the drag point along the constraint vector */
            p[0] = center;
            snappoint = m.constrainedSnap(Inkscape::SnapCandidatePoint(p[1], Inkscape::SNAPSOURCE_NODE_HANDLE),
                                          Inkscape::Snapper::SnapConstraint(p[1] - p[0]));
            if (snappoint.getSnapped()) {
                p[1] = snappoint.getPoint();
            }
        }

    } else if (off_center) {
        // Our origin is the centre point rather than the corner point;
        // this means that corner-point movements are bound to each other.

        p[1] = pt;
        p[0] = 2 * center - p[1];

        Inkscape::SnappedPoint s[2];

        s[0] = m.freeSnap(Inkscape::SnapCandidatePoint(p[0], Inkscape::SNAPSOURCE_NODE_HANDLE));
        s[1] = m.freeSnap(Inkscape::SnapCandidatePoint(p[1], Inkscape::SNAPSOURCE_NODE_HANDLE));

        if (s[0].getSnapDistance() < s[1].getSnapDistance()) {
            if (s[0].getSnapped()) {
                p[0] = s[0].getPoint();
                p[1] = 2 * center - s[0].getPoint();
                snappoint = s[0];
            }
        } else {
            if (s[1].getSnapped()) {
                p[0] = 2 * center - s[1].getPoint();
                p[1] = s[1].getPoint();
                snappoint = s[1];
            }
        }

    } else {

        /* There's no constraint on the corner point, so just snap it to anything */
        p[0] = center;
        p[1] = pt;
        snappoint = m.freeSnap(Inkscape::SnapCandidatePoint(pt, Inkscape::SNAPSOURCE_NODE_HANDLE));
        if (snappoint.getSnapped()) {
            p[1] = snappoint.getPoint();
        }
    }

    if (snappoint.getSnapped()) {
        desktop->snapindicator->set_new_snaptarget(snappoint);
    }

    p[0] *= desktop->dt2doc();
    p[1] *= desktop->dt2doc();

    m.unSetup();

    return Geom::Rect(Geom::Point(MIN(p[0][Geom::X], p[1][Geom::X]), MIN(p[0][Geom::Y], p[1][Geom::Y])),
                    Geom::Point(MAX(p[0][Geom::X], p[1][Geom::X]), MAX(p[0][Geom::Y], p[1][Geom::Y])));
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
