// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <filter> implementation.
 */
/*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Niko Kiirala <niko@kiirala.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006,2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-filter.h"

#include <map>
#include <cstring>
#include <utility>
#include <vector>

#include <glibmm.h>
#include <2geom/transforms.h>

#include "bad-uri-exception.h"
#include "attributes.h"
#include "display/nr-filter.h"
#include "document.h"
#include "sp-filter-reference.h"
#include "filters/sp-filter-primitive.h"
#include "uri.h"
#include "xml/repr.h"

static void filter_ref_changed(SPObject *old_ref, SPObject *ref, SPFilter *filter);
static void filter_ref_modified(SPObject *href, guint flags, SPFilter *filter);


SPFilter::SPFilter()
    : SPObject(), filterUnits(SP_FILTER_UNITS_OBJECTBOUNDINGBOX), filterUnits_set(FALSE),
      primitiveUnits(SP_FILTER_UNITS_USERSPACEONUSE), primitiveUnits_set(FALSE),
      filterRes(NumberOptNumber()),
      _renderer(nullptr), _image_name(new std::map<gchar *, int, ltstr>), _image_number_next(0)
{
    this->href = new SPFilterReference(this);
    this->href->changedSignal().connect(sigc::bind(sigc::ptr_fun(filter_ref_changed), this));

    this->x = 0;
    this->y = 0;
    this->width = 0;
    this->height = 0;
    this->auto_region = true;

    this->_image_name->clear();
}

SPFilter::~SPFilter() = default;


/**
 * Reads the Inkscape::XML::Node, and initializes SPFilter variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void SPFilter::build(SPDocument *document, Inkscape::XML::Node *repr) {
    //Read values of key attributes from XML nodes into object.
    this->readAttr(SPAttr::STYLE); // struct not derived from SPItem, we need to do this ourselves.
    this->readAttr(SPAttr::FILTERUNITS);
    this->readAttr(SPAttr::PRIMITIVEUNITS);
    this->readAttr(SPAttr::X);
    this->readAttr(SPAttr::Y);
    this->readAttr(SPAttr::WIDTH);
    this->readAttr(SPAttr::HEIGHT);
    this->readAttr(SPAttr::AUTO_REGION);
    this->readAttr(SPAttr::FILTERRES);
    this->readAttr(SPAttr::XLINK_HREF);
    this->_refcount = 0;

	SPObject::build(document, repr);

//is this necessary?
    document->addResource("filter", this);
}

/**
 * Drops any allocated memory.
 */
void SPFilter::release() {
    if (this->document) {
        // Unregister ourselves
        this->document->removeResource("filter", this);
    }

//TODO: release resources here

    //release href
    if (this->href) {
        this->modified_connection.disconnect();
        this->href->detach();
        delete this->href;
        this->href = nullptr;
    }

    for (std::map<gchar *, int, ltstr>::const_iterator i = this->_image_name->begin() ; i != this->_image_name->end() ; ++i) {
        g_free(i->first);
    }

    delete this->_image_name;

    SPObject::release();
}

/**
 * Sets a specific value in the SPFilter.
 */
