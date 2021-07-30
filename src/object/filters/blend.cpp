// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * SVG <feBlend> implementation.
 *
 */
/*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *   Niko Kiirala <niko@kiirala.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006,2007 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>

#include "blend.h"

#include "attributes.h"

#include "display/nr-filter.h"

#include "object/sp-filter.h"

#include "xml/repr.h"


SPFeBlend::SPFeBlend()
    : SPFilterPrimitive(), blend_mode(SP_CSS_BLEND_NORMAL),
      in2(Inkscape::Filters::NR_FILTER_SLOT_NOT_SET)
{
}

SPFeBlend::~SPFeBlend() = default;

/**
 * Reads the Inkscape::XML::Node, and initializes SPFeBlend variables.  For this to get called,
 * our name must be associated with a repr via "sp_object_type_register".  Best done through
 * sp-object-repr.cpp's repr_name_entries array.
 */
void SPFeBlend::build(SPDocument *document, Inkscape::XML::Node *repr) {
    SPFilterPrimitive::build(document, repr);

    /*LOAD ATTRIBUTES FROM REPR HERE*/
    this->readAttr(SPAttr::MODE);
    this->readAttr(SPAttr::IN2);

    /* Unlike normal in, in2 is required attribute. Make sure, we can call
     * it by some name. */
    if (this->in2 == Inkscape::Filters::NR_FILTER_SLOT_NOT_SET ||
        this->in2 == Inkscape::Filters::NR_FILTER_UNNAMED_SLOT)
    {
        SPFilter *parent = SP_FILTER(this->parent);
        this->in2 = this->name_previous_out();
        repr->setAttribute("in2", parent->name_for_image(this->in2));
    }
}

/**
 * Drops any allocated memory.
 */
void SPFeBlend::release() {
	SPFilterPrimitive::release();
}

static  SPBlendMode sp_feBlend_readmode(gchar const *value) {
    if (!value) {
    	return SP_CSS_BLEND_NORMAL;
    }

    switch (value[0]) {
        case 'n':
            if (strncmp(value, "normal", 6) == 0)
                return SP_CSS_BLEND_NORMAL;
            break;
        case 'm':
            if (strncmp(value, "multiply", 8) == 0)
                return SP_CSS_BLEND_MULTIPLY;
            break;
        case 's':
            if (strncmp(value, "screen", 6) == 0)
                return SP_CSS_BLEND_SCREEN;
            if (strncmp(value, "saturation", 10) == 0)
                return SP_CSS_BLEND_SATURATION;
            break;
        case 'd':
            if (strncmp(value, "darken", 6) == 0)
                return SP_CSS_BLEND_DARKEN;
            if (strncmp(value, "difference", 10) == 0)
                return SP_CSS_BLEND_DIFFERENCE;
            break;
        case 'l':
            if (strncmp(value, "lighten", 7) == 0)
                return SP_CSS_BLEND_LIGHTEN;
            if (strncmp(value, "luminosity", 10) == 0)
                return SP_CSS_BLEND_LUMINOSITY;
            break;
        case 'o':
            if (strncmp(value, "overlay", 7) == 0)
                return SP_CSS_BLEND_OVERLAY;
            break;
        case 'c':
            if (strncmp(value, "color-dodge", 11) == 0)
                return SP_CSS_BLEND_COLORDODGE;
            if (strncmp(value, "color-burn", 10) == 0)
                return SP_CSS_BLEND_COLORBURN;
            if (strncmp(value, "color", 5) == 0)
                return SP_CSS_BLEND_COLOR;
            break;
        case 'h':
            if (strncmp(value, "hard-light", 10) == 0)
                return SP_CSS_BLEND_HARDLIGHT;
            if (strncmp(value, "hue", 3) == 0)
                return SP_CSS_BLEND_HUE;
            break;
        case 'e':
            if (strncmp(value, "exclusion", 10) == 0)
                return SP_CSS_BLEND_EXCLUSION;
        default:
            std::cout << "SPBlendMode: Unimplemented mode: " << value << std::endl;
            // do nothing by default
            break;
    }

    return SP_CSS_BLEND_NORMAL;
}

/**
 * Sets a specific value in the SPFeBlend.
 */
