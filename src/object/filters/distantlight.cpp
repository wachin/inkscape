// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <fedistantlight> implementation.
 */
/*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Niko Kiirala <niko@kiirala.com>
 *   Jean-Rene Reinhard <jr@komite.net>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006,2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h>

// In same directory
#include "distantlight.h"
#include "diffuselighting.h"
#include "specularlighting.h"

#include "attributes.h"
#include "document.h"

#include "xml/repr.h"

SPFeDistantLight::SPFeDistantLight()
    : SPObject(), azimuth(0), azimuth_set(FALSE), elevation(0), elevation_set(FALSE) {
}

SPFeDistantLight::~SPFeDistantLight() = default;

/**
 * Reads the Inkscape::XML::Node, and initializes SPDistantLight variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void SPFeDistantLight::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPObject::build(document, repr);

    //Read values of key attributes from XML nodes into object.
    this->readAttr(SPAttr::AZIMUTH);
    this->readAttr(SPAttr::ELEVATION);

//is this necessary?
    document->addResource("fedistantlight", this);
}

/**
 * Drops any allocated memory.
 */
void SPFeDistantLight::release() {
    if ( this->document ) {
        // Unregister ourselves
        this->document->removeResource("fedistantlight", this);
    }

//TODO: release resources here
}

/**
 * Sets a specific value in the SPFeDistantLight.
 */
void SPFeDistantLight::set(SPAttr key, gchar const *value) {
    gchar *end_ptr;

    switch (key) {
    case SPAttr::AZIMUTH:
        end_ptr =nullptr;

        if (value) {
            this->azimuth = g_ascii_strtod(value, &end_ptr);

            if (end_ptr) {
                this->azimuth_set = TRUE;
            }
        }

        if (!value || !end_ptr) {
                this->azimuth_set = FALSE;
                this->azimuth = 0;
        }

        if (this->parent &&
                (SP_IS_FEDIFFUSELIGHTING(this->parent) ||
                 SP_IS_FESPECULARLIGHTING(this->parent))) {
            this->parent->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
        }
        break;
    case SPAttr::ELEVATION:
        end_ptr =nullptr;

        if (value) {
            this->elevation = g_ascii_strtod(value, &end_ptr);

            if (end_ptr) {
                this->elevation_set = TRUE;
            }
        }

        if (!value || !end_ptr) {
                this->elevation_set = FALSE;
                this->elevation = 0;
        }

        if (this->parent &&
                (SP_IS_FEDIFFUSELIGHTING(this->parent) ||
                 SP_IS_FESPECULARLIGHTING(this->parent))) {
            this->parent->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
        }
        break;
    default:
        // See if any parents need this value.
    	SPObject::set(key, value);
        break;
    }
}

/**
 *  * Receives update notifications.
 *   */
void SPFeDistantLight::update(SPCtx *ctx, guint flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        /* do something to trigger redisplay, updates? */
        this->readAttr(SPAttr::AZIMUTH);
        this->readAttr(SPAttr::ELEVATION);
    }

    SPObject::update(ctx, flags);
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* SPFeDistantLight::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
    if (!repr) {
        repr = this->getRepr()->duplicate(doc);
    }

    if (this->azimuth_set) {
        repr->setAttributeCssDouble("azimuth", this->azimuth);
    }

    if (this->elevation_set) {
        repr->setAttributeCssDouble("elevation", this->elevation);
    }

    SPObject::write(doc, repr, flags);

    return repr;
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