void SPFilter::set(SPAttr key, gchar const *value) {
    switch (key) {
        case SPAttr::FILTERUNITS:
            if (value) {
                if (!strcmp(value, "userSpaceOnUse")) {
                    this->filterUnits = SP_FILTER_UNITS_USERSPACEONUSE;
                } else {
                    this->filterUnits = SP_FILTER_UNITS_OBJECTBOUNDINGBOX;
                }

                this->filterUnits_set = TRUE;
            } else {
                this->filterUnits = SP_FILTER_UNITS_OBJECTBOUNDINGBOX;
                this->filterUnits_set = FALSE;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::PRIMITIVEUNITS:
            if (value) {
                if (!strcmp(value, "objectBoundingBox")) {
                    this->primitiveUnits = SP_FILTER_UNITS_OBJECTBOUNDINGBOX;
                } else {
                    this->primitiveUnits = SP_FILTER_UNITS_USERSPACEONUSE;
                }

                this->primitiveUnits_set = TRUE;
            } else {
                this->primitiveUnits = SP_FILTER_UNITS_USERSPACEONUSE;
                this->primitiveUnits_set = FALSE;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::X:
            this->x.readOrUnset(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::Y:
            this->y.readOrUnset(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::WIDTH:
            this->width.readOrUnset(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::HEIGHT:
            this->height.readOrUnset(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::AUTO_REGION:
            this->auto_region = (!value || strcmp(value, "false"));
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::FILTERRES:
            this->filterRes.set(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::XLINK_HREF:
            if (value) {
                try {
                    this->href->attach(Inkscape::URI(value));
                } catch (Inkscape::BadURIException &e) {
                    g_warning("%s", e.what());
                    this->href->detach();
                }
            } else {
                this->href->detach();
            }
            break;
        default:
            // See if any parents need this value.
        	SPObject::set(key, value);
            break;
    }
}


/**
 * Returns the number of references to the filter.
 */
guint SPFilter::getRefCount() {
	// NOTE: this is currently updated by sp_style_filter_ref_changed() in style.cpp
	return _refcount;
}

void SPFilter::modified(guint flags) {
    // We are not an LPE, do not update filter regions on load.
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        update_filter_all_regions();
    }
}

/**
 * Receives update notifications.
 */
void SPFilter::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {

        SPItemCtx *ictx = (SPItemCtx *) ctx;

        // Do here since we know viewport (Bounding box case handled during rendering)
        // Note: This only works for root viewport since this routine is not called after
        // setting a new viewport. A true fix requires a strategy like SPItemView or SPMarkerView.
        if(this->filterUnits == SP_FILTER_UNITS_USERSPACEONUSE) {
            this->calcDimsFromParentViewport(ictx, true);
        }
        /* do something to trigger redisplay, updates? */

    }

    // Update filter primitives in order to update filter primitive area
    // (SPObject::ActionUpdate is not actually used)
    unsigned childflags = flags;

    if (flags & SP_OBJECT_MODIFIED_FLAG) {
      childflags |= SP_OBJECT_PARENT_MODIFIED_FLAG;
    }
    childflags &= SP_OBJECT_MODIFIED_CASCADE;
    std::vector<SPObject*> l(this->childList(true, SPObject::ActionUpdate));
    for(SPObject* child: l){
        if( SP_IS_FILTER_PRIMITIVE( child ) ) {
            child->updateDisplay(ctx, childflags);
        }
        sp_object_unref(child);
    }

    SPObject::update(ctx, flags);
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* SPFilter::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
    // Original from sp-item-group.cpp
    if (flags & SP_OBJECT_WRITE_BUILD) {
        if (!repr) {
            repr = doc->createElement("svg:filter");
        }

        std::vector<Inkscape::XML::Node *> l;
        for (auto& child: children) {
            Inkscape::XML::Node *crepr = child.updateRepr(doc, nullptr, flags);

            if (crepr) {
                l.push_back(crepr);
            }
        }

        for (auto i=l.rbegin();i!=l.rend();++i) {
           repr->addChild(*i, nullptr);
            Inkscape::GC::release(*i);
        }
    } else {
        for (auto& child: children) {
            child.updateRepr(flags);
        }
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->filterUnits_set) {
        switch (this->filterUnits) {
            case SP_FILTER_UNITS_USERSPACEONUSE:
                repr->setAttribute("filterUnits", "userSpaceOnUse");
                break;
            default:
                repr->setAttribute("filterUnits", "objectBoundingBox");
                break;
        }
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->primitiveUnits_set) {
        switch (this->primitiveUnits) {
            case SP_FILTER_UNITS_OBJECTBOUNDINGBOX:
                repr->setAttribute("primitiveUnits", "objectBoundingBox");
                break;
            default:
                repr->setAttribute("primitiveUnits", "userSpaceOnUse");
                break;
        }
    }

    if (this->x._set) {
        repr->setAttributeSvgDouble("x", this->x.computed);
    } else {
        repr->removeAttribute("x");
    }

    if (this->y._set) {
        repr->setAttributeSvgDouble("y", this->y.computed);
    } else {
        repr->removeAttribute("y");
    }

    if (this->width._set) {
        repr->setAttributeSvgDouble("width", this->width.computed);
    } else {
        repr->removeAttribute("width");
    }

    if (this->height._set) {
        repr->setAttributeSvgDouble("height", this->height.computed);
    } else {
        repr->removeAttribute("height");
    }

    if (this->filterRes.getNumber()>=0) {
        auto tmp = this->filterRes.getValueString();
        repr->setAttribute("filterRes", tmp);
    } else {
        repr->removeAttribute("filterRes");
    }

    if (this->href->getURI()) {
        auto uri_string = this->href->getURI()->str();
        repr->setAttributeOrRemoveIfEmpty("xlink:href", uri_string);
    }

    SPObject::write(doc, repr, flags);

    return repr;
}

/**
 * Update the filter's region based on it's detectable href links
 *
 * Automatic region only updated if auto_region is false
 * and filterUnits is not UserSpaceOnUse
 */
void SPFilter::update_filter_all_regions()
{
    if (!this->auto_region || this->filterUnits == SP_FILTER_UNITS_USERSPACEONUSE)
        return;

    // Combine all items into one region for updating.
    Geom::OptRect opt_r;
    for (auto & obj : this->hrefList) {
        SPItem *item = dynamic_cast<SPItem *>(obj);
        opt_r.unionWith(this->get_automatic_filter_region(item));
    }
    if (opt_r) {
        Geom::Rect region = *opt_r;
        this->set_filter_region(region.left(), region.top(), region.width(), region.height());
    }
}

/**
 * Update the filter region based on the object's bounding box
 *
 * @param item - The item who's coords are used as the basis for the area.
 */
void SPFilter::update_filter_region(SPItem *item)
{
    if (!this->auto_region || this->filterUnits == SP_FILTER_UNITS_USERSPACEONUSE)
        return; // No adjustment for dead box

    auto region = this->get_automatic_filter_region(item);

    // Set the filter region into this filter object
    this->set_filter_region(region.left(), region.top(), region.width(), region.height());
}

/**
 * Generate a filter region based on the item and return it.
 *
 * @param item - The item who's coords are used as the basis for the area.
 */
Geom::Rect SPFilter::get_automatic_filter_region(SPItem *item)
{
    // Calling bbox instead of visualBound() avoids re-requesting filter regions
    Geom::OptRect v_box = item->bbox(Geom::identity(), SPItem::VISUAL_BBOX);
    Geom::OptRect g_box = item->bbox(Geom::identity(), SPItem::GEOMETRIC_BBOX);
    if (!v_box || !g_box) return Geom::Rect(); // No adjustment for dead box

    // Because the filter box is in geometric bounding box units, it must ALSO
    // take account of the visualBox, so even if the filter does NOTHING to the
    // size of an object, we must add the difference between the geometric and
    // visual boxes ourselves or find them cut off by renderers of all kinds.
    Geom::Rect inbox = *g_box;
    Geom::Rect outbox = *v_box;
    for(auto& primitive_obj: this->children) {
        auto primitive = dynamic_cast<SPFilterPrimitive *>(&primitive_obj);
        if (primitive) {
            // Update the region with the primitive's options
            outbox = primitive->calculate_region(outbox);
        }
    }

    // Include the original visual bounding-box in the result
    outbox.unionWith(v_box);
    // Scale outbox to width/height scale of input, this scales the geometric
    // into the visual bounding box requiring any changes to it to re-run this.
    outbox *= Geom::Translate(-inbox.left(), -inbox.top());
    outbox *= Geom::Scale(1/inbox.width(), 1/inbox.height());
    return outbox;
}

/**
 * Set the filter region attributes from a bounding box
 */
void SPFilter::set_filter_region(double x, double y, double width, double height)
{
    if (width != 0 && height != 0)
    {
        // TODO: set it in UserSpaceOnUse instead?
        auto repr = this->getRepr();
        repr->setAttributeSvgDouble("x", x);
        repr->setAttributeSvgDouble("y", y);
        repr->setAttributeSvgDouble("width", width);
        repr->setAttributeSvgDouble("height", height);
    }
}

/**
 * Check each filter primitive for conflicts with this object.
 */
bool SPFilter::valid_for(SPObject const *obj) const
{
    for(auto& primitive_obj: this->children) {
        auto primitive = dynamic_cast<SPFilterPrimitive const *>(&primitive_obj);
        if (primitive && !primitive->valid_for(obj)) {
            return false;
        }
    }
    return true;
}

/**
 * Gets called when the filter is (re)attached to another filter.
 */
static void
filter_ref_changed(SPObject *old_ref, SPObject *ref, SPFilter *filter)
{
    if (old_ref) {
        filter->modified_connection.disconnect();
    }

    if ( SP_IS_FILTER(ref)
         && ref != filter )
    {
        filter->modified_connection =
            ref->connectModified(sigc::bind(sigc::ptr_fun(&filter_ref_modified), filter));
    }

    filter_ref_modified(ref, 0, filter);
}

static void filter_ref_modified(SPObject */*href*/, guint /*flags*/, SPFilter *filter)
{
    filter->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for child_added event.
 */
void SPFilter::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
	SPObject::child_added(child, ref);

    this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

/**
 * Callback for remove_child event.
 */
void SPFilter::remove_child(Inkscape::XML::Node *child) {
	SPObject::remove_child(child);

	this->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFilter::build_renderer(Inkscape::Filters::Filter *nr_filter)
{
    g_assert(nr_filter != nullptr);

    this->_renderer = nr_filter;

    nr_filter->set_filter_units(this->filterUnits);
    nr_filter->set_primitive_units(this->primitiveUnits);
    nr_filter->set_x(this->x);
    nr_filter->set_y(this->y);
    nr_filter->set_width(this->width);
    nr_filter->set_height(this->height);

    if (this->filterRes.getNumber() >= 0) {
        if (this->filterRes.getOptNumber() >= 0) {
            nr_filter->set_resolution(this->filterRes.getNumber(),
                                      this->filterRes.getOptNumber());
        } else {
            nr_filter->set_resolution(this->filterRes.getNumber());
        }
    }

    nr_filter->clear_primitives();
    for(auto& primitive_obj: this->children) {
        if (SP_IS_FILTER_PRIMITIVE(&primitive_obj)) {
            SPFilterPrimitive *primitive = SP_FILTER_PRIMITIVE(&primitive_obj);
            g_assert(primitive != nullptr);

//            if (((SPFilterPrimitiveClass*) G_OBJECT_GET_CLASS(primitive))->build_renderer) {
//                ((SPFilterPrimitiveClass *) G_OBJECT_GET_CLASS(primitive))->build_renderer(primitive, nr_filter);
//            } else {
//                g_warning("Cannot build filter renderer: missing builder");
//            }  // CPPIFY: => FilterPrimitive should be abstract.
            primitive->build_renderer(nr_filter);
        }
    }
}

int SPFilter::primitive_count() const {
    int count = 0;

    for(const auto& primitive_obj: this->children) {
        if (SP_IS_FILTER_PRIMITIVE(&primitive_obj)) {
            count++;
        }
    }

    return count;
}

int SPFilter::get_image_name(gchar const *name) const {
    std::map<gchar *, int, ltstr>::iterator result = this->_image_name->find(const_cast<gchar*>(name));
    if (result == this->_image_name->end()) return -1;
    else return (*result).second;
}

int SPFilter::set_image_name(gchar const *name) {
    int value = this->_image_number_next;
    this->_image_number_next++;
    gchar *name_copy = strdup(name);
    std::pair<gchar*,int> new_pair(name_copy, value);
    const std::pair<std::map<gchar*,int,ltstr>::iterator,bool> ret = this->_image_name->insert(new_pair);
    if (ret.second == false) {
        // The element is not inserted (because an element with the same key was already in the map) 
        // Therefore, free the memory allocated for the new entry:
        free(name_copy);

        return (*ret.first).second;
    }
    return value;
}

gchar const *SPFilter::name_for_image(int const image) const {
    switch (image) {
        case Inkscape::Filters::NR_FILTER_SOURCEGRAPHIC:
            return "SourceGraphic";
            break;
        case Inkscape::Filters::NR_FILTER_SOURCEALPHA:
            return "SourceAlpha";
            break;
        case Inkscape::Filters::NR_FILTER_BACKGROUNDIMAGE:
            return "BackgroundImage";
            break;
        case Inkscape::Filters::NR_FILTER_BACKGROUNDALPHA:
            return "BackgroundAlpha";
            break;
        case Inkscape::Filters::NR_FILTER_STROKEPAINT:
            return "StrokePaint";
            break;
        case Inkscape::Filters::NR_FILTER_FILLPAINT:
            return "FillPaint";
            break;
        case Inkscape::Filters::NR_FILTER_SLOT_NOT_SET:
        case Inkscape::Filters::NR_FILTER_UNNAMED_SLOT:
            return nullptr;
            break;
        default:
            for (std::map<gchar *, int, ltstr>::const_iterator i
                     = this->_image_name->begin() ;
                 i != this->_image_name->end() ; ++i) {
                if (i->second == image) {
                    return i->first;
                }
            }
    }
    return nullptr;
}

Glib::ustring SPFilter::get_new_result_name() const {
    int largest = 0;

    for(const auto& primitive_obj: this->children) {
        if (SP_IS_FILTER_PRIMITIVE(&primitive_obj)) {
            const Inkscape::XML::Node *repr = primitive_obj.getRepr();
            char const *result = repr->attribute("result");
            int index;
            if (result)
            {
                if (sscanf(result, "result%5d", &index) == 1)
                {
                    if (index > largest)
                    {
                        largest = index;
                    }
                }
            }
        }
    }

    return "result" + Glib::Ascii::dtostr(largest + 1);
}

bool ltstr::operator()(const char* s1, const char* s2) const
{
    return strcmp(s1, s2) < 0;
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
