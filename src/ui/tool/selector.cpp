// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Selector component (click and rubberband)
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gdk/gdkkeysyms.h>

#include "control-point.h"
#include "desktop.h"

#include "display/control/canvas-item-rect.h"

#include "ui/tools/tool-base.h"
#include "ui/tool/event-utils.h"
#include "ui/tool/selector.h"

namespace Inkscape {
namespace UI {

/** A hidden control point used for rubberbanding and selection.
 * It uses a clever hack: the canvas item is hidden and only receives events when they
 * are passed to it using Selector's event() function. When left mouse button
 * is pressed, it grabs events and handles drags and clicks in the usual way. */
class SelectorPoint : public ControlPoint {
public:
    SelectorPoint(SPDesktop *d, Inkscape::CanvasItemGroup *group, Selector *s) :
        ControlPoint(d, Geom::Point(0,0), SP_ANCHOR_CENTER,
                     Inkscape::CANVAS_ITEM_CTRL_TYPE_INVISIPOINT,
                     invisible_cset, group),
        _selector(s),
        _cancel(false)
    {
        _canvas_item_ctrl->set_name("CanvasItemCtrl:SelectorPoint");
        setVisible(false);
        _rubber = new Inkscape::CanvasItemRect(_desktop->getCanvasControls());
        _rubber->set_name("CanavasItemRect:SelectorPoint:Rubberband");
        _rubber->set_stroke(0x8080ffff);
        _rubber->set_inverted(true);
        _rubber->hide();
   }

    ~SelectorPoint() override {
        delete _rubber;
    }

    SPDesktop *desktop() { return _desktop; }

    bool event(Inkscape::UI::Tools::ToolBase *ec, GdkEvent *e) {
        return _eventHandler(ec, e);
    }

protected:
    bool _eventHandler(Inkscape::UI::Tools::ToolBase *event_context, GdkEvent *event) override {
        if (event->type == GDK_KEY_PRESS               &&
            shortcut_key(event->key) == GDK_KEY_Escape &&
            _rubber->is_visible()                       )
        {
            _cancel = true;
            _rubber->hide();
            return true;
        }
        return ControlPoint::_eventHandler(event_context, event);
    }

private:
    bool grabbed(GdkEventMotion *) override {
        _cancel = false;
        _start = position();
        _rubber->show();
        return false;
    }

    void dragged(Geom::Point &new_pos, GdkEventMotion *) override {
        if (_cancel) return;
        Geom::Rect sel(_start, new_pos);
        _rubber->set_rect(sel);
    }

    void ungrabbed(GdkEventButton *event) override {
        if (_cancel) return;
        _rubber->hide();
        Geom::Rect sel(_start, position());
        _selector->signal_area.emit(sel, event);
    }

    bool clicked(GdkEventButton *event) override {
        if (event->button != 1) return false;
        _selector->signal_point.emit(position(), event);
        return true;
    }

    Inkscape::CanvasItemRect *_rubber;
    Selector *_selector;
    Geom::Point _start;
    bool _cancel;
};


Selector::Selector(SPDesktop *desktop)
    : Manipulator(desktop)
    , _dragger(new SelectorPoint(desktop, desktop->getCanvasControls(), this))
{
    _dragger->setVisible(false);
}

Selector::~Selector()
{
    delete _dragger;
}

bool Selector::event(Inkscape::UI::Tools::ToolBase *event_context, GdkEvent *event)
{
    // The hidden control point will capture all events after it obtains the grab,
    // but it relies on this function to initiate it. If we pass only first button
    // press events here, it won't interfere with any other event handling.
    switch (event->type) {
    case GDK_BUTTON_PRESS:
        // Do not pass button presses other than left button to the control point.
        // This way middle click and right click can be handled in ToolBase.
        if (event->button.button == 1 && !event_context->is_space_panning()) {
            _dragger->setPosition(_desktop->w2d(event_point(event->motion)));
            return _dragger->event(event_context, event);
        }
        break;
    default: break;
    }
    return false;
}

bool Selector::doubleClicked() {
    return _dragger->doubleClicked();
}

} // namespace UI
} // namespace Inkscape

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
