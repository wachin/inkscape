// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_CLIPPATH_H
#define SEEN_SP_CLIPPATH_H

/*
 * SVG <clipPath> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2001-2002 authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstdio>
#include "sp-object-group.h"
#include "display/drawing.h"
#include "display/drawing-group.h"
#include "uri-references.h"
#include "xml/node.h"

namespace Inkscape {

class Drawing;
class DrawingItem;

} // namespace Inkscape


struct SPClipPathView {
    SPClipPathView *next;
    unsigned int key;
    Inkscape::DrawingItem *arenaitem;
    Geom::OptRect bbox;
};

class SPClipPath : public SPObjectGroup {
public:
	SPClipPath();
	~SPClipPath() override;

    class Reference;

    unsigned int clipPathUnits_set : 1;
    unsigned int clipPathUnits : 1;

    SPClipPathView *display;
    static char const *create(std::vector<Inkscape::XML::Node*> &reprs, SPDocument *document);
    //static GType sp_clippath_get_type(void);

    Inkscape::DrawingItem *show(Inkscape::Drawing &drawing, unsigned int key);
    void hide(unsigned int key);

    void setBBox(unsigned int key, Geom::OptRect const &bbox);
    Geom::OptRect geometricBounds(Geom::Affine const &transform);

protected:
    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	void release() override;

	void child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) override;

	void set(SPAttr key, char const* value) override;

	void update(SPCtx* ctx, unsigned int flags) override;
	void modified(unsigned int flags) override;

	Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags) override;
};

MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_CLIPPATH, SPClipPath)

class SPClipPathReference : public Inkscape::URIReference {
public:
    SPClipPathReference(SPObject *obj) : URIReference(obj) {}
    SPClipPath *getObject() const {
        return static_cast<SPClipPath *>(URIReference::getObject());
    }

protected:
    /**
     * If the owner element of this reference (the element with <... clippath="...">)
     * is a child of the clippath it refers to, return false.
     * \return false if obj is not a clippath or if obj is a parent of this
     *         reference's owner element.  True otherwise.
     */
    bool _acceptObject(SPObject *obj) const override {
        if (!SP_IS_CLIPPATH(obj)) {
            return false;
        }
        SPObject * const owner = this->getOwner();
        if (!URIReference::_acceptObject(obj)) {
            //XML Tree being used directly here while it shouldn't be...
            Inkscape::XML::Node * const owner_repr = owner->getRepr();
            //XML Tree being used directly here while it shouldn't be...
            Inkscape::XML::Node * const obj_repr = obj->getRepr();
            char const * owner_name = "";
            char const * owner_clippath = "";
            char const * obj_name = "";
            char const * obj_id = "";
            if (owner_repr != nullptr) {
                owner_name = owner_repr->name();
                owner_clippath = owner_repr->attribute("clippath");
            }
            if (obj_repr != nullptr) {
                obj_name = obj_repr->name();
                obj_id = obj_repr->attribute("id");
            }
            printf("WARNING: Ignoring recursive clippath reference "
                      "<%s clippath=\"%s\"> in <%s id=\"%s\">",
                      owner_name, owner_clippath,
                      obj_name, obj_id);
            return false;
        }
        return true;
    }
};

#endif // SEEN_SP_CLIPPATH_H

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
