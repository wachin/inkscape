// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Handy zooming tool
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *
 * Copyright (C) 1999-2002 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#include <gdk/gdkkeysyms.h>

#include "zoom-tool.h"

#include "desktop.h"
#include "rubberband.h"
#include "selection-chemistry.h"

#include "include/macros.h"

namespace Inkscape {
namespace UI {
namespace Tools {

ZoomTool::ZoomTool(SPDesktop *desktop)
    : ToolBase(desktop, "/tools/zoom", "zoom-in.svg")
    , escaped(false)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/tools/zoom/selcue")) {
        this->enableSelectionCue();
    }

    if (prefs->getBool("/tools/zoom/gradientdrag")) {
        this->enableGrDrag();
    }
}

ZoomTool::~ZoomTool()
{
    this->enableGrDrag(false);
    ungrabCanvasEvents();
}

bool ZoomTool::root_handler(GdkEvent* event) {
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
	
    tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);
    double const zoom_inc = prefs->getDoubleLimited("/options/zoomincrement/value", M_SQRT2, 1.01, 10);

    bool ret = false;

    switch (event->type) {
        case GDK_BUTTON_PRESS:
        {
            Geom::Point const button_w(event->button.x, event->button.y);
            Geom::Point const button_dt(_desktop->w2d(button_w));

            if (event->button.button == 1) {
                // save drag origin
                xp = (gint) event->button.x;
                yp = (gint) event->button.y;
                within_tolerance = true;

                Inkscape::Rubberband::get(_desktop)->start(_desktop, button_dt);

                escaped = false;

                ret = true;
            } else if (event->button.button == 3) {
                double const zoom_rel( (event->button.state & GDK_SHIFT_MASK)
                                       ? zoom_inc
                                       : 1 / zoom_inc );

                _desktop->zoom_relative(button_dt, zoom_rel);
                ret = true;
            }

            grabCanvasEvents(Gdk::KEY_PRESS_MASK      |
                             Gdk::KEY_RELEASE_MASK    |
                             Gdk::BUTTON_PRESS_MASK   |
                             Gdk::BUTTON_RELEASE_MASK |
                             Gdk::POINTER_MOTION_MASK );
            break;
        }

	case GDK_MOTION_NOTIFY:
            if ((event->motion.state & GDK_BUTTON1_MASK)) {
                ret = true;

                if ( within_tolerance
                     && ( abs( (gint) event->motion.x - xp ) < tolerance )
                     && ( abs( (gint) event->motion.y - yp ) < tolerance ) ) {
                    break; // do not drag if we're within tolerance from origin
                }
                // Once the user has moved farther than tolerance from the original location
                // (indicating they intend to move the object, not click), then always process the
                // motion notify coordinates as given (no snapping back to origin)
                within_tolerance = false;

                Geom::Point const motion_w(event->motion.x, event->motion.y);
                Geom::Point const motion_dt(_desktop->w2d(motion_w));
                Inkscape::Rubberband::get(_desktop)->move(motion_dt);
                gobble_motion_events(GDK_BUTTON1_MASK);
            }
            break;

      	case GDK_BUTTON_RELEASE:
        {
            Geom::Point const button_w(event->button.x, event->button.y);
            Geom::Point const button_dt(_desktop->w2d(button_w));

            if ( event->button.button == 1) {
                Geom::OptRect const b = Inkscape::Rubberband::get(_desktop)->getRectangle();

                if (b && !within_tolerance && !(GDK_SHIFT_MASK & event->button.state) ) {
                    _desktop->set_display_area(*b, 10);
                } else if (!escaped) {
                    double const zoom_rel( (event->button.state & GDK_SHIFT_MASK)
                                           ? 1 / zoom_inc
                                           : zoom_inc );

                    _desktop->zoom_relative(button_dt, zoom_rel);
                }

                ret = true;
            }

            Inkscape::Rubberband::get(_desktop)->stop();

            ungrabCanvasEvents();
			
            xp = yp = 0;
            escaped = false;
            break;
        }
        case GDK_KEY_PRESS:
            switch (get_latin_keyval (&event->key)) {
                case GDK_KEY_Escape:
                    if (!Inkscape::Rubberband::get(_desktop)->is_started()) {
                        Inkscape::SelectionHelper::selectNone(_desktop);
                    }

                    Inkscape::Rubberband::get(_desktop)->stop();
                    xp = yp = 0;
                    escaped = true;
                    ret = true;
                    break;

                case GDK_KEY_Up:
                case GDK_KEY_Down:
                case GDK_KEY_KP_Up:
                case GDK_KEY_KP_Down:
                    // prevent the zoom field from activation
                    if (!MOD__CTRL_ONLY(event))
                        ret = true;
                    break;

                case GDK_KEY_Shift_L:
                case GDK_KEY_Shift_R:
                    this->set_cursor("zoom-out.svg");
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
            switch (get_latin_keyval (&event->key)) {
            	case GDK_KEY_Shift_L:
            	case GDK_KEY_Shift_R:
                    this->set_cursor("zoom-in.svg");
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
