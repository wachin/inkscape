// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <font-face> element implementation
 *
 * Section 20.8.3 of the W3C SVG 1.1 spec
 * available at:
 * http://www.w3.org/TR/SVG/fonts.html#FontFaceElement
 *
 * Author:
 *   Felipe C. da S. Sanches <juca@members.fsf.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2008, Felipe C. da S. Sanches
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "xml/repr.h"
#include "attributes.h"
#include "sp-font-face.h"
#include "document.h"

#include <cstring>

static std::vector<FontFaceStyleType> sp_read_fontFaceStyleType(gchar const *value){
    std::vector<FontFaceStyleType> v;

    if (!value){
        v.push_back(SP_FONTFACE_STYLE_ALL);
        return v;
    }

    if (strncmp(value, "all", 3) == 0){
        value += 3;
        while(value[0]==',' || value[0]==' ')
            value++;
        v.push_back(SP_FONTFACE_STYLE_ALL);
        return v;
    }

    while(value[0]!='\0'){
        switch(value[0]){
            case 'n':
                if (strncmp(value, "normal", 6) == 0){
                    v.push_back(SP_FONTFACE_STYLE_NORMAL);
                    value += 6;
                }
                break;
            case 'i':
                if (strncmp(value, "italic", 6) == 0){
                    v.push_back(SP_FONTFACE_STYLE_ITALIC);
                    value += 6;
                }
                break;
            case 'o':
                if (strncmp(value, "oblique", 7) == 0){
                    v.push_back(SP_FONTFACE_STYLE_OBLIQUE);
                    value += 7;
                }
                break;
        }
        while(value[0]==',' || value[0]==' ')
            value++;
    }
    return v;
}

static std::vector<FontFaceVariantType> sp_read_fontFaceVariantType(gchar const *value){
    std::vector<FontFaceVariantType> v;

    if (!value){
        v.push_back(SP_FONTFACE_VARIANT_NORMAL);
        return v;
    }

    while(value[0]!='\0'){
        switch(value[0]){
            case 'n':
                if (strncmp(value, "normal", 6) == 0){
                    v.push_back(SP_FONTFACE_VARIANT_NORMAL);
                    value += 6;
                }
                break;
            case 's':
                if (strncmp(value, "small-caps", 10) == 0){
                    v.push_back(SP_FONTFACE_VARIANT_SMALL_CAPS);
                    value += 10;
                }
                break;
        }
        while(value[0]==',' || value[0]==' ')
            value++;
    }
    return v;
}

static std::vector<FontFaceWeightType> sp_read_fontFaceWeightType(gchar const *value){
    std::vector<FontFaceWeightType> v;

    if (!value){
        v.push_back(SP_FONTFACE_WEIGHT_ALL);
        return v;
    }

    if (strncmp(value, "all", 3) == 0){
        value += 3;
        while(value[0]==',' || value[0]==' ')
            value++;
        v.push_back(SP_FONTFACE_WEIGHT_ALL);
        return v;
    }

    while(value[0]!='\0'){
        switch(value[0]){
            case 'n':
                if (strncmp(value, "normal", 6) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_NORMAL);
                    value += 6;
                }
                break;
            case 'b':
                if (strncmp(value, "bold", 4) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_BOLD);
                    value += 4;
                }
                break;
            case '1':
                if (strncmp(value, "100", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_100);
                    value += 3;
                }
                break;
            case '2':
                if (strncmp(value, "200", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_200);
                    value += 3;
                }
                break;
            case '3':
                if (strncmp(value, "300", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_300);
                    value += 3;
                }
                break;
            case '4':
                if (strncmp(value, "400", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_400);
                    value += 3;
                }
                break;
            case '5':
                if (strncmp(value, "500", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_500);
                    value += 3;
                }
                break;
            case '6':
                if (strncmp(value, "600", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_600);
                    value += 3;
                }
                break;
            case '7':
                if (strncmp(value, "700", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_700);
                    value += 3;
                }
                break;
            case '8':
                if (strncmp(value, "800", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_800);
                    value += 3;
                }
                break;
            case '9':
                if (strncmp(value, "900", 3) == 0){
                    v.push_back(SP_FONTFACE_WEIGHT_900);
                    value += 3;
                }
                break;
        }
        while(value[0]==',' || value[0]==' ')
            value++;
    }
    return v;
}

