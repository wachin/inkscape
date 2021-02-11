// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
/*
 * Inkscape::GC::Finalized - mixin for GC-managed objects with non-trivial
 *                           destructors
 *
 * Copyright 2006 MenTaLguY <mental@rydia.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * See the file COPYING for details.
 *
 */

#include <typeinfo>
#include "debug/simple-event.h"
#include "debug/event-tracker.h"
#include "util/format.h"
#include "gc-finalized.h"

namespace Inkscape {

namespace GC {

namespace {

// workaround for g++ 4.0.2
typedef Debug::SimpleEvent<Debug::Event::FINALIZERS> BaseEvent;

class FinalizerEvent : public BaseEvent {
public:
    FinalizerEvent(Finalized *object)
    : BaseEvent("gc-finalizer")
    {
        _addProperty("base", Util::format("%p", Core::base(object)).pointer());
        _addProperty("pointer", Util::format("%p", object).pointer());
        _addProperty("class", typeid(*object).name());
    }
};

}

void Finalized::_invoke_dtor(void *base, void *offset) {
    Finalized *object=_unoffset(base, offset);
    Debug::EventTracker<FinalizerEvent> tracker(object);
    object->~Finalized();
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