void SPFeBlend::set(SPAttr key, gchar const *value) {
    SPBlendMode mode;
    int input;

    switch(key) {
	/*DEAL WITH SETTING ATTRIBUTES HERE*/
        case SPAttr::MODE:
            mode = sp_feBlend_readmode(value);

            if (mode != this->blend_mode) {
                this->blend_mode = mode;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        case SPAttr::IN2:
            input = this->read_in(value);

            if (input != this->in2) {
                this->in2 = input;
                this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        default:
        	SPFilterPrimitive::set(key, value);
            break;
    }
}

/**
 * Receives update notifications.
 */
void SPFeBlend::update(SPCtx *ctx, guint flags) {
    if (flags & SP_OBJECT_MODIFIED_FLAG) {
        this->readAttr(SPAttr::MODE);
        this->readAttr(SPAttr::IN2);
    }

    /* Unlike normal in, in2 is required attribute. Make sure, we can call
     * it by some name. */
    /* This may not be true.... see issue at 
     * http://www.w3.org/TR/filter-effects/#feBlendElement (but it doesn't hurt). */
    if (this->in2 == Inkscape::Filters::NR_FILTER_SLOT_NOT_SET ||
        this->in2 == Inkscape::Filters::NR_FILTER_UNNAMED_SLOT)
    {
        SPFilter *parent = SP_FILTER(this->parent);
        this->in2 = this->name_previous_out();

        // TODO: XML Tree being used directly here while it shouldn't be.
        this->setAttribute("in2", parent->name_for_image(this->in2));
    }

    SPFilterPrimitive::update(ctx, flags);
}

/**
 * Writes its settings to an incoming repr object, if any.
 */
Inkscape::XML::Node* SPFeBlend::write(Inkscape::XML::Document *doc, Inkscape::XML::Node *repr, guint flags) {
    SPFilter *parent = SP_FILTER(this->parent);

    if (!repr) {
        repr = doc->createElement("svg:feBlend");
    }

    gchar const *in2_name = parent->name_for_image(this->in2);

    if( !in2_name ) {

        // This code is very similar to name_previous_out()
        SPObject *i = parent->firstChild();

        // Find previous filter primitive
        while (i && i->getNext() != this) {
        	i = i->getNext();
        }

        if( i ) {
            SPFilterPrimitive *i_prim = SP_FILTER_PRIMITIVE(i);
            in2_name = parent->name_for_image(i_prim->image_out);
        }
    }

    if (in2_name) {
        repr->setAttribute("in2", in2_name);
    } else {
        g_warning("Unable to set in2 for feBlend");
    }

    char const *mode;
    switch(this->blend_mode) {
        case SP_CSS_BLEND_NORMAL:
            mode = "normal";      break;
        case SP_CSS_BLEND_MULTIPLY:
            mode = "multiply";    break;
        case SP_CSS_BLEND_SCREEN:
            mode = "screen";      break;
        case SP_CSS_BLEND_DARKEN:
            mode = "darken";      break;
        case SP_CSS_BLEND_LIGHTEN:
            mode = "lighten";     break;
        // New
        case SP_CSS_BLEND_OVERLAY:
            mode = "overlay";     break;
        case SP_CSS_BLEND_COLORDODGE:
            mode = "color-dodge"; break;
        case SP_CSS_BLEND_COLORBURN:
            mode = "color-burn";  break;
        case SP_CSS_BLEND_HARDLIGHT:
            mode = "hard-light";  break;
        case SP_CSS_BLEND_SOFTLIGHT:
            mode = "soft-light";  break;
        case SP_CSS_BLEND_DIFFERENCE:
            mode = "difference";  break;
        case SP_CSS_BLEND_EXCLUSION:
            mode = "exclusion";   break;
        case SP_CSS_BLEND_HUE:
            mode = "hue";         break;
        case SP_CSS_BLEND_SATURATION:
            mode = "saturation";  break;
        case SP_CSS_BLEND_COLOR:
            mode = "color";       break;
        case SP_CSS_BLEND_LUMINOSITY:
            mode = "luminosity";  break;
        default:
            mode = nullptr;
    }

    repr->setAttribute("mode", mode);

    SPFilterPrimitive::write(doc, repr, flags);

    return repr;
}

void SPFeBlend::build_renderer(Inkscape::Filters::Filter* filter) {
    g_assert(filter != nullptr);

    int primitive_n = filter->add_primitive(Inkscape::Filters::NR_FILTER_BLEND);
    Inkscape::Filters::FilterPrimitive *nr_primitive = filter->get_primitive(primitive_n);
    Inkscape::Filters::FilterBlend *nr_blend = dynamic_cast<Inkscape::Filters::FilterBlend*>(nr_primitive);
    g_assert(nr_blend != nullptr);

    this->renderer_common(nr_primitive);

    nr_blend->set_mode(this->blend_mode);
    nr_blend->set_input(1, this->in2);
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