static std::vector<FontFaceStretchType> sp_read_fontFaceStretchType(gchar const *value){
    std::vector<FontFaceStretchType> v;

    if (!value){
        v.push_back(SP_FONTFACE_STRETCH_NORMAL);
        return v;
    }

    if (strncmp(value, "all", 3) == 0){
        value += 3;
        while(value[0]==',' || value[0]==' ')
            value++;
        v.push_back(SP_FONTFACE_STRETCH_ALL);
        return v;
    }

    while(value[0]!='\0'){
        switch(value[0]){
            case 'n':
                if (strncmp(value, "normal", 6) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_NORMAL);
                    value += 6;
                }
                break;
            case 'u':
                if (strncmp(value, "ultra-condensed", 15) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_ULTRA_CONDENSED);
                    value += 15;
                }
                if (strncmp(value, "ultra-expanded", 14) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_ULTRA_EXPANDED);
                    value += 14;
                }
                break;
            case 'e':
                if (strncmp(value, "expanded", 8) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_EXPANDED);
                    value += 8;
                }
                if (strncmp(value, "extra-condensed", 15) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_EXTRA_CONDENSED);
                    value += 15;
                }
                if (strncmp(value, "extra-expanded", 14) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_EXTRA_EXPANDED);
                    value += 14;
                }
                break;
            case 'c':
                if (strncmp(value, "condensed", 9) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_CONDENSED);
                    value += 9;
                }
                break;
            case 's':
                if (strncmp(value, "semi-condensed", 14) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_SEMI_CONDENSED);
                    value += 14;
                }
                if (strncmp(value, "semi-expanded", 13) == 0){
                    v.push_back(SP_FONTFACE_STRETCH_SEMI_EXPANDED);
                    value += 13;
                }
                break;
        }
        while(value[0]==',' || value[0]==' ')
            value++;
    }
    return v;
}

SPFontFace::SPFontFace() : SPObject() {
    std::vector<FontFaceStyleType> style;
    style.push_back(SP_FONTFACE_STYLE_ALL);
    this->font_style = style;

    std::vector<FontFaceVariantType> variant;
    variant.push_back(SP_FONTFACE_VARIANT_NORMAL);
    this->font_variant = variant;

    std::vector<FontFaceWeightType> weight;
    weight.push_back(SP_FONTFACE_WEIGHT_ALL);
    this->font_weight = weight;

    std::vector<FontFaceStretchType> stretch;
    stretch.push_back(SP_FONTFACE_STRETCH_NORMAL);
    this->font_stretch = stretch;
    this->font_family = nullptr;

    //this->font_style = ;
    //this->font_variant = ;
    //this->font_weight = ;
    //this->font_stretch = ;
    this->font_size = nullptr;
    //this->unicode_range = ;
    this->units_per_em = 1000;
    //this->panose_1 = ;
    this->stemv = 0;
    this->stemh = 0;
    this->slope = 0;
    this->cap_height = 0;
    this->x_height = 0;
    this->accent_height = 0;
    this->ascent = 0;
    this->descent = 0;
    this->widths = nullptr;
    this->bbox = nullptr;
    this->ideographic = 0;
    this->alphabetic = 0;
    this->mathematical = 0;
    this->hanging = 0;
    this->v_ideographic = 0;
    this->v_alphabetic = 0;
    this->v_mathematical = 0;
    this->v_hanging = 0;
    this->underline_position = 0;
    this->underline_thickness = 0;
    this->strikethrough_position = 0;
    this->strikethrough_thickness = 0;
    this->overline_position = 0;
    this->overline_thickness = 0;
}

SPFontFace::~SPFontFace() = default;

