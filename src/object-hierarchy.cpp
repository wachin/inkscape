// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Object hierarchy implementation.
 *
 * Authors:
 *   MenTaLguY <mental@rydia.net>
 *
 * Copyright (C) 2004 MenTaLguY
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstdio>

#include "object-hierarchy.h"

#include "object/sp-object.h"

namespace Inkscape {

ObjectHierarchy::ObjectHierarchy(SPObject *top) {
    if (top) {
        _addBottom(top);
    }
}

ObjectHierarchy::~ObjectHierarchy() {
    _clear();
}

void ObjectHierarchy::clear() {
    _clear();
    _changed_signal.emit(nullptr, nullptr);
}

void ObjectHierarchy::setTop(SPObject *object) {
    if (object == nullptr) { printf("Assertion object != NULL failed\n"); return; }

    if ( top() == object ) {
        return;
    }

    if (!top()) {
        _addTop(object);
    } else if (object->isAncestorOf(top())) {
        _addTop(object, top());
    } else if ( object == bottom() || object->isAncestorOf(bottom()) ) {
        _trimAbove(object);
    } else {
        _clear();
        _addTop(object);
    }

    _changed_signal.emit(top(), bottom());
}

void ObjectHierarchy::_addTop(SPObject *senior, SPObject *junior) {
    assert(junior != NULL);
    assert(senior != NULL);

    SPObject *object = junior->parent;
    do {
        _addTop(object);
        object = object->parent;
    } while ( object != senior );
}

void ObjectHierarchy::_addTop(SPObject *object) {
    assert(object != NULL);
    _hierarchy.push_back(_attach(object));
    _added_signal.emit(object);
}

void ObjectHierarchy::_trimAbove(SPObject *limit) {
    while ( !_hierarchy.empty() && _hierarchy.back().object != limit ) {
        SPObject *object=_hierarchy.back().object;

        sp_object_ref(object, nullptr);
        _detach(_hierarchy.back());
        _hierarchy.pop_back();
        _removed_signal.emit(object);
        sp_object_unref(object, nullptr);
    }
}

void ObjectHierarchy::setBottom(SPObject *object) {
    if (object == nullptr) { printf("assertion object != NULL failed\n"); return; }

    if ( bottom() == object ) {
        return;
    }

    if (!top()) {
        _addBottom(object);
    } else if (bottom()->isAncestorOf(object)) {
        _addBottom(bottom(), object);
    } else if ( top() == object ) {
        _trimBelow(top());
    } else if (top()->isAncestorOf(object)) {
        if (object->isAncestorOf(bottom())) {
            _trimBelow(object);
        } else { // object is a sibling or cousin of bottom()
            SPObject *saved_top=top();
            sp_object_ref(saved_top, nullptr);
            _clear();
            _addBottom(saved_top);
            _addBottom(saved_top, object);
            sp_object_unref(saved_top, nullptr);
        }
    } else {
        _clear();
        _addBottom(object);
    }

    _changed_signal.emit(top(), bottom());
}

void ObjectHierarchy::_trimBelow(SPObject *limit) {
    while ( !_hierarchy.empty() && _hierarchy.front().object != limit ) {
        SPObject *object=_hierarchy.front().object;
        sp_object_ref(object, nullptr);
        _detach(_hierarchy.front());
        _hierarchy.pop_front();
        _removed_signal.emit(object);
        sp_object_unref(object, nullptr);
    }
}

void ObjectHierarchy::_addBottom(SPObject *senior, SPObject *junior) {
    assert(junior != NULL);
    assert(senior != NULL);

    if ( junior != senior ) {
        _addBottom(senior, junior->parent);
        _addBottom(junior);
    }
}

void ObjectHierarchy::_addBottom(SPObject *object) {
    assert(object != NULL);
    _hierarchy.push_front(_attach(object));
    _added_signal.emit(object);
}

void ObjectHierarchy::_trim_for_release(SPObject *object) {
    this->_trimBelow(object);
    assert(!this->_hierarchy.empty());
    assert(this->_hierarchy.front().object == object);

    sp_object_ref(object, nullptr);
    this->_detach(this->_hierarchy.front());
    this->_hierarchy.pop_front();
    this->_removed_signal.emit(object);
    sp_object_unref(object, nullptr);

    this->_changed_signal.emit(this->top(), this->bottom());
}

ObjectHierarchy::Record ObjectHierarchy::_attach(SPObject *object) {
    sp_object_ref(object, nullptr);
    sigc::connection connection
      = object->connectRelease(
          sigc::mem_fun(*this, &ObjectHierarchy::_trim_for_release)
        );
    return Record(object, connection);
}

void ObjectHierarchy::_detach(ObjectHierarchy::Record &rec) {
    rec.connection.disconnect();
    sp_object_unref(rec.object, nullptr);
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
