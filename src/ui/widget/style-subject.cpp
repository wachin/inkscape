// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2007 MenTaLguY <mental@rydia.net>
 *   Abhishek Sharma
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "style-subject.h"

#include "desktop.h"
#include "desktop-style.h"
#include "selection.h"

#include "xml/sp-css-attr.h"

namespace Inkscape {
namespace UI {
namespace Widget {

StyleSubject::StyleSubject() : _desktop(nullptr) {
}

StyleSubject::~StyleSubject() {
    setDesktop(nullptr);
}

void StyleSubject::setDesktop(SPDesktop *desktop) {
    if (desktop != _desktop) {
        if (desktop) {
            GC::anchor(desktop);
        }
        if (_desktop) {
            GC::release(_desktop);
        }
        _desktop = desktop;
        _afterDesktopSwitch(desktop);
        _emitChanged();
    }
}

StyleSubject::Selection::Selection() = default;

StyleSubject::Selection::~Selection() = default;

Inkscape::Selection *StyleSubject::Selection::_getSelection() const {
    SPDesktop *desktop = getDesktop();
    if (desktop) {
        return desktop->getSelection();
    } else {
        return nullptr;
    }
}

std::vector<SPObject*> StyleSubject::Selection::list() {
    Inkscape::Selection *selection = _getSelection();
    if(selection) {
        return std::vector<SPObject *>(selection->objects().begin(), selection->objects().end());
    }

    return std::vector<SPObject*>();
}

Geom::OptRect StyleSubject::Selection::getBounds(SPItem::BBoxType type) {
    Inkscape::Selection *selection = _getSelection();
    if (selection) {
        return selection->bounds(type);
    } else {
        return Geom::OptRect();
    }
}

int StyleSubject::Selection::queryStyle(SPStyle *query, int property) {
    SPDesktop *desktop = getDesktop();
    if (desktop) {
        return sp_desktop_query_style(desktop, query, property);
    } else {
        return QUERY_STYLE_NOTHING;
    }
}

void StyleSubject::Selection::_afterDesktopSwitch(SPDesktop *desktop) {
    _sel_changed.disconnect();
    _subsel_changed.disconnect();
    _sel_modified.disconnect();
    if (desktop) {
        _subsel_changed = desktop->connectToolSubselectionChanged(sigc::hide(sigc::mem_fun(*this, &Selection::_emitChanged)));
        Inkscape::Selection *selection = desktop->getSelection();
        if (selection) {
            _sel_changed = selection->connectChanged(sigc::hide(sigc::mem_fun(*this, &Selection::_emitChanged)));
            _sel_modified = selection->connectModified(sigc::mem_fun(*this, &Selection::_emitModified));
        }
    }
}

void StyleSubject::Selection::setCSS(SPCSSAttr *css) {
    SPDesktop *desktop = getDesktop();
    if (desktop) {
        sp_desktop_set_style(desktop, css);
    }
}

StyleSubject::CurrentLayer::CurrentLayer() {
    _element = nullptr;
}

StyleSubject::CurrentLayer::~CurrentLayer() = default;

void StyleSubject::CurrentLayer::_setLayer(SPObject *layer) {
    _layer_release.disconnect();
    _layer_modified.disconnect();
    if (_element) {
        sp_object_unref(_element, nullptr);
    }
    _element = layer;
    if (layer) {
        sp_object_ref(layer, nullptr);
        _layer_release = layer->connectRelease(sigc::hide(sigc::bind(sigc::mem_fun(*this, &CurrentLayer::_setLayer), (SPObject *)nullptr)));
        _layer_modified = layer->connectModified(sigc::hide(sigc::hide(sigc::mem_fun(*this, &CurrentLayer::_emitChanged))));
    }
    _emitChanged();
}

SPObject *StyleSubject::CurrentLayer::_getLayer() const {
    return _element;
}

SPObject *StyleSubject::CurrentLayer::_getLayerSList() const {
        return _element;

}

std::vector<SPObject*> StyleSubject::CurrentLayer::list(){
    std::vector<SPObject*> list;
    list.push_back(_element);
    return list;
}

Geom::OptRect StyleSubject::CurrentLayer::getBounds(SPItem::BBoxType type) {
    SPObject *layer = _getLayer();
    if (layer && SP_IS_ITEM(layer)) {
        return SP_ITEM(layer)->desktopBounds(type);
    } else {
        return Geom::OptRect();
    }
}

int StyleSubject::CurrentLayer::queryStyle(SPStyle *query, int property) {
	std::vector<SPItem*> list;
    SPObject* i=_getLayerSList();
    if (i) {
		list.push_back((SPItem*)i);
        return sp_desktop_query_style_from_list(list, query, property);
    } else {
        return QUERY_STYLE_NOTHING;
    }
}

void StyleSubject::CurrentLayer::setCSS(SPCSSAttr *css) {
    SPObject *layer = _getLayer();
    if (layer) {
        sp_desktop_apply_css_recursive(layer, css, true);
    }
}

void StyleSubject::CurrentLayer::_afterDesktopSwitch(SPDesktop *desktop) {
    _layer_switched.disconnect();
    if (desktop) {
        _layer_switched = desktop->connectCurrentLayerChanged(sigc::mem_fun(*this, &CurrentLayer::_setLayer));
        _setLayer(desktop->currentLayer());
    } else {
        _setLayer(nullptr);
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