void SPFontFace::build(SPDocument *document, Inkscape::XML::Node *repr) {
	SPObject::build(document, repr);

	this->readAttr(SPAttr::FONT_FAMILY);
	this->readAttr(SPAttr::FONT_STYLE);
	this->readAttr(SPAttr::FONT_VARIANT);
	this->readAttr(SPAttr::FONT_WEIGHT);
	this->readAttr(SPAttr::FONT_STRETCH);
	this->readAttr(SPAttr::FONT_SIZE);
	this->readAttr(SPAttr::UNICODE_RANGE);
	this->readAttr(SPAttr::UNITS_PER_EM);
	this->readAttr(SPAttr::PANOSE_1);
	this->readAttr(SPAttr::STEMV);
	this->readAttr(SPAttr::STEMH);
	this->readAttr(SPAttr::SLOPE);
	this->readAttr(SPAttr::CAP_HEIGHT);
	this->readAttr(SPAttr::X_HEIGHT);
	this->readAttr(SPAttr::ACCENT_HEIGHT);
	this->readAttr(SPAttr::ASCENT);
	this->readAttr(SPAttr::DESCENT);
	this->readAttr(SPAttr::WIDTHS);
	this->readAttr(SPAttr::BBOX);
	this->readAttr(SPAttr::IDEOGRAPHIC);
	this->readAttr(SPAttr::ALPHABETIC);
	this->readAttr(SPAttr::MATHEMATICAL);
	this->readAttr(SPAttr::HANGING);
	this->readAttr(SPAttr::V_IDEOGRAPHIC);
	this->readAttr(SPAttr::V_ALPHABETIC);
	this->readAttr(SPAttr::V_MATHEMATICAL);
	this->readAttr(SPAttr::V_HANGING);
	this->readAttr(SPAttr::UNDERLINE_POSITION);
	this->readAttr(SPAttr::UNDERLINE_THICKNESS);
	this->readAttr(SPAttr::STRIKETHROUGH_POSITION);
	this->readAttr(SPAttr::STRIKETHROUGH_THICKNESS);
	this->readAttr(SPAttr::OVERLINE_POSITION);
	this->readAttr(SPAttr::OVERLINE_THICKNESS);
}

/**
 * Callback for child_added event.
 */
void SPFontFace::child_added(Inkscape::XML::Node *child, Inkscape::XML::Node *ref) {
    SPObject::child_added(child, ref);

    this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}


/**
 * Callback for remove_child event.
 */
