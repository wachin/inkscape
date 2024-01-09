// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <clipPath> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2001-2002 authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>
#include <string>

#include "xml/repr.h"

#include "enums.h"
#include "attributes.h"
#include "document.h"
#include "style.h"

#include <2geom/transforms.h>

#include "sp-clippath.h"
#include "sp-item.h"
#include "sp-defs.h"

#include "display/drawing-item.h"
#include "display/drawing-group.h"

SPClipPath::SPClipPath()
{
    clipPathUnits_set = false;
    clipPathUnits = SP_CONTENT_UNITS_USERSPACEONUSE;
}

SPClipPath::~SPClipPath() = default;

void SPClipPath::build(SPDocument *doc, Inkscape::XML::Node *repr)
{
    SPObjectGroup::build(doc, repr);

    readAttr(SPAttr::STYLE);
    readAttr(SPAttr::CLIPPATHUNITS);

    doc->addResource("clipPath", this);
}

void SPClipPath::release()
{
    if (document) {
        document->removeResource("clipPath", this);
    }

    views.clear();

    SPObjectGroup::release();
}

void SPClipPath::set(SPAttr key, char const *value)
{
    switch (key) {
        case SPAttr::CLIPPATHUNITS:
            clipPathUnits = SP_CONTENT_UNITS_USERSPACEONUSE;
            clipPathUnits_set = false;
            
            if (value) {
                if (!std::strcmp(value, "userSpaceOnUse")) {
                    clipPathUnits_set = true;
                } else if (!std::strcmp(value, "objectBoundingBox")) {
                    clipPathUnits = SP_CONTENT_UNITS_OBJECTBOUNDINGBOX;
                    clipPathUnits_set = true;
                }
            }
            
            requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        default:
            if (SP_ATTRIBUTE_IS_CSS(key)) {
                style->clear(key);
                requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
            } else {
                SPObjectGroup::set(key, value);
            }
            break;
    }
}

void SPClipPath::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref)
{
    SPObjectGroup::child_added(child, ref);

    if (auto item = cast<SPItem>(document->getObjectByRepr(child))) {
        for (auto &v : views) {
            auto ac = item->invoke_show(v.drawingitem->drawing(), v.key, SP_ITEM_REFERENCE_FLAGS);
            if (ac) {
                v.drawingitem->prependChild(ac);
            }
        }
    }
}

void SPClipPath::update(SPCtx *ctx, unsigned flags)
{
    auto const cflags = cascade_flags(flags);

    for (auto c : childList(true)) {
        if (cflags || (c->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            c->updateDisplay(ctx, cflags);
        }
        sp_object_unref(c);
    }

    for (auto &v : views) {
        update_view(v);
    }
}

void SPClipPath::update_view(View &v)
{
    if (clipPathUnits == SP_CONTENT_UNITS_OBJECTBOUNDINGBOX && v.bbox) {
        v.drawingitem->setChildTransform(Geom::Scale(v.bbox->dimensions()) * Geom::Translate(v.bbox->min()));
    } else {
        v.drawingitem->setChildTransform(Geom::identity());
    }
}

void SPClipPath::modified(unsigned flags)
{
    auto const cflags = cascade_flags(flags);

    for (auto c : childList(true)) {
        if (cflags || (c->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            c->emitModified(cflags);
        }
        sp_object_unref(c);
    }
}

Inkscape::XML::Node *SPClipPath::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned flags)
{
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:clipPath");
    }

    SPObjectGroup::write(xml_doc, repr, flags);

    return repr;
}

Inkscape::DrawingItem *SPClipPath::show(Inkscape::Drawing &drawing, unsigned key, Geom::OptRect const &bbox)
{
    views.emplace_back(make_drawingitem<Inkscape::DrawingGroup>(drawing), bbox, key);
    auto &v = views.back();
    auto root = v.drawingitem.get();

    for (auto &child : children) {
        if (auto item = cast<SPItem>(&child)) {
            auto ac = item->invoke_show(drawing, key, SP_ITEM_REFERENCE_FLAGS);
            if (ac) {
                // The order is not important in clippath.
                root->appendChild(ac);
            }
        }
    }

    root->setStyle(style);

    update_view(v);

    return root;
}

void SPClipPath::hide(unsigned key)
{
    for (auto &child : children) {
        if (auto item = cast<SPItem>(&child)) {
            item->invoke_hide(key);
        }
    }

    auto it = std::find_if(views.begin(), views.end(), [=] (auto &v) {
        return v.key == key;
    });

    if (it == views.end()) {
        return;
    }

    views.erase(it);
}

void SPClipPath::setBBox(unsigned key, Geom::OptRect const &bbox)
{
    auto it = std::find_if(views.begin(), views.end(), [=] (auto &v) {
        return v.key == key;
    });
    assert(it != views.end());
    auto &v = *it;

    v.bbox = bbox;
    update_view(v);
}

Geom::OptRect SPClipPath::geometricBounds(Geom::Affine const &transform) const
{
    Geom::OptRect bbox;
    for (auto &child : children) {
        if (auto item = cast<SPItem>(&child)) {
            bbox.unionWith(item->geometricBounds(item->transform * transform));
        }
    }
    return bbox;
}

// Create a mask element (using passed elements), add it to <defs>
char const *SPClipPath::create(std::vector<Inkscape::XML::Node*> &reprs, SPDocument *document)
{
    auto defsrepr = document->getDefs()->getRepr();

    auto xml_doc = document->getReprDoc();
    auto repr = xml_doc->createElement("svg:clipPath");
    repr->setAttribute("clipPathUnits", "userSpaceOnUse");

    defsrepr->appendChild(repr);
    auto id = repr->attribute("id");
    auto clip_path_object = document->getObjectById(id);

    for (auto node : reprs) {
        clip_path_object->appendChildRepr(node);
    }

    Inkscape::GC::release(repr);
    return id;
}

SPClipPath::View::View(DrawingItemPtr<Inkscape::DrawingGroup> drawingitem, Geom::OptRect const &bbox, unsigned key)
    : drawingitem(std::move(drawingitem))
    , bbox(bbox)
    , key(key) {}

bool SPClipPathReference::_acceptObject(SPObject *obj) const
{
    if (!is<SPClipPath>(obj)) {
        return false;
    }

    if (URIReference::_acceptObject(obj)) {
        return true;
    }

    auto const owner = getOwner();
    //XML Tree being used directly here while it shouldn't be...
    auto const owner_repr = owner->getRepr();
    //XML Tree being used directly here while it shouldn't be...
    auto const obj_repr = obj->getRepr();
    char const *owner_name = "";
    char const *owner_clippath = "";
    char const *obj_name = "";
    char const *obj_id = "";
    if (owner_repr) {
        owner_name = owner_repr->name();
        owner_clippath = owner_repr->attribute("clippath");
    }
    if (obj_repr) {
        obj_name = obj_repr->name();
        obj_id = obj_repr->attribute("id");
    }
    std::printf("WARNING: Ignoring recursive clippath reference "
                "<%s clippath=\"%s\"> in <%s id=\"%s\">",
                owner_name, owner_clippath,
                obj_name, obj_id);

    return false;
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
