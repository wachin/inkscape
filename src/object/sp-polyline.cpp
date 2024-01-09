// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <polyline> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "attributes.h"
#include "sp-polygon.h"
#include "sp-polyline.h"
#include "display/curve.h"
#include <glibmm/i18n.h>
#include "xml/repr.h"
#include "document.h"

SPPolyLine::SPPolyLine() : SPShape() {
}

SPPolyLine::~SPPolyLine() = default;

void SPPolyLine::build(SPDocument * document, Inkscape::XML::Node * repr) {
    SPShape::build(document, repr);

    this->readAttr(SPAttr::POINTS);
}

void SPPolyLine::set(SPAttr key, const gchar* value) {
    switch (key) {
        case SPAttr::POINTS:
            if (value) {
                setCurve(sp_poly_parse_curve(value));
            }
            break;
        default:
            SPShape::set(key, value);
            break;
    }
}

Inkscape::XML::Node* SPPolyLine::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:polyline");
    }

    if (repr != this->getRepr()) {
        repr->mergeFrom(this->getRepr(), "id");
    }

    SPShape::write(xml_doc, repr, flags);

    return repr;
}

const char* SPPolyLine::typeName() const {
    return "path";
}

gchar* SPPolyLine::description() const {
	return g_strdup(_("<b>Polyline</b>"));
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