void SPFontFace::remove_child(Inkscape::XML::Node *child) {
    SPObject::remove_child(child);

    this->parent->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void SPFontFace::release() {
	SPObject::release();
}

void SPFontFace::set(SPAttr key, const gchar *value) {
    std::vector<FontFaceStyleType> style;
    std::vector<FontFaceVariantType> variant;
    std::vector<FontFaceWeightType> weight;
    std::vector<FontFaceStretchType> stretch;

    switch (key) {
        case SPAttr::FONT_FAMILY:
            if (this->font_family) {
                g_free(this->font_family);
            }
            
            this->font_family = g_strdup(value);
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::FONT_STYLE:
            style = sp_read_fontFaceStyleType(value);
            
            if (this->font_style.size() != style.size()){
                this->font_style = style;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            } else {
                for (unsigned int i=0;i<style.size();i++){
                    if (style[i] != this->font_style[i]){
                        this->font_style = style;
                        this->requestModified(SP_OBJECT_MODIFIED_FLAG);
                        break;
                    }
                }
            }
            break;
        case SPAttr::FONT_VARIANT:
            variant = sp_read_fontFaceVariantType(value);
            
            if (this->font_variant.size() != variant.size()){
                this->font_variant = variant;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            } else {
                for (unsigned int i=0;i<variant.size();i++){
                    if (variant[i] != this->font_variant[i]){
                        this->font_variant = variant;
                        this->requestModified(SP_OBJECT_MODIFIED_FLAG);
                        break;
                    }
                }
            }
            break;
        case SPAttr::FONT_WEIGHT:
            weight = sp_read_fontFaceWeightType(value);
            
            if (this->font_weight.size() != weight.size()){
                this->font_weight = weight;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            } else {
                for (unsigned int i=0;i<weight.size();i++){
                    if (weight[i] != this->font_weight[i]){
                        this->font_weight = weight;
                        this->requestModified(SP_OBJECT_MODIFIED_FLAG);
                        break;
                    }
                }
            }
            break;
        case SPAttr::FONT_STRETCH:
            stretch = sp_read_fontFaceStretchType(value);
            
            if (this->font_stretch.size() != stretch.size()){
                this->font_stretch = stretch;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            } else {
                for (unsigned int i=0;i<stretch.size();i++){
                    if (stretch[i] != this->font_stretch[i]){
                        this->font_stretch = stretch;
                        this->requestModified(SP_OBJECT_MODIFIED_FLAG);
                        break;
                    }
                }
            }
            break;
        case SPAttr::UNITS_PER_EM:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->units_per_em){
                this->units_per_em = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::STEMV:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->stemv){
                this->stemv = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::STEMH:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->stemh){
                this->stemh = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::SLOPE:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->slope){
                this->slope = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::CAP_HEIGHT:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->cap_height){
                this->cap_height = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::X_HEIGHT:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->x_height){
                this->x_height = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::ACCENT_HEIGHT:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->accent_height){
                this->accent_height = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::ASCENT:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->ascent){
                this->ascent = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::DESCENT:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->descent){
                this->descent = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::IDEOGRAPHIC:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->ideographic){
                this->ideographic = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::ALPHABETIC:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->alphabetic){
                this->alphabetic = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::MATHEMATICAL:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->mathematical){
                this->mathematical = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::HANGING:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->hanging){
                this->hanging = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::V_IDEOGRAPHIC:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->v_ideographic){
                this->v_ideographic = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::V_ALPHABETIC:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->v_alphabetic){
                this->v_alphabetic = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::V_MATHEMATICAL:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->v_mathematical){
                this->v_mathematical = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::V_HANGING:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->v_hanging){
                this->v_hanging = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::UNDERLINE_POSITION:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->underline_position){
                this->underline_position = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::UNDERLINE_THICKNESS:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->underline_thickness){
                this->underline_thickness = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::STRIKETHROUGH_POSITION:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->strikethrough_position){
                this->strikethrough_position = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::STRIKETHROUGH_THICKNESS:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->strikethrough_thickness){
                this->strikethrough_thickness = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::OVERLINE_POSITION:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->overline_position){
                this->overline_position = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        case SPAttr::OVERLINE_THICKNESS:
        {
            double number = value ? g_ascii_strtod(value, nullptr) : 0;
            
            if (number != this->overline_thickness){
                this->overline_thickness = number;
                this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
        }
        default:
        	SPObject::set(key, value);
            break;
    }
}

/**
 * Receives update notifications.
 */
void SPFontFace::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG)) {
        this->readAttr(SPAttr::FONT_FAMILY);
        this->readAttr(SPAttr::FONT_STYLE);
        this->readAttr(SPAttr::FONT_VARIANT);
        this->readAttr(SPAttr::FONT_WEIGHT);
        this->readAttr(SPAttr::FONT_STRETCH);
        this->readAttr(SPAttr::FONT_SIZE);
        this->readAttr(SPAttr::UNICODE_RANGE);
        this->readAttr(SPAttr::UNITS_PER_EM);
        this->readAttr(SPAttr::PANOSE_1);
        this->readAttr(SPAttr::STEMV);
        this->readAttr(SPAttr::STEMH);
        this->readAttr(SPAttr::SLOPE);
        this->readAttr(SPAttr::CAP_HEIGHT);
        this->readAttr(SPAttr::X_HEIGHT);
        this->readAttr(SPAttr::ACCENT_HEIGHT);
        this->readAttr(SPAttr::ASCENT);
        this->readAttr(SPAttr::DESCENT);
        this->readAttr(SPAttr::WIDTHS);
        this->readAttr(SPAttr::BBOX);
        this->readAttr(SPAttr::IDEOGRAPHIC);
        this->readAttr(SPAttr::ALPHABETIC);
        this->readAttr(SPAttr::MATHEMATICAL);
        this->readAttr(SPAttr::HANGING);
        this->readAttr(SPAttr::V_IDEOGRAPHIC);
        this->readAttr(SPAttr::V_ALPHABETIC);
        this->readAttr(SPAttr::V_MATHEMATICAL);
        this->readAttr(SPAttr::V_HANGING);
        this->readAttr(SPAttr::UNDERLINE_POSITION);
        this->readAttr(SPAttr::UNDERLINE_THICKNESS);
        this->readAttr(SPAttr::STRIKETHROUGH_POSITION);
        this->readAttr(SPAttr::STRIKETHROUGH_THICKNESS);
        this->readAttr(SPAttr::OVERLINE_POSITION);
        this->readAttr(SPAttr::OVERLINE_THICKNESS);
    }

    SPObject::update(ctx, flags);
}

#define COPY_ATTR(rd,rs,key) (rd)->setAttribute((key), rs->attribute(key));

