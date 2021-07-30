// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Multiindex container for selection
 *
 * Authors:
 *   Adrian Boguszewski
 *
 * Copyright (C) 2016 Adrian Boguszewski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "object-set.h"

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <glib.h>
#include <sigc++/sigc++.h>

#include "box3d.h"
#include "persp3d.h"
#include "preferences.h"

namespace Inkscape {

bool ObjectSet::add(SPObject* object, bool nosignal) {
    g_return_val_if_fail(object != nullptr, false);
    g_return_val_if_fail(SP_IS_OBJECT(object), false);

    // any ancestor is in the set - do nothing
    if (_anyAncestorIsInSet(object)) {
        return false;
    }

    // very nice function, but changes selection behavior (probably needs new selection option to deal with it)
    // check if there is mutual ancestor for some elements, which can replace all of them in the set
//    object = _getMutualAncestor(object);

    // remove all descendants from the set
    _removeDescendantsFromSet(object);

    _add(object);
    if (!nosignal)
        _emitChanged();
    return true;
}

void ObjectSet::add(XML::Node *repr)
{
    if (document() && repr) {
        SPObject *obj = document()->getObjectByRepr(repr);
        assert(obj == document()->getObjectById(repr->attribute("id")));
        add(obj);
    }
}

bool ObjectSet::remove(SPObject* object) {
    g_return_val_if_fail(object != nullptr, false);
    g_return_val_if_fail(SP_IS_OBJECT(object), false);

    // object is the top of subtree
    if (includes(object)) {
        _remove(object);
        _emitChanged();
        return true;
    }

    // any ancestor of object is in the set
    if (_anyAncestorIsInSet(object)) {
        _removeAncestorsFromSet(object);
        _emitChanged();
        return true;
    }

    // no object nor any parent in the set
    return false;
}

bool ObjectSet::includes(SPObject *object) {
    g_return_val_if_fail(object != nullptr, false);
    g_return_val_if_fail(SP_IS_OBJECT(object), false);

    return _container.get<hashed>().find(object) != _container.get<hashed>().end();
}

void ObjectSet::clear() {
    _clear();
    _emitChanged();
}

int ObjectSet::size() {
    return _container.size();
}

bool ObjectSet::_anyAncestorIsInSet(SPObject *object) {
    SPObject* o = object;
    while (o != nullptr) {
        if (includes(o)) {
            return true;
        }
        o = o->parent;
    }

    return false;
}

void ObjectSet::_removeDescendantsFromSet(SPObject *object) {
    for (auto& child: object->children) {
        if (includes(&child)) {
            _remove(&child);
            // there is certainly no children of this child in the set
            continue;
        }

        _removeDescendantsFromSet(&child);
    }
}

void ObjectSet::_disconnect(SPObject *object) {
    _releaseConnections[object].disconnect();
    _releaseConnections.erase(object);
    _remove3DBoxesRecursively(object);
    _releaseSignals(object);
}

void ObjectSet::_remove(SPObject *object) {
    _disconnect(object);
    _container.get<hashed>().erase(object);
}

void ObjectSet::_add(SPObject *object) {
    _releaseConnections[object] = object->connectRelease(sigc::hide_return(sigc::mem_fun(*this, &ObjectSet::remove)));
    _container.push_back(object);
    _add3DBoxesRecursively(object);
    _connectSignals(object);
}

void ObjectSet::_clear() {
    for (auto object: _container)
        _disconnect(object);
    _container.clear();
}

SPObject *ObjectSet::_getMutualAncestor(SPObject *object) {
    SPObject *o = object;

    bool flag = true;
    while (o->parent != nullptr) {
        for (auto &child: o->parent->children) {
            if(&child != o && !includes(&child)) {
                flag = false;
                break;
            }
        }
        if (!flag) {
            break;
        }
        o = o->parent;
    }
    return o;
}

void ObjectSet::_removeAncestorsFromSet(SPObject *object) {
    SPObject* o = object;
    while (o->parent != nullptr) {
        for (auto &child: o->parent->children) {
            if (&child != o) {
                _add(&child);
            }
        }
        if (includes(o->parent)) {
            _remove(o->parent);
            break;
        }
        o = o->parent;
    }
}

ObjectSet::~ObjectSet() {
    _clear();
}

void ObjectSet::toggle(SPObject *obj) {
    if (includes(obj)) {
        remove(obj);
    } else {
        add(obj);
    }
}

bool ObjectSet::isEmpty() {
    return _container.size() == 0;
}

SPObject *ObjectSet::single() {
    return _container.size() == 1 ? *_container.begin() : nullptr;
}

SPItem *ObjectSet::singleItem() {
    if (_container.size() == 1) {
        SPObject* obj = *_container.begin();
        if (SP_IS_ITEM(obj)) {
            return SP_ITEM(obj);
        }
    }

    return nullptr;
}

SPItem *ObjectSet::smallestItem(CompareSize compare) {
    return _sizeistItem(true, compare);
}

SPItem *ObjectSet::largestItem(CompareSize compare) {
    return _sizeistItem(false, compare);
}

SPItem *ObjectSet::_sizeistItem(bool sml, CompareSize compare) {
    auto items = this->items();
    gdouble max = sml ? 1e18 : 0;
    SPItem *ist = nullptr;

    for (auto *item : items) {
        Geom::OptRect obox = item->documentPreferredBounds();
        if (!obox || obox.empty()) {
            continue;
        }

        Geom::Rect bbox = *obox;

        gdouble size = compare == AREA ? bbox.area() :
                       (compare == VERTICAL ? bbox.height() : bbox.width());
        size = sml ? size : size * -1;
        if (size < max) {
            max = size;
            ist = item;
        }
    }

    return ist;
}

SPObjectRange ObjectSet::objects() {
    return SPObjectRange(_container.get<random_access>().begin(), _container.get<random_access>().end());
}

Inkscape::XML::Node *ObjectSet::singleRepr() {
    SPObject *obj = single();
    return obj ? obj->getRepr() : nullptr;
}

Inkscape::XML::Node *ObjectSet::topRepr() const
{
    auto const &nodes = const_cast<ObjectSet *>(this)->xmlNodes();

    if (nodes.empty()) {
        return nullptr;
    }

#ifdef __APPLE__
    // workaround for
    // static_assert(__is_cpp17_forward_iterator<_ForwardIterator>::value
    auto const n = std::vector<Inkscape::XML::Node *>(nodes.begin(), nodes.end());
#else
    auto const& n = nodes;
#endif

    return *std::max_element(n.begin(), n.end(), sp_repr_compare_position_bool);
}

void ObjectSet::set(SPObject *object, bool persist_selection_context) {
    _clear();
    _add(object);
    _emitChanged(persist_selection_context);
}

void ObjectSet::set(XML::Node *repr)
{
    if (document() && repr) {
        SPObject *obj = document()->getObjectByRepr(repr);
        assert(obj == document()->getObjectById(repr->attribute("id")));
        set(obj);
    }
}

int ObjectSet::setBetween(SPObject *obj_a, SPObject *obj_b)
{
    auto parent = obj_a->parent;
    if (!obj_b)
        obj_b = singleItem();

    if (!obj_a || !obj_b || parent != obj_b->parent) {
        return 0;
    } else if (obj_a == obj_b) {
        set(obj_a);
        return 1;
    }
    clear();

    int count = 0;
    int min = std::min(obj_a->getPosition(), obj_b->getPosition());
    int max = std::max(obj_a->getPosition(), obj_b->getPosition());
    for (int i = min ; i <= max ; i++) {
        if (auto child = parent->nthChild(i)) {
            count += add(child);
        }    
    }
    return count;
}


void ObjectSet::setReprList(std::vector<XML::Node*> const &list) {
    if(!document())
        return;
    clear();
    for (auto iter = list.rbegin(); iter != list.rend(); ++iter) {
#if 0
        // This can fail when pasting a clone into a new document
        SPObject *obj = document()->getObjectByRepr(*iter);
        assert(obj == document()->getObjectById((*iter)->attribute("id")));
#else
        SPObject *obj = document()->getObjectById((*iter)->attribute("id"));
#endif
        if (obj) {
            add(obj, true);
        }
    }
    _emitChanged();
}

void ObjectSet::enforceIds()
{
    bool idAssigned = false;
    auto items = this->items();
    for (auto *item : items) {
        if (!item->getId()) {
            // Selected object does not have an ID, so assign it a unique ID
            gchar *id = sp_object_get_unique_id(item, nullptr);
            item->setAttribute("id", id);
            g_free(id);
            idAssigned = true;
        }
    }
    if (idAssigned) {
        SPDocument *document = _desktop->getDocument();
        if (document) {
            document->setModifiedSinceSave(true);
        }
    }
}

Geom::OptRect ObjectSet::bounds(SPItem::BBoxType type) const
{
    return (type == SPItem::GEOMETRIC_BBOX) ?
           geometricBounds() : visualBounds();
}

Geom::OptRect ObjectSet::geometricBounds() const
{
    auto items = const_cast<ObjectSet *>(this)->items();

    Geom::OptRect bbox;
    for (auto *item : items) {
        bbox.unionWith(item->desktopGeometricBounds());
    }
    return bbox;
}

Geom::OptRect ObjectSet::visualBounds() const
{
    auto items = const_cast<ObjectSet *>(this)->items();

    Geom::OptRect bbox;
    for (auto *item : items) {
        bbox.unionWith(item->desktopVisualBounds());
    }
    return bbox;
}

Geom::OptRect ObjectSet::preferredBounds() const
{
    if (Inkscape::Preferences::get()->getInt("/tools/bounding_box") == 0) {
        return bounds(SPItem::VISUAL_BBOX);
    } else {
        return bounds(SPItem::GEOMETRIC_BBOX);
    }
}

Geom::OptRect ObjectSet::documentBounds(SPItem::BBoxType type) const
{
    Geom::OptRect bbox;
    auto items = const_cast<ObjectSet *>(this)->items();
    if (items.empty()) return bbox;

    for (auto *item : items) {
        bbox |= item->documentBounds(type);
    }

    return bbox;
}

// If we have a selection of multiple items, then the center of the first item
// will be returned; this is also the case in SelTrans::centerRequest()
std::optional<Geom::Point> ObjectSet::center() const {
    auto items = const_cast<ObjectSet *>(this)->items();
    if (!items.empty()) {
        SPItem *first = items.back(); // from the first item in selection
        if (first->isCenterSet()) { // only if set explicitly
            return first->getCenter();
        }
    }
    Geom::OptRect bbox = preferredBounds();
    if (bbox) {
        return bbox->midpoint();
    } else {
        return std::optional<Geom::Point>();
    }
}

std::list<Persp3D *> const ObjectSet::perspList() {
    std::list<Persp3D *> pl;
    for (auto & _3dboxe : _3dboxes) {
        Persp3D *persp = _3dboxe->get_perspective();
        if (std::find(pl.begin(), pl.end(), persp) == pl.end())
            pl.push_back(persp);
    }
    return pl;
}

std::list<SPBox3D *> const ObjectSet::box3DList(Persp3D *persp) {
    std::list<SPBox3D *> boxes;
    if (persp) {
        for (auto box : _3dboxes) {
            if (persp == box->get_perspective()) {
                boxes.push_back(box);
            }
        }
    } else {
        boxes = _3dboxes;
    }
    return boxes;
}

void ObjectSet::_add3DBoxesRecursively(SPObject *obj) {
    std::list<SPBox3D *> boxes = SPBox3D::extract_boxes(obj);

    for (auto box : boxes) {
        _3dboxes.push_back(box);
    }
}

void ObjectSet::_remove3DBoxesRecursively(SPObject *obj) {
    std::list<SPBox3D *> boxes = SPBox3D::extract_boxes(obj);

    for (auto box : boxes) {
        std::list<SPBox3D *>::iterator b = std::find(_3dboxes.begin(), _3dboxes.end(), box);
        if (b == _3dboxes.end()) {
            g_print ("Warning! Trying to remove unselected box from selection.\n");
            return;
        }
        _3dboxes.erase(b);
    }
}

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
