// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_PATH_H
#define SEEN_SP_PATH_H

/*
 * SVG <path> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ximian, Inc.
 *   Johan Engelen
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 1999-2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-shape.h"
#include "sp-conn-end-pair.h"
#include "style-internal.h" // For SPStyleSrc

class SPCurve;

/**
 * SVG <path> implementation
 */
class SPPath : public SPShape {
public:
    SPPath();
    ~SPPath() override;

    int nodesInPath() const;
    friend class SPConnEndPair;
    SPConnEndPair connEndPair;

    void build(SPDocument *document, Inkscape::XML::Node *repr) override;
    void release() override;
    void update(SPCtx* ctx, unsigned int flags) override;

    void set(SPAttr key, char const* value) override;
    void update_patheffect(bool write) override;
    Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;

    const char* typeName() const override;
    const char* displayName() const override;
    char* description() const override;
    Geom::Affine set_transform(Geom::Affine const &transform) override;
    void convert_to_guides() const override;
private:
    SPStyleSrc d_source;  // Source of 'd' value, saved for output.
};

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_PATH, SPPath)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_PATH, SPPath)

#endif // SEEN_SP_PATH_H

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