Inkscape::XML::Node* SPFontFace::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:font-face");
    }

    //TODO:
    //repr->setAttributeSvgDouble("font-family", face->font_family);
    //repr->setAttributeSvgDouble("font-style", face->font_style);
    //repr->setAttributeSvgDouble("font-variant", face->font_variant);
    //repr->setAttributeSvgDouble("font-weight", face->font_weight);
    //repr->setAttributeSvgDouble("font-stretch", face->font_stretch);
    //repr->setAttributeSvgDouble("font-size", face->font_size);
    //repr->setAttributeSvgDouble("unicode-range", face->unicode_range);
    repr->setAttributeSvgDouble("units-per-em", this->units_per_em);
    //repr->setAttributeSvgDouble("panose-1", face->panose_1);
    repr->setAttributeSvgDouble("stemv", this->stemv);
    repr->setAttributeSvgDouble("stemh", this->stemh);
    repr->setAttributeSvgDouble("slope", this->slope);
    repr->setAttributeSvgDouble("cap-height", this->cap_height);
    repr->setAttributeSvgDouble("x-height", this->x_height);
    repr->setAttributeSvgDouble("accent-height", this->accent_height);
    repr->setAttributeSvgDouble("ascent", this->ascent);
    repr->setAttributeSvgDouble("descent", this->descent);
    //repr->setAttributeSvgDouble("widths", face->widths);
    //repr->setAttributeSvgDouble("bbox", face->bbox);
    repr->setAttributeSvgDouble("ideographic", this->ideographic);
    repr->setAttributeSvgDouble("alphabetic", this->alphabetic);
    repr->setAttributeSvgDouble("mathematical", this->mathematical);
    repr->setAttributeSvgDouble("hanging", this->hanging);
    repr->setAttributeSvgDouble("v-ideographic", this->v_ideographic);
    repr->setAttributeSvgDouble("v-alphabetic", this->v_alphabetic);
    repr->setAttributeSvgDouble("v-mathematical", this->v_mathematical);
    repr->setAttributeSvgDouble("v-hanging", this->v_hanging);
    repr->setAttributeSvgDouble("underline-position", this->underline_position);
    repr->setAttributeSvgDouble("underline-thickness", this->underline_thickness);
    repr->setAttributeSvgDouble("strikethrough-position", this->strikethrough_position);
    repr->setAttributeSvgDouble("strikethrough-thickness", this->strikethrough_thickness);
    repr->setAttributeSvgDouble("overline-position", this->overline_position);
    repr->setAttributeSvgDouble("overline-thickness", this->overline_thickness);

    if (repr != this->getRepr()) {
        // In all COPY_ATTR given below the XML tree is 
        //  being used directly while it shouldn't be.
        COPY_ATTR(repr, this->getRepr(), "font-family");
        COPY_ATTR(repr, this->getRepr(), "font-style");
        COPY_ATTR(repr, this->getRepr(), "font-variant");
        COPY_ATTR(repr, this->getRepr(), "font-weight");
        COPY_ATTR(repr, this->getRepr(), "font-stretch");
        COPY_ATTR(repr, this->getRepr(), "font-size");
        COPY_ATTR(repr, this->getRepr(), "unicode-range");
        COPY_ATTR(repr, this->getRepr(), "units-per-em");
        COPY_ATTR(repr, this->getRepr(), "panose-1");
        COPY_ATTR(repr, this->getRepr(), "stemv");
        COPY_ATTR(repr, this->getRepr(), "stemh");
        COPY_ATTR(repr, this->getRepr(), "slope");
        COPY_ATTR(repr, this->getRepr(), "cap-height");
        COPY_ATTR(repr, this->getRepr(), "x-height");
        COPY_ATTR(repr, this->getRepr(), "accent-height");
        COPY_ATTR(repr, this->getRepr(), "ascent");
        COPY_ATTR(repr, this->getRepr(), "descent");
        COPY_ATTR(repr, this->getRepr(), "widths");
        COPY_ATTR(repr, this->getRepr(), "bbox");
        COPY_ATTR(repr, this->getRepr(), "ideographic");
        COPY_ATTR(repr, this->getRepr(), "alphabetic");
        COPY_ATTR(repr, this->getRepr(), "mathematical");
        COPY_ATTR(repr, this->getRepr(), "hanging");
        COPY_ATTR(repr, this->getRepr(), "v-ideographic");
        COPY_ATTR(repr, this->getRepr(), "v-alphabetic");
        COPY_ATTR(repr, this->getRepr(), "v-mathematical");
        COPY_ATTR(repr, this->getRepr(), "v-hanging");
        COPY_ATTR(repr, this->getRepr(), "underline-position");
        COPY_ATTR(repr, this->getRepr(), "underline-thickness");
        COPY_ATTR(repr, this->getRepr(), "strikethrough-position");
        COPY_ATTR(repr, this->getRepr(), "strikethrough-thickness");
        COPY_ATTR(repr, this->getRepr(), "overline-position");
        COPY_ATTR(repr, this->getRepr(), "overline-thickness");
    }

    SPObject::write(xml_doc, repr, flags);

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
