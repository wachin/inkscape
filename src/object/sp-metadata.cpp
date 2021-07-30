// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <metadata> implementation
 *
 * Authors:
 *   Kees Cook <kees@outflux.net>
 *
 * Copyright (C) 2004 Kees Cook
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-metadata.h"

#include <regex>

#include "xml/node-iterators.h"
#include "document.h"

#include "sp-item-group.h"
#include "sp-root.h"

#define noDEBUG_METADATA
#ifdef DEBUG_METADATA
# define debug(f, a...) { g_print("%s(%d) %s:", \
                                  __FILE__,__LINE__,__FUNCTION__); \
                          g_print(f, ## a); \
                          g_print("\n"); \
                        }
#else
# define debug(f, a...) /**/
#endif

/* Metadata base class */

SPMetadata::SPMetadata() : SPObject() {
}

SPMetadata::~SPMetadata() = default;

namespace {

void strip_ids_recursively(Inkscape::XML::Node *node) {
    using Inkscape::XML::NodeSiblingIterator;
    if ( node->type() == Inkscape::XML::NodeType::ELEMENT_NODE ) {
        node->removeAttribute("id");
    }
    for ( NodeSiblingIterator iter=node->firstChild() ; iter ; ++iter ) {
        strip_ids_recursively(iter);
    }
}

/**
 * Return true if the given metadata belongs to a CorelDraw layer.
 */
bool is_corel_layer_metadata(SPMetadata const &metadata)
{
    char const *id = metadata.getId();
    return id &&                                  //
           g_str_has_prefix(id, "CorelCorpID") && //
           g_str_has_suffix(id, "Corel-Layer");
}

/**
 * Get the label of a CorelDraw layer.
 */
std::string corel_layer_get_label(SPGroup const &layer)
{
    char const *id = layer.getId();
    if (id) {
        return std::regex_replace(id, std::regex("_x0020_"), " ");
    }
    return "<unnamed-corel-layer>";
}
}


void SPMetadata::build(SPDocument* doc, Inkscape::XML::Node* repr) {
    using Inkscape::XML::NodeSiblingIterator;

    debug("0x%08x",(unsigned int)this);

    /* clean up our mess from earlier versions; elements under rdf:RDF should not
     * have id= attributes... */
    static GQuark const rdf_root_name = g_quark_from_static_string("rdf:RDF");

    for ( NodeSiblingIterator iter=repr->firstChild() ; iter ; ++iter ) {
        if ( (GQuark)iter->code() == rdf_root_name ) {
            strip_ids_recursively(iter);
        }
    }

    SPObject::build(doc, repr);
}

void SPMetadata::release() {
    debug("0x%08x",(unsigned int)this);

    // handle ourself

    SPObject::release();
}

void SPMetadata::set(SPAttr key, const gchar* value) {
    debug("0x%08x %s(%u): '%s'",(unsigned int)this,
          sp_attribute_name(key),key,value);

    // see if any parents need this value
    SPObject::set(key, value);
}

void SPMetadata::update(SPCtx* /*ctx*/, unsigned int flags) {
    debug("0x%08x",(unsigned int)this);
    //SPMetadata *metadata = SP_METADATA(object);

    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {

        /* do something? */

        // Detect CorelDraw layers
        if (is_corel_layer_metadata(*this)) {
            auto layer = dynamic_cast<SPGroup *>(parent);
            if (layer && layer->layerMode() == SPGroup::GROUP) {
                layer->setLayerMode(SPGroup::LAYER);
                if (!layer->label()) {
                    layer->setLabel(corel_layer_get_label(*layer).c_str());
                }
            }
        }
    }

//    SPObject::onUpdate(ctx, flags);
}

Inkscape::XML::Node* SPMetadata::write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, guint flags) {
    debug("0x%08x",(unsigned int)this);

    if ( repr != this->getRepr() ) {
        if (repr) {
            repr->mergeFrom(this->getRepr(), "id");
        } else {
            repr = this->getRepr()->duplicate(doc);
        }
    }

    SPObject::write(doc, repr, flags);

    return repr;
}

/**
 * Retrieves the metadata object associated with a document.
 */
SPMetadata *sp_document_metadata(SPDocument *document)
{
    SPObject *nv;

    g_return_val_if_fail (document != nullptr, NULL);

    nv = sp_item_group_get_child_by_name( document->getRoot(), nullptr,
                                        "metadata");
    g_assert (nv != nullptr);

    return (SPMetadata *)nv;
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
