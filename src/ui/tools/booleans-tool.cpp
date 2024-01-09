// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Martin Owens
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/i18n.h>

#include "actions/actions-tools.h" // set_active_tool()
#include "ui/tools/booleans-tool.h"
#include "ui/tools/booleans-builder.h"
#include "display/control/canvas-item-group.h"
#include "display/control/canvas-item-drawing.h"

#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "event-log.h"
#include "include/macros.h"
#include "selection.h"
#include "ui/icon-names.h"
#include "ui/modifiers.h"

using Inkscape::DocumentUndo;
using Inkscape::Modifiers::Modifier;

namespace Inkscape {
namespace UI {
namespace Tools {

InteractiveBooleansTool::InteractiveBooleansTool(SPDesktop *desktop)
    : ToolBase(desktop, "/tools/booleans", "select.svg")
{
    to_commit = false;
    change_mode(true);
    update_status();
    if (auto selection = desktop->getSelection()) {
        desktop->setWaitingCursor();
        boolean_builder = std::make_unique<BooleanBuilder>(selection);
        desktop->clearWaitingCursor();

        // Any changes to the selection cancel the shape building process
        _sel_modified = selection->connectModified([=](Selection *sel, int) { shape_cancel(); });
        _sel_changed = selection->connectChanged([=](Selection *sel) { shape_cancel(); });
    }
}

InteractiveBooleansTool::~InteractiveBooleansTool()
{
    change_mode(false);
    _sel_modified.disconnect();
    _sel_changed.disconnect();
}

void InteractiveBooleansTool::change_mode(bool setup)
{
    _desktop->doc()->get_event_log()->updateUndoVerbs();
    _desktop->getCanvasPagesBg()->set_visible(!setup);
    _desktop->getCanvasPagesFg()->set_visible(!setup);
    _desktop->getCanvasDrawing()->set_visible(!setup);
}

void InteractiveBooleansTool::switching_away(const std::string &new_tool)
{
    if (!new_tool.empty() && boolean_builder && new_tool == "/tools/select" || new_tool == "/tool/nodes") {
        // Only forcefully commit if we have the user's explicit instruction to do so.
        if (boolean_builder->has_changes() || to_commit) {
            _desktop->getSelection()->setList(boolean_builder->shape_commit(true));
            DocumentUndo::done(_desktop->doc(), "Built Shapes", INKSCAPE_ICON("draw-booleans"));
        }
    }
}

bool InteractiveBooleansTool::is_ready() const {
    if (!boolean_builder || !boolean_builder->has_items()) {
        if (_desktop->getSelection()->isEmpty()) {
            _desktop->showNotice(_("You must select some objects to use the Shape Builder tool."), 5000);
        } else {
            _desktop->showNotice(_("The Shape Builder requires regular shapes to be selected."), 5000);
        }
        return false;
    }
    return true;
}

void InteractiveBooleansTool::set(const Inkscape::Preferences::Entry& val)
{
    Glib::ustring path = val.getEntryName();
    if (path == "/tools/booleans/mode") {
        update_status();
        boolean_builder->task_cancel();
    }
}

void InteractiveBooleansTool::shape_commit()
{
    to_commit = true;
    // disconnect so we don't get canceled by accident.
    _sel_modified.disconnect();
    _sel_changed.disconnect();
    set_active_tool(_desktop, "Select");
}

void InteractiveBooleansTool::shape_cancel()
{
    boolean_builder.reset();
    set_active_tool(_desktop, "Select");
}

bool InteractiveBooleansTool::root_handler(GdkEvent* event)
{
    if (!boolean_builder)
        return false;

    bool ret = false;
    bool add = should_add(event->button.state);
    switch (event->type) {
        case GDK_BUTTON_PRESS:
            ret = event_button_press_handler(event);
            break;
        case GDK_BUTTON_RELEASE:
            ret = event_button_release_handler(event);
            break;
        case GDK_KEY_PRESS:
            ret = event_key_press_handler(event);
            // no-break;
        case GDK_KEY_RELEASE:
            add = should_add(Modifiers::add_keyval(event->key.state, event->key.keyval, event->type == GDK_KEY_RELEASE));
            break;
        case GDK_MOTION_NOTIFY:
            ret = event_motion_handler(event, add);
            break;
    }
    if (!ret) {
        set_cursor(add ? "cursor-union.svg" : "cursor-delete.svg");
        update_status();
    }
    return ret || ToolBase::root_handler(event);
}

/**
 * Returns true if the shape builder should add items,
 * false if shape builder should delete items
 */
bool InteractiveBooleansTool::should_add(int state) const
{
    auto prefs = Inkscape::Preferences::get();
    bool pref = prefs->getInt("/tools/booleans/mode", 0) != 0;
    auto modifier = Modifier::get(Modifiers::Type::BOOL_SHIFT);
    return pref == modifier->active(state);
}

void InteractiveBooleansTool::update_status()
{
    auto prefs = Inkscape::Preferences::get();
    bool pref = prefs->getInt("/tools/booleans/mode", 0) == 0;
    auto modifier = Modifier::get(Modifiers::Type::BOOL_SHIFT);
    message_context->setF(Inkscape::IMMEDIATE_MESSAGE,
        (pref ? "<b>Drag</b> over fragments to unite them. <b>Click</b> to create a segment. Hold <b>%s</b> to Subtract."
              : "<b>Drag</b> over fragments to delete them. <b>Click</b> to delete a segment. Hold <b>%s</b> to Unite."),
        modifier->get_label().c_str());
}

bool InteractiveBooleansTool::event_button_press_handler(GdkEvent *event)
{
    if (event->button.button == 1) {
        Geom::Point const button_pt(event->button.x, event->button.y);
        boolean_builder->task_select(button_pt, should_add(event->button.state));
        return true;

    } else if (event->button.button == 3) {
        // right click; do not eat it so that right-click menu can appear, but cancel dragging
        boolean_builder->task_cancel();
    }

    return false;
}

bool InteractiveBooleansTool::event_motion_handler(GdkEvent *event, bool add)
{
    Geom::Point const motion_pt(event->motion.x, event->motion.y);

    if ((event->motion.state & GDK_BUTTON1_MASK)) {
        if (boolean_builder->has_task()) {
            return boolean_builder->task_add(motion_pt);
        } else {
            return boolean_builder->task_select(motion_pt, add);
        }
    } else {
        return boolean_builder->highlight(motion_pt, add);
    }

    return false;
}

bool InteractiveBooleansTool::event_button_release_handler(GdkEvent *event)
{
    if (event->button.button == 1) {
        boolean_builder->task_commit();
    }
    return true;
}

bool InteractiveBooleansTool::catch_undo(bool redo) {
    if (redo) {
        boolean_builder->redo();
    } else {
        boolean_builder->undo();
    }
    return true;
}

bool InteractiveBooleansTool::event_key_press_handler(GdkEvent *event)
{
    bool ret = false;
    switch (get_latin_keyval (&event->key)) {
        case GDK_KEY_Escape:
            if (boolean_builder->has_task()) {
                boolean_builder->task_cancel();
            } else {
                shape_cancel();
            }
            ret = true;
            break;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (boolean_builder->has_task()) {
                boolean_builder->task_commit();
            } else {
                shape_commit();
            }
            ret = true;
            break;
        case GDK_KEY_z:
        case GDK_KEY_Z:
            if (event->key.state & INK_GDK_PRIMARY_MASK) {
                ret = catch_undo(event->key.state & GDK_SHIFT_MASK);
            }
            break;


        default:
            break;
    }

    return ret;
}

} // namespace Tools
} // namespace UI
} // namespace Inkscape
