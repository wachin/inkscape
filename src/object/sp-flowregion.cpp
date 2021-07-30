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
 */

#include <glibmm/i18n.h>

#include <xml/repr.h>
#include "display/curve.h"
#include "sp-shape.h"
#include "sp-text.h"
#include "sp-use.h"
#include "style.h"
#include "document.h"
#include "sp-title.h"
#include "sp-desc.h"

#include "sp-flowregion.h"

#include "livarot/Path.h"
#include "livarot/Shape.h"


static void         GetDest(SPObject* child,Shape **computed);


SPFlowregion::SPFlowregion() : SPItem() {
}

SPFlowregion::~SPFlowregion() {
	for (auto & it : this->computed) {
        delete it;
	}
}

void SPFlowregion::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
	SPItem::child_added(child, ref);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

void SPFlowregion::remove_child(Inkscape::XML::Node * child) {
	SPItem::remove_child(child);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


void SPFlowregion::update(SPCtx *ctx, unsigned int flags) {
    SPItemCtx *ictx = reinterpret_cast<SPItemCtx *>(ctx);
    SPItemCtx cctx = *ictx;

    unsigned childflags = flags;
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        childflags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    childflags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject*>l;

    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for (auto child:l) {
        g_assert(child != nullptr);
        SPItem *item = dynamic_cast<SPItem *>(child);

        if (childflags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            if (item) {
                SPItem const &chi = *item;
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, childflags);
            } else {
                child->updateDisplay(ctx, childflags);
            }
        }

        sp_object_unref(child);
    }

    SPItem::update(ctx, flags);

    this->UpdateComputed();
}

void SPFlowregion::UpdateComputed()
{
    for (auto & it : computed) {
        delete it;
    }
    computed.clear();

    for (auto& child: children) {
        Shape *shape = nullptr;
        GetDest(&child, &shape);
        computed.push_back(shape);
    }
}

void SPFlowregion::modified(guint flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *>l;

    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for (auto child:l) {
        g_assert(child != nullptr);

        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }

        sp_object_unref(child);
    }
}

Inkscape::XML::Node *SPFlowregion::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if (flags & SP_OBJECT_WRITE_BUILD) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowRegion");
        }

        std::vector<Inkscape::XML::Node *> l;
        for (auto& child: children) {
            if ( !dynamic_cast<SPTitle *>(&child) && !dynamic_cast<SPDesc *>(&child) ) {
                Inkscape::XML::Node *crepr = child.updateRepr(xml_doc, nullptr, flags);

                if (crepr) {
                    l.push_back(crepr);
                }
            }
        }

        for (auto i = l.rbegin(); i != l.rend(); ++i) {
            repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }

        for (auto& child: children) {
            if ( !dynamic_cast<SPTitle *>(&child) && !dynamic_cast<SPDesc *>(&child) ) {
                child.updateRepr(flags);
            }
        }
    }

    SPItem::write(xml_doc, repr, flags);

    this->UpdateComputed();  // copied from update(), see LP Bug 1339305

    return repr;
}

const char* SPFlowregion::typeName() const {
    return "text-flow";
}

const char* SPFlowregion::displayName() const {
    // TRANSLATORS: "Flow region" is an area where text is allowed to flow
    return _("Flow Region");
}

SPFlowregionExclude::SPFlowregionExclude() : SPItem() {
	this->computed = nullptr;
}

SPFlowregionExclude::~SPFlowregionExclude() {
    if (this->computed) {
        delete this->computed;
        this->computed = nullptr;
    }
}

void SPFlowregionExclude::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
	SPItem::child_added(child, ref);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/* fixme: hide (Lauris) */

