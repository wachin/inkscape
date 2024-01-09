// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * New node tool - implementation.
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk@gmail.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <iomanip>

#include <glibmm/ustring.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "desktop.h"
#include "document.h"
#include "message-context.h"
#include "rubberband.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "snap.h"

#include "display/curve.h"
#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-group.h"

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"

#include "include/macros.h"

#include "object/sp-clippath.h"
#include "object/sp-item-group.h"
#include "object/sp-mask.h"
#include "object/sp-namedview.h"
#include "object/sp-path.h"
#include "object/sp-shape.h"
#include "object/sp-text.h"

#include "ui/knot/knot-holder.h"
#include "ui/modifiers.h"
#include "ui/shape-editor.h" // temporary!
#include "ui/tool/control-point-selection.h"
#include "ui/tool/curve-drag-point.h"
#include "ui/tool/event-utils.h"
#include "ui/tool/multi-path-manipulator.h"
#include "ui/tool/path-manipulator.h"
#include "ui/tools/node-tool.h"

using Inkscape::Modifiers::Modifier;

/** @struct NodeTool
 *
 * Node tool event context.
 *
 * @par Architectural overview of the tool
 * @par
 * Here's a breakdown of what each object does.
 * - Handle: shows a handle and keeps the node type constraint (smooth / symmetric) by updating
 *   the other handle's position when dragged. Its move() method cannot violate the constraints.
 * - Node: keeps node type constraints for auto nodes and smooth nodes at ends of linear segments.
 *   Its move() method cannot violate constraints. Handles linear grow and dispatches spatial grow
 *   to MultiPathManipulator. Keeps a reference to its NodeList.
 * - NodeList: exposes an iterator-based interface to nodes. It is possible to obtain an iterator
 *   to a node from the node. Keeps a reference to its SubpathList.
 * - SubpathList: list of NodeLists that represents an editable pathvector. Keeps a reference
 *   to its PathManipulator.
 * - PathManipulator: performs most of the single-path actions like reverse subpaths,
 *   delete segment, shift selection, etc. Keeps a reference to MultiPathManipulator.
 * - MultiPathManipulator: performs additional operations for actions that are not per-path,
 *   for example node joins and segment joins. Tracks the control transforms for PMs that edit
 *   clipping paths and masks. It is more or less equivalent to ShapeEditor and in the future
 *   it might handle all shapes. Handles XML commit of actions that affect all paths or
 *   the node selection and removes PathManipulators that have no nodes left after e.g. node
 *   deletes.
 * - ControlPointSelection: keeps track of node selection and a set of nodes that can potentially
 *   be selected. There can be more than one selection. Performs actions that require no
 *   knowledge about the path, only about the nodes, like dragging and transforms. It is not
 *   specific to nodes and can accommodate any control point derived from SelectableControlPoint.
 *   Transforms nodes in response to transform handle events.
 * - TransformHandleSet: displays nodeset transform handles and emits transform events. The aim
 *   is to eventually use a common class for object and control point transforms.
 * - SelectableControlPoint: base for any type of selectable point. It can belong to only one
 *   selection.
 *
 * @par Functionality that resides in weird places
 * @par
 *
 * This list is probably incomplete.
 * - Curve dragging: CurveDragPoint, controlled by PathManipulator
 * - Single handle shortcuts: MultiPathManipulator::event(), ModifierTracker
 * - Linear and spatial grow: Node, spatial grow routed to ControlPointSelection
 * - Committing handle actions performed with the mouse: PathManipulator
 * - Sculpting: ControlPointSelection
 *
 * @par Plans for the future
 * @par
 * - MultiPathManipulator should become a generic shape editor that manages all active manipulator,
 *   more or less like the old ShapeEditor.
 * - Knotholder should be rewritten into one manipulator class per shape, using the control point
 *   classes. Interesting features like dragging rectangle sides could be added along the way.
 * - Better handling of clip and mask editing, particularly in response to undo.
 * - High level refactoring of the event context hierarchy. All aspects of tools, like toolbox
 *   controls, icons, event handling should be collected in one class, though each aspect
 *   of a tool might be in an separate class for better modularity. The long term goal is to allow
 *   tools to be defined in extensions or shared library plugins.
 */


