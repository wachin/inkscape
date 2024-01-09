// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <cairo.h>
#include <2geom/transforms.h>

#include "sp-radial-gradient.h"

#include "attributes.h"
#include "style.h"
#include "xml/repr.h"

#include "display/drawing-paintserver.h"

/*
 * Radial Gradient
 */
SPRadialGradient::SPRadialGradient() : SPGradient() {
    this->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->r.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->fx.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->fy.unset(SVGLength::PERCENT, 0.5, 0.5);
    this->fr.unset(SVGLength::PERCENT, 0.5, 0.5);
}

SPRadialGradient::~SPRadialGradient() = default;

/**
 * Set radial gradient attributes from associated repr.
 */
void SPRadialGradient::build(SPDocument *document, Inkscape::XML::Node *repr) {
    SPGradient::build(document, repr);

    this->readAttr(SPAttr::CX);
    this->readAttr(SPAttr::CY);
    this->readAttr(SPAttr::R);
    this->readAttr(SPAttr::FX);
    this->readAttr(SPAttr::FY);
    this->readAttr(SPAttr::FR);
}

/**
 * Set radial gradient attribute.
 */
void SPRadialGradient::set(SPAttr key, gchar const *value) {

    switch (key) {
        case SPAttr::CX:
            if (!this->cx.read(value)) {
                this->cx.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            if (!this->fx._set) {
                this->fx.value = this->cx.value;
                this->fx.computed = this->cx.computed;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::CY:
            if (!this->cy.read(value)) {
                this->cy.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            if (!this->fy._set) {
                this->fy.value = this->cy.value;
                this->fy.computed = this->cy.computed;
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::R:
            if (!this->r.read(value)) {
                this->r.unset(SVGLength::PERCENT, 0.5, 0.5);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::FX:
            if (!this->fx.read(value)) {
                this->fx.unset(this->cx.unit, this->cx.value, this->cx.computed);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::FY:
            if (!this->fy.read(value)) {
                this->fy.unset(this->cy.unit, this->cy.value, this->cy.computed);
            }

            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::FR:
            if (!this->fr.read(value)) {
                this->fr.unset(SVGLength::PERCENT, 0.0, 0.0);
            }
            this->requestModified(SP_OBJECT_MODIFIED_FLAG);
            break;

        default:
            SPGradient::set(key, value);
            break;
    }
}

void
SPRadialGradient::update(SPCtx *ctx, guint flags)
{
    // To do: Verify flags.
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {

        SPItemCtx const *ictx = reinterpret_cast<SPItemCtx const *>(ctx);

        if (getUnits() == SP_GRADIENT_UNITS_USERSPACEONUSE) {
            double w = ictx->viewport.width();
            double h = ictx->viewport.height();
            double d = sqrt ((w*w + h*h)/2.0);
            double const em = style->font_size.computed;
            double const ex = 0.5 * em;  // fixme: get x height from pango or libnrtype.

            this->cx.update(em, ex, w);
            this->cy.update(em, ex, h);
            this->r.update(em, ex, d);
            this->fx.update(em, ex, w);
            this->fy.update(em, ex, h);
            this->fr.update(em, ex, d);
        }
    }
}

/**
 * Write radial gradient attributes to associated repr.
 */
Inkscape::XML::Node* SPRadialGradient::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:radialGradient");
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->cx._set) {
    	repr->setAttributeSvgDouble("cx", this->cx.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->cy._set) {
    	repr->setAttributeSvgDouble("cy", this->cy.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->r._set) {
    	repr->setAttributeSvgDouble("r", this->r.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->fx._set) {
    	repr->setAttributeSvgDouble("fx", this->fx.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->fy._set) {
    	repr->setAttributeSvgDouble("fy", this->fy.computed);
    }

    if ((flags & SP_OBJECT_WRITE_ALL) || this->fr._set) {
    	repr->setAttributeSvgDouble("fr", this->fr.computed);
    }

    SPGradient::write(xml_doc, repr, flags);

    return repr;
}

std::unique_ptr<Inkscape::DrawingPaintServer> SPRadialGradient::create_drawing_paintserver()
{
    ensureVector();
    return std::make_unique<Inkscape::DrawingRadialGradient>(getSpread(), getUnits(), gradientTransform,
                                                             fx.computed, fy.computed, cx.computed, cy.computed, r.computed, fr.computed, vector.stops);
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
