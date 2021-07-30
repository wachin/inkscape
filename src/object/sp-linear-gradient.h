// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SP_LINEAR_GRADIENT_H
#define SP_LINEAR_GRADIENT_H

/** \file
 * SPLinearGradient: SVG <lineargradient> implementation
 */

#include "sp-gradient.h"
#include "svg/svg-length.h"

/** Linear gradient. */
class SPLinearGradient : public SPGradient {
public:
    SPLinearGradient();
    ~SPLinearGradient() override;

    SVGLength x1;
    SVGLength y1;
    SVGLength x2;
    SVGLength y2;

    cairo_pattern_t* pattern_new(cairo_t *ct, Geom::OptRect const &bbox, double opacity) override;

protected:
    void build(SPDocument *document, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, char const *value) override;
    void update(SPCtx *ctx, guint flags) override;
    Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;
};

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_LINEARGRADIENT, SPLinearGradient)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_LINEARGRADIENT, SPLinearGradient)

#endif /* !SP_LINEAR_GRADIENT_H */

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
