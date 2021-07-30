// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_MASK_H
#define SEEN_SP_MASK_H

/*
 * SVG <mask> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2003 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/rect.h>
#include "sp-object-group.h"
#include "uri-references.h"
#include "xml/node.h"

namespace Inkscape {

class Drawing;
class DrawingItem;


} // namespace Inkscape

struct SPMaskView {
	SPMaskView *next;
	unsigned int key;
	Inkscape::DrawingItem *arenaitem;
	Geom::OptRect bbox;
};

class SPMask : public SPObjectGroup {
public:
	SPMask();
	~SPMask() override;

	unsigned int maskUnits_set : 1;
	unsigned int maskUnits : 1;

	unsigned int maskContentUnits_set : 1;
	unsigned int maskContentUnits : 1;

	SPMaskView *display;

	Inkscape::DrawingItem *sp_mask_show(Inkscape::Drawing &drawing, unsigned int key);
	void sp_mask_hide(unsigned int key);

    Geom::OptRect geometricBounds(Geom::Affine const &transform);

    Geom::OptRect visualBounds(Geom::Affine const &transform) ;

	void sp_mask_set_bbox(unsigned int key, Geom::OptRect const &bbox);

protected:
	void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	void release() override;

	void child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) override;

	void set(SPAttr key, const char* value) override;

	void update(SPCtx* ctx, unsigned int flags) override;
	void modified(unsigned int flags) override;

	Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags) override;
};

MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_MASK, SPMask)

class SPMaskReference : public Inkscape::URIReference {
public:
	SPMaskReference(SPObject *obj) : URIReference(obj) {}
	SPMask *getObject() const {
		return static_cast<SPMask *>(URIReference::getObject());
	}
protected:
    /**
     * If the owner element of this reference (the element with <... mask="...">)
     * is a child of the mask it refers to, return false.
     * \return false if obj is not a mask or if obj is a parent of this
     *         reference's owner element.  True otherwise.
     */
	bool _acceptObject(SPObject *obj) const override {
		if (!SP_IS_MASK(obj)) {
		    return false;
	    }
	    SPObject * const owner = this->getOwner();
        if (!URIReference::_acceptObject(obj)) {
	  //XML Tree being used directly here while it shouldn't be...
	  Inkscape::XML::Node * const owner_repr = owner->getRepr();
	  //XML Tree being used directly here while it shouldn't be...
	  Inkscape::XML::Node * const obj_repr = obj->getRepr();
            char const * owner_name = "";
            char const * owner_mask = "";
            char const * obj_name = "";
            char const * obj_id = "";
            if (owner_repr != nullptr) {
                owner_name = owner_repr->name();
                owner_mask = owner_repr->attribute("mask");
            }
            if (obj_repr != nullptr) {
                obj_name = obj_repr->name();
                obj_id = obj_repr->attribute("id");
            }
            printf("WARNING: Ignoring recursive mask reference "
                      "<%s mask=\"%s\"> in <%s id=\"%s\">",
                      owner_name, owner_mask,
                      obj_name, obj_id);
            return false;
        }
        return true;
	}
};

const char *sp_mask_create (std::vector<Inkscape::XML::Node*> &reprs, SPDocument *document);

#endif // SEEN_SP_MASK_H
