// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SP_TREF_H
#define SP_TREF_H

/** \file
 * SVG <tref> implementation, see sp-tref.cpp.
 * 
 * This file was created based on skeleton.h
 */
/*
 * Authors:
 *   Gail Banaszkiewicz <Gail.Banaszkiewicz@gmail.com>
 *
 * Copyright (C) 2007 Gail Banaszkiewicz
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-item.h"
#include "sp-tref-reference.h"
#include "text-tag-attributes.h"


/* tref base class */

class SPTRef : public SPItem {
public:
	SPTRef();
	~SPTRef() override;

    // Attributes that are used in the same way they would be in a tspan
    TextTagAttributes attributes;
    
    // Text stored in the xlink:href attribute
    char *href;
    
    // URI reference to original object
    SPTRefReference *uriOriginalRef;
    
    // Shortcut pointer to the child of the tref (which is a copy
    // of the character data stored at and/or below the node
    // referenced by uriOriginalRef)
    SPObject *stringChild;
    
    // The sigc connections for various notifications
    sigc::connection _delete_connection;
    sigc::connection _changed_connection;
    
    SPObject * getObjectReferredTo();
    SPObject const *getObjectReferredTo() const;

	void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	void release() override;
	void set(SPAttr key, char const* value) override;
	void update(SPCtx* ctx, unsigned int flags) override;
	void modified(unsigned int flags) override;
	Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, guint flags) override;

	Geom::OptRect bbox(Geom::Affine const &transform, SPItem::BBoxType type) const override;
        const char* typeName() const override;
        const char* displayName() const override;
	char* description() const override;
};

void sp_tref_update_text(SPTRef *tref);
bool sp_tref_reference_allowed(SPTRef *tref, SPObject *possible_ref);
bool sp_tref_fully_contained(SPObject *start_item, Glib::ustring::iterator &start, 
                             SPObject *end_item, Glib::ustring::iterator &end);
SPObject * sp_tref_convert_to_tspan(SPObject *item);

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_TREF, SPTRef)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_TREF, SPTRef)

#endif /* !SP_TREF_H */

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