void SPFlowregionExclude::remove_child(Inkscape::XML::Node * child) {
	SPItem::remove_child(child);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


void SPFlowregionExclude::update(SPCtx *ctx, unsigned int flags) {
    SPItemCtx *ictx = reinterpret_cast<SPItemCtx *>(ctx);
    SPItemCtx cctx = *ictx;

    SPItem::update(ctx, flags);

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject *> l;

    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for(auto child:l) {
        g_assert(child != nullptr);

        if (flags || (child->uflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            SPItem *item = dynamic_cast<SPItem *>(child);
            if (item) {
                SPItem const &chi = *item;
                cctx.i2doc = chi.transform * ictx->i2doc;
                cctx.i2vp = chi.transform * ictx->i2vp;
                child->updateDisplay((SPCtx *)&cctx, flags);
            } else {
                child->updateDisplay(ctx, flags);
            }
        }

        sp_object_unref(child);
    }

    this->UpdateComputed();
}


void SPFlowregionExclude::UpdateComputed()
{
    if (computed) {
        delete computed;
        computed = nullptr;
    }

    for (auto& child: children) {
        GetDest(&child, &computed);
    }
}

void SPFlowregionExclude::modified(guint flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        flags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }

    flags &= SP_OBJECT_MODIFIED_CASCADE;

    std::vector<SPObject*> l;

    for (auto& child: children) {
        sp_object_ref(&child);
        l.push_back(&child);
    }

    for (auto child:l) {
        g_assert(child != nullptr);

        if (flags || (child->mflags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG))) {
            child->emitModified(flags);
        }

        sp_object_unref(child);
    }
}

Inkscape::XML::Node *SPFlowregionExclude::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if (flags & SP_OBJECT_WRITE_BUILD) {
        if ( repr == nullptr ) {
            repr = xml_doc->createElement("svg:flowRegionExclude");
        }

        std::vector<Inkscape::XML::Node *> l;

        for (auto& child: children) {
            Inkscape::XML::Node *crepr = child.updateRepr(xml_doc, nullptr, flags);

            if (crepr) {
                l.push_back(crepr);
            }
        }

        for (auto i = l.rbegin(); i != l.rend(); ++i) { 
            repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }

    } else {
        for (auto& child: children) {
            child.updateRepr(flags);
        }
    }

    SPItem::write(xml_doc, repr, flags);

    return repr;
}

const char* SPFlowregionExclude::typeName() const {
    return "text-flow";
}

const char* SPFlowregionExclude::displayName() const {
	/* TRANSLATORS: A region "cut out of" a flow region; text is not allowed to flow inside the
	 * flow excluded region.  flowRegionExclude in SVG 1.2: see
	 * http://www.w3.org/TR/2004/WD-SVG12-20041027/flow.html#flowRegion-elem and
	 * http://www.w3.org/TR/2004/WD-SVG12-20041027/flow.html#flowRegionExclude-elem. */
	return _("Flow Excluded Region");
}

static void UnionShape(Shape *&base_shape, Shape const *add_shape)
{
    if (base_shape == nullptr)
        base_shape = new Shape;

    if (base_shape->hasEdges() == false) {
        base_shape->Copy(const_cast<Shape *>(add_shape));
    } else if (add_shape->hasEdges()) {
        Shape *temp = new Shape;
        temp->Booleen(const_cast<Shape *>(add_shape), base_shape, bool_op_union);
        delete base_shape;
        base_shape = temp;
    }
}

static void         GetDest(SPObject* child,Shape **computed)
{
    auto item = dynamic_cast<SPItem *>(child);
    if (item == nullptr)
        return;

    std::unique_ptr<SPCurve> curve;
    Geom::Affine tr_mat;

    SPObject* u_child = child;
    SPUse *use = dynamic_cast<SPUse *>(item);
    if ( use ) {
        u_child = use->child;
        tr_mat = use->getRelativeTransform(child->parent);
    } else {
        tr_mat = item->transform;
    }
    SPShape *shape = dynamic_cast<SPShape *>(u_child);
    if ( shape ) {
        if (!shape->curve()) {
            shape->set_shape();
        }
        curve = SPCurve::copy(shape->curve());
    } else {
        SPText *text = dynamic_cast<SPText *>(u_child);
        if ( text ) {
            curve = text->getNormalizedBpath();
        }
    }

	if ( curve ) {
		Path*   temp=new Path;
        temp->LoadPathVector(curve->get_pathvector(), tr_mat, true);
		Shape*  n_shp=new Shape;
		temp->Convert(0.25);
		temp->Fill(n_shp,0);
		Shape*  uncross=new Shape;
		SPStyle* style = u_child->style;
		if ( style && style->fill_rule.computed == SP_WIND_RULE_EVENODD ) {
			uncross->ConvertToShape(n_shp,fill_oddEven);
		} else {
			uncross->ConvertToShape(n_shp,fill_nonZero);
		}
		UnionShape(*computed, uncross);
		delete uncross;
		delete n_shp;
		delete temp;
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