namespace Inkscape {
namespace UI {
namespace Tools {

Inkscape::CanvasItemGroup *create_control_group(SPDesktop *desktop)
{
    auto group = new Inkscape::CanvasItemGroup(desktop->getCanvasControls());
    group->set_name("CanvasItemGroup:NodeTool");
    return group;
}

NodeTool::NodeTool(SPDesktop *desktop)
    : ToolBase(desktop, "/tools/nodes", "node.svg")
{
    this->_path_data = new Inkscape::UI::PathSharedData();

    Inkscape::UI::PathSharedData &data = *this->_path_data;
    data.node_data.desktop = desktop;

    // Prepare canvas groups for controls. This guarantees correct z-order, so that
    // for example a dragpoint won't obscure a node
    data.outline_group          = create_control_group(desktop);
    data.node_data.handle_line_group = new Inkscape::CanvasItemGroup(desktop->getCanvasControls());
    data.dragpoint_group        = create_control_group(desktop);
    _transform_handle_group     = create_control_group(desktop);
    data.node_data.node_group   = create_control_group(desktop);
    data.node_data.handle_group = create_control_group(desktop);

    data.node_data.handle_line_group->set_name("CanvasItemGroup:NodeTool:handle_line_group");

    Inkscape::Selection *selection = desktop->getSelection();

    this->_selection_changed_connection.disconnect();
    this->_selection_changed_connection =
        selection->connectChanged(sigc::mem_fun(*this, &NodeTool::selection_changed));

    this->_mouseover_changed_connection.disconnect();
    this->_mouseover_changed_connection = 
        Inkscape::UI::ControlPoint::signal_mouseover_change.connect(sigc::mem_fun(*this, &NodeTool::mouseover_changed));

    if (this->_transform_handle_group) {
        this->_selected_nodes = new Inkscape::UI::ControlPointSelection(desktop, this->_transform_handle_group);
    }
    data.node_data.selection = this->_selected_nodes;

    this->_multipath = new Inkscape::UI::MultiPathManipulator(data, this->_selection_changed_connection);

    this->_multipath->signal_coords_changed.connect([=](){
        desktop->emit_control_point_selected(this, _selected_nodes);
    });

    this->_selected_nodes->signal_selection_changed.connect(
        // Hide both signal parameters and bind the function parameter to 0
        // sigc::signal<void (SelectableControlPoint *, bool)>
        // <=>
        // void update_tip(GdkEvent *event)
        sigc::hide(sigc::hide(sigc::bind(
                sigc::mem_fun(*this, &NodeTool::update_tip),
                (GdkEvent*)nullptr
        )))
    );

    this->cursor_drag = false;
    this->show_transform_handles = true;
    this->single_node_transform_handles = false;
    this->flash_tempitem = nullptr;
    this->flashed_item = nullptr;
    this->_last_over = nullptr;

    // read prefs before adding items to selection to prevent momentarily showing the outline
    sp_event_context_read(this, "show_handles");
    sp_event_context_read(this, "show_outline");
    sp_event_context_read(this, "live_outline");
    sp_event_context_read(this, "live_objects");
    sp_event_context_read(this, "show_path_direction");
    sp_event_context_read(this, "show_transform_handles");
    sp_event_context_read(this, "single_node_transform_handles");
    sp_event_context_read(this, "edit_clipping_paths");
    sp_event_context_read(this, "edit_masks");

    this->selection_changed(selection);
    this->update_tip(nullptr);

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (prefs->getBool("/tools/nodes/selcue")) {
        this->enableSelectionCue();
    }

    if (prefs->getBool("/tools/nodes/gradientdrag")) {
        this->enableGrDrag();
    }

    desktop->emit_control_point_selected(this, _selected_nodes); // sets the coord entry fields to inactive
    sp_update_helperpath(desktop);
}

NodeTool::~NodeTool()
{
    this->_selected_nodes->clear();
    this->get_rubberband()->stop();

    this->enableGrDrag(false);

    if (this->flash_tempitem) {
        _desktop->remove_temporary_canvasitem(this->flash_tempitem);
    }
    for (auto hp : this->_helperpath_tmpitem) {
        _desktop->remove_temporary_canvasitem(hp);
    }
    this->_selection_changed_connection.disconnect();
    // this->_selection_modified_connection.disconnect();
    this->_mouseover_changed_connection.disconnect();

    delete this->_multipath;
    delete this->_selected_nodes;

    _path_data->node_data.node_group->unlink();
    _path_data->node_data.handle_group->unlink();
    _path_data->node_data.handle_line_group->unlink();
    _path_data->outline_group->unlink();
    _path_data->dragpoint_group->unlink();
    _transform_handle_group->unlink();
}

Inkscape::Rubberband *NodeTool::get_rubberband() const
{
    return Inkscape::Rubberband::get(_desktop);
}

void NodeTool::deleteSelected()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    // This takes care of undo internally
    _multipath->deleteNodes(prefs->getBool("/tools/nodes/delete_preserves_shape", true));
}

// show helper paths of the applied LPE, if any
void sp_update_helperpath(SPDesktop *desktop)
{
    if (!desktop) {
        return;
    }

    Inkscape::UI::Tools::NodeTool *nt = dynamic_cast<Inkscape::UI::Tools::NodeTool*>(desktop->event_context);
    if (!nt) {
        // We remove this warning and just stop execution
        // because we are updating helper paths also from LPE dialog so we not unsure the tool used
        // std::cerr << "sp_update_helperpath called when Node Tool not active!" << std::endl;
        return;
    }

    Inkscape::Selection *selection = desktop->getSelection();
    for (auto hp : nt->_helperpath_tmpitem) {
        desktop->remove_temporary_canvasitem(hp);
    }
    nt->_helperpath_tmpitem.clear();
    std::vector<SPItem *> vec(selection->items().begin(), selection->items().end());
    std::vector<std::pair<Geom::PathVector, Geom::Affine>> cs;
    for (auto item : vec) {
        auto lpeitem = cast<SPLPEItem>(item);
        if (lpeitem && lpeitem->hasPathEffectRecursive()) {
            Inkscape::LivePathEffect::Effect *lpe = lpeitem->getCurrentLPE();
            if (lpe && lpe->isVisible()/* && lpe->showOrigPath()*/) {
                std::vector<Geom::Point> selectedNodesPositions;
                if (nt->_selected_nodes) {
                    Inkscape::UI::ControlPointSelection *selectionNodes = nt->_selected_nodes;
                    for (auto selectionNode : *selectionNodes) {
                        Inkscape::UI::Node *n = dynamic_cast<Inkscape::UI::Node *>(selectionNode);
                        selectedNodesPositions.push_back(n->position());
                    }
                }
                lpe->setSelectedNodePoints(selectedNodesPositions);
                lpe->setCurrentZoom(desktop->current_zoom());
                SPCurve c;
                std::vector<Geom::PathVector> cs = lpe->getCanvasIndicators(lpeitem);
                for (auto &p : cs) {
                    p *= desktop->dt2doc();
                    c.append(p);
                }
                if (!c.is_empty()) {
                    auto helperpath = new Inkscape::CanvasItemBpath(desktop->getCanvasTemp(), c.get_pathvector(), true);
                    helperpath->set_stroke(0x0000ff9a);
                    helperpath->set_fill(0x0, SP_WIND_RULE_NONZERO); // No fill
                    nt->_helperpath_tmpitem.emplace_back(desktop->add_temporary_canvasitem(helperpath, 0));
                }
            }
        }
    }
}

void NodeTool::set(const Inkscape::Preferences::Entry& value) {
    Glib::ustring entry_name = value.getEntryName();

    if (entry_name == "show_handles") {
        this->show_handles = value.getBool(true);
        this->_multipath->showHandles(this->show_handles);
    } else if (entry_name == "show_outline") {
        this->show_outline = value.getBool();
        this->_multipath->showOutline(this->show_outline);
    } else if (entry_name == "live_outline") {
        this->live_outline = value.getBool();
        this->_multipath->setLiveOutline(this->live_outline);
    } else if (entry_name == "live_objects") {
        this->live_objects = value.getBool();
        this->_multipath->setLiveObjects(this->live_objects);
    } else if (entry_name == "show_path_direction") {
        this->show_path_direction = value.getBool();
        this->_multipath->showPathDirection(this->show_path_direction);
    } else if (entry_name == "show_transform_handles") {
        this->show_transform_handles = value.getBool(true);
        this->_selected_nodes->showTransformHandles(
            this->show_transform_handles, this->single_node_transform_handles);
    } else if (entry_name == "single_node_transform_handles") {
        this->single_node_transform_handles = value.getBool();
        this->_selected_nodes->showTransformHandles(
            this->show_transform_handles, this->single_node_transform_handles);
    } else if (entry_name == "edit_clipping_paths") {
        this->edit_clipping_paths = value.getBool();
        this->selection_changed(_desktop->getSelection());
    } else if (entry_name == "edit_masks") {
        this->edit_masks = value.getBool();
        this->selection_changed(_desktop->getSelection());
    } else {
        ToolBase::set(value);
    }
}

/** Recursively collect ShapeRecords */
static
void gather_items(NodeTool *nt, SPItem *base, SPObject *obj, Inkscape::UI::ShapeRole role,
    std::set<Inkscape::UI::ShapeRecord> &s)
{
    using namespace Inkscape::UI;

    if (!obj) {
        return;
    }

    //XML Tree being used directly here while it shouldn't be.
    if (role != SHAPE_ROLE_NORMAL && (is<SPGroup>(obj) || is<SPObjectGroup>(obj))) {
        for (auto& c: obj->children) {
            gather_items(nt, base, &c, role, s);
        }
    } else if (auto item = cast<SPItem>(obj)) {
        ShapeRecord r;
        r.object = obj;
        r.role = role;

        // TODO add support for objectBoundingBox
        if (role != SHAPE_ROLE_NORMAL && base) {
            r.edit_transform = base->i2doc_affine();
        }

        if (s.insert(r).second) {
            // this item was encountered the first time
            if (nt->edit_clipping_paths) {
                gather_items(nt, item, item->getClipObject(), SHAPE_ROLE_CLIPPING_PATH, s);
            }

            if (nt->edit_masks) {
                gather_items(nt, item, item->getMaskObject(), SHAPE_ROLE_MASK, s);
            }
        }
    }
}

void NodeTool::selection_changed(Inkscape::Selection *sel) {
    using namespace Inkscape::UI;

    std::set<ShapeRecord> shapes;

    auto items= sel->items();
    for(auto i=items.begin();i!=items.end();++i){
        SPItem *item = *i;
        if (item) {
            gather_items(this, nullptr, item, SHAPE_ROLE_NORMAL, shapes);
        }
    }

    // use multiple ShapeEditors for now, to allow editing many shapes at once
    // needs to be rethought
    for (auto i = this->_shape_editors.begin(); i != this->_shape_editors.end();) {
        ShapeRecord s;
        s.object = i->first;

        if (shapes.find(s) == shapes.end()) {
            this->_shape_editors.erase(i++);
        } else {
            ++i;
        }
    }

    for (const auto & r : shapes) {
        if (this->_shape_editors.find(cast<SPItem>(r.object)) == this->_shape_editors.end()) {
            auto si = std::make_unique<ShapeEditor>(_desktop, r.edit_transform);
            auto item = cast<SPItem>(r.object);
            si->set_item(item);
            this->_shape_editors.insert({item, std::move(si)});
        }
    }

    std::vector<SPItem *> vec(sel->items().begin(), sel->items().end());
    _previous_selection = _current_selection;
    _current_selection = vec;
    this->_multipath->setItems(shapes);
    this->update_tip(nullptr);
    sp_update_helperpath(_desktop);
    // This not need to be called canvas is updated on selection change on setItems
    // _desktop->updateNow();
}

bool NodeTool::root_handler(GdkEvent* event) {
    /* things to handle here:
     * 1. selection of items
     * 2. passing events to manipulators
     * 3. some keybindings
     */
    using namespace Inkscape::UI; // pull in event helpers

    Inkscape::Selection *selection = _desktop->getSelection();
    static Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto rband = get_rubberband();

    if (!rband->is_started()) {
        if (_multipath->event(this, event) || _selected_nodes->event(this, event))
            return true;
    }

    switch (event->type)
    {

    case GDK_MOTION_NOTIFY: {
        sp_update_helperpath(_desktop);
        SPItem *over_item = nullptr;
        over_item = sp_event_context_find_item(_desktop, event_point(event->button), FALSE, TRUE);

        Geom::Point const motion_w(event->motion.x, event->motion.y);
        Geom::Point const motion_dt(_desktop->w2d(motion_w));

        if (event->motion.state & GDK_BUTTON1_MASK) {
            if (rband->is_started()) {
                rband->move(motion_dt);
            }
  
            auto touch_path = Modifier::get(Modifiers::Type::SELECT_TOUCH_PATH)->get_label();
            if (rband->getMode() == RUBBERBAND_MODE_TOUCHPATH) {
                this->defaultMessageContext()->setF(Inkscape::NORMAL_MESSAGE,
                    _("<b>Draw over</b> lines to select their nodes; release <b>%s</b> to switch to rubberband selection"), touch_path.c_str());
            } else {
                this->defaultMessageContext()->setF(Inkscape::NORMAL_MESSAGE,
                    _("<b>Drag around</b> nodes to select them; press <b>%s</b> to switch to box selection"), touch_path.c_str());
            }
            return true;
        } else if (rband->is_moved()) {
            // Mouse button is up, but rband is still kicking.
            rband->stop();
        }

        SnapManager &m = _desktop->namedview->snap_manager;

        // We will show a pre-snap indication for when the user adds a node through double-clicking
        // Adding a node will only work when a path has been selected; if that's not the case then snapping is useless
        if (!_desktop->getSelection()->isEmpty()) {
            if (!(event->motion.state & GDK_SHIFT_MASK)) {
                m.setup(_desktop);
                Inkscape::SnapCandidatePoint scp(motion_dt, Inkscape::SNAPSOURCE_OTHER_HANDLE);
                m.preSnap(scp, true);
                m.unSetup();
            }
        }

        if (over_item && over_item != this->_last_over) {
            this->_last_over = over_item;
            //ink_node_tool_update_tip(nt, event);
            this->update_tip(event);
        }
        // create pathflash outline

        if (prefs->getBool("/tools/nodes/pathflash_enabled")) {
            if (over_item == this->flashed_item) {
                break;
            }

            if (!prefs->getBool("/tools/nodes/pathflash_selected") && over_item && selection->includes(over_item)) {
                break;
            }

            if (this->flash_tempitem) {
                _desktop->remove_temporary_canvasitem(this->flash_tempitem);
                this->flash_tempitem = nullptr;
                this->flashed_item = nullptr;
            }

            auto shape = cast<SPShape>(over_item);
            if (!shape) {
                break; // for now, handle only shapes
            }

            this->flashed_item = over_item;
            if (!shape->curveForEdit()) {
                break; // break out when curve doesn't exist
            }

            auto c = shape->curveForEdit()->transformed(over_item->i2dt_affine());

            auto flash = new Inkscape::CanvasItemBpath(_desktop->getCanvasTemp(), c.get_pathvector(), true);
            flash->set_stroke(over_item->highlight_color());
            flash->set_fill(0x0, SP_WIND_RULE_NONZERO); // No fill.
            flash_tempitem =
                _desktop->add_temporary_canvasitem(flash, prefs->getInt("/tools/nodes/pathflash_timeout", 500));
        }
        break; // do not return true, because we need to pass this event to the parent context
               // otherwise some features cease to work
    }

    case GDK_KEY_PRESS:
        switch (get_latin_keyval(&event->key))
        {
        case GDK_KEY_Escape: // deselect everything
            if (this->_selected_nodes->empty()) {
                Inkscape::SelectionHelper::selectNone(_desktop);
            } else {
                this->_selected_nodes->clear();
            }
            //ink_node_tool_update_tip(nt, event);
            this->update_tip(event);
            return TRUE;

        case GDK_KEY_a:
        case GDK_KEY_A:
            if (held_control(event->key) && held_alt(event->key)) {
                this->_selected_nodes->selectAll();
                // Ctrl+A is handled in selection-chemistry.cpp via verb
                //ink_node_tool_update_tip(nt, event);
                this->update_tip(event);
                return TRUE;
            }
            break;

        case GDK_KEY_h:
        case GDK_KEY_H:
            if (held_only_control(event->key)) {
                Inkscape::Preferences *prefs = Inkscape::Preferences::get();
                prefs->setBool("/tools/nodes/show_handles", !this->show_handles);
                return TRUE;
            }
            break;

        case GDK_KEY_Tab:
            _multipath->shiftSelection(1);
            return TRUE;
            break;
        case GDK_KEY_ISO_Left_Tab:
            _multipath->shiftSelection(-1);
            return TRUE;
            break;

        default:
            break;
        }
        //ink_node_tool_update_tip(nt, event);
        this->update_tip(event);
        break;

    case GDK_KEY_RELEASE:
        //ink_node_tool_update_tip(nt, event);
        this->update_tip(event);
        break;

    case GDK_BUTTON_PRESS:
        if (event->button.button == 1) {
            if(Modifier::get(Modifiers::Type::SELECT_TOUCH_PATH)->active(event->button.state)) {
                rband->setMode(RUBBERBAND_MODE_TOUCHPATH);
            } else {
                rband->defaultMode();
            }

            Geom::Point const event_pt(event->button.x, event->button.y);
            Geom::Point const desktop_pt(_desktop->w2d(event_pt));
            rband->start(_desktop, desktop_pt, true);
            return true;
        }
        break;

    case GDK_BUTTON_RELEASE:
        if (event->button.button == 1) {
            if (rband->is_started() && rband->is_moved()) {
                select_area(rband->getPath(), &event->button);
            } else {
                select_point(&event->button);
            }
            rband->stop();
            return true;
        }
        break;

    case GDK_2BUTTON_PRESS:
        if ( event->button.button == 1 ) {
            // If the selector received the doubleclick event, then we're at some distance from
            // the path; otherwise, the doubleclick event would have been received by
            // CurveDragPoint; we will insert nodes into the path anyway but only if we can snap
            // to the path. Otherwise the position would not be very well defined.
            if (!(event->motion.state & GDK_SHIFT_MASK)) {
                Geom::Point const motion_w(event->motion.x, event->motion.y);
                Geom::Point const motion_dt(_desktop->w2d(motion_w));

                SnapManager &m = _desktop->namedview->snap_manager;
                m.setup(_desktop);
                Inkscape::SnapCandidatePoint scp(motion_dt, Inkscape::SNAPSOURCE_OTHER_HANDLE);
                Inkscape::SnappedPoint sp = m.freeSnap(scp, Geom::OptRect(), true);
                m.unSetup();

                if (sp.getSnapped()) {
                    // The first click of the double click will have cleared the path selection, because
                    // we clicked aside of the path. We need to undo this on double click
                    Inkscape::Selection *selection = _desktop->getSelection();
                    selection->addList(_previous_selection);

                    // The selection has been restored, and the signal selection_changed has been emitted,
                    // which has again forced a restore of the _mmap variable of the MultiPathManipulator (this->_multipath)
                    // Now we can insert the new nodes as if nothing has happened!
                    this->_multipath->insertNode(_desktop->d2w(sp.getPoint()));
                    return true;
                }
            }
        }
        break;

    default:
        break;
    }
    // we really dont want to stop any node operation we want to success all even the time consume it

    return ToolBase::root_handler(event);
}

bool NodeTool::item_handler(SPItem *item, GdkEvent *event)
{
    bool ret = ToolBase::item_handler(item, event);

    // Node shape editors are handled differently than shape tools
    if (!ret && event->type == GDK_BUTTON_PRESS && event->button.button == 1) {
        for (auto &se : _shape_editors) {
            // This allows users to select an arbitary position in a pattern to edit on canvas.
            if (auto knotholder = se.second->knotholder) {
                auto point = Geom::Point(event->button.x, event->button.y);

                // This allows us to dive into groups and find what the real item is
                if (_desktop->getItemAtPoint(point, true) != knotholder->getItem())
                    continue;

                ret = knotholder->set_item_clickpos(_desktop->w2d(point) * _desktop->dt2doc());
            }
        }
    }
    return ret;
}

void NodeTool::update_tip(GdkEvent *event) {
    using namespace Inkscape::UI;
    if (event && (event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE)) {
        unsigned new_state = state_after_event(event);

        if (new_state == event->key.state) {
            return;
        }

        if (state_held_shift(new_state)) {
            if (this->_last_over) {
                this->message_context->set(Inkscape::NORMAL_MESSAGE,
                    C_("Node tool tip", "<b>Shift</b>: drag to add nodes to the selection, "
                    "click to toggle object selection"));
            } else {
                this->message_context->set(Inkscape::NORMAL_MESSAGE,
                    C_("Node tool tip", "<b>Shift</b>: drag to add nodes to the selection"));
            }

            return;
        }
    }

    unsigned sz = this->_selected_nodes->size();
    unsigned total = this->_selected_nodes->allPoints().size();

    if (sz != 0) {
        // TODO: Use Glib::ustring::compose and remove the useless copy after string freeze
        char *nodestring_temp = g_strdup_printf(
                ngettext("<b>%u of %u</b> node selected.", "<b>%u of %u</b> nodes selected.", total),
                sz, total);
        Glib::ustring nodestring(nodestring_temp);
        g_free(nodestring_temp);

        if (sz == 2) {
            // if there are only two nodes selected, display the angle
            // of a line going through them relative to the X axis.
            Inkscape::UI::ControlPointSelection::Set &selection_nodes = this->_selected_nodes->allPoints();
            std::vector<Geom::Point> positions;
            for (auto selection_node : selection_nodes) {
                if (selection_node->selected()) {
                    Inkscape::UI::Node *n = dynamic_cast<Inkscape::UI::Node *>(selection_node);
                    positions.push_back(n->position());
                }
            }
            g_assert(positions.size() == 2);
            const double angle = Geom::deg_from_rad(Geom::Line(positions[0], positions[1]).angle());
            nodestring += " ";
            nodestring += Glib::ustring::compose(_("Angle: %1°."),
                                                 Glib::ustring::format(std::fixed, std::setprecision(2), angle));
        }

        if (this->_last_over) {
            // TRANSLATORS: The %s below is where the "%u of %u nodes selected" sentence gets put
            char *dyntip = g_strdup_printf(C_("Node tool tip",
                "%s Drag to select nodes, click to edit only this object (more: Shift)"),
                nodestring.c_str());
            this->message_context->set(Inkscape::NORMAL_MESSAGE, dyntip);
            g_free(dyntip);
        } else {
            char *dyntip = g_strdup_printf(C_("Node tool tip",
                "%s Drag to select nodes, click clear the selection"),
                nodestring.c_str());
            this->message_context->set(Inkscape::NORMAL_MESSAGE, dyntip);
            g_free(dyntip);
        }
    } else if (!this->_multipath->empty()) {
        if (this->_last_over) {
            this->message_context->set(Inkscape::NORMAL_MESSAGE, C_("Node tool tip",
                "Drag to select nodes, click to edit only this object"));
        } else {
            this->message_context->set(Inkscape::NORMAL_MESSAGE, C_("Node tool tip",
                "Drag to select nodes, click to clear the selection"));
        }
    } else {
        if (this->_last_over) {
            this->message_context->set(Inkscape::NORMAL_MESSAGE, C_("Node tool tip",
                "Drag to select objects to edit, click to edit this object (more: Shift)"));
        } else {
            this->message_context->set(Inkscape::NORMAL_MESSAGE, C_("Node tool tip",
                "Drag to select objects to edit"));
        }
    }
}

void NodeTool::select_area(Geom::Path const &path, GdkEventButton *event) {
    using namespace Inkscape::UI;
    
    if (this->_multipath->empty()) {
        // if multipath is empty, select rubberbanded items rather than nodes
        Inkscape::Selection *selection = _desktop->getSelection();
        auto sel_doc = _desktop->dt2doc() * *path.boundsFast();
        std::vector<SPItem *> items = _desktop->getDocument()->getItemsInBox(_desktop->dkey, sel_doc);
        selection->setList(items);
    } else {
        bool shift = held_shift(*event);
        bool ctrl = held_control(*event);

        if (!shift) {
            // A/C. No modifier, selects all nodes, or selects all other nodes.
            this->_selected_nodes->clear();
        }
        if (shift && ctrl) {
            // D. Shift+Ctrl pressed, removes nodes under box from existing selection.
            this->_selected_nodes->selectArea(path, true);
        } else {
            // A/B/C. Adds nodes under box to existing selection.
            this->_selected_nodes->selectArea(path);
            if (ctrl) {
                // C. Selects the inverse of all nodes under the box.
                this->_selected_nodes->invertSelection();
            }
        }
    }
}

void NodeTool::select_point(GdkEventButton *event) {
    using namespace Inkscape::UI; // pull in event helpers
    
    if (!event) {
        return;
    }

    if (event->button != 1) {
        return;
    }

    Inkscape::Selection *selection = _desktop->getSelection();

    SPItem *item_clicked = sp_event_context_find_item (_desktop, event_point(*event),
                    (event->state & GDK_MOD1_MASK) && !(event->state & GDK_CONTROL_MASK), TRUE);

    if (item_clicked == nullptr) { // nothing under cursor
        // if no Shift, deselect
        // if there are nodes selected, the first click should deselect the nodes
        // and the second should deselect the items
        if (!state_held_shift(event->state)) {
            if (this->_selected_nodes->empty()) {
                selection->clear();
            } else {
                this->_selected_nodes->clear();
            }
        }
    } else {
        if (held_shift(*event)) {
            selection->toggle(item_clicked);
        } else if (!selection->includes(item_clicked)) {
            selection->set(item_clicked);
        }
        // This not need to be called canvas is updated on selection change
        // _desktop->updateNow();
    }
}

void NodeTool::mouseover_changed(Inkscape::UI::ControlPoint *p) {
    using Inkscape::UI::CurveDragPoint;

    CurveDragPoint *cdp = dynamic_cast<CurveDragPoint*>(p);

    if (cdp && !this->cursor_drag) {
        this->set_cursor("node-mouseover.svg");
        this->cursor_drag = true;
    } else if (!cdp && this->cursor_drag) {
        this->set_cursor("node.svg");
        this->cursor_drag = false;
    }
}

void NodeTool::handleControlUiStyleChange() {
    this->_multipath->updateHandles();
}

}
}
}

//} // anonymous namespace

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
