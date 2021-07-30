// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Authors:
 *    Felipe C. da S. Sanches <juca@members.fsf.org>
 *
 * Copyright (C) 2008 Felipe C. da S. Sanches
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SP_GLYPH_H
#define SEEN_SP_GLYPH_H

#include "sp-object.h"

enum glyphArabicForm {
    GLYPH_ARABIC_FORM_INITIAL,
    GLYPH_ARABIC_FORM_MEDIAL,
    GLYPH_ARABIC_FORM_TERMINAL,
    GLYPH_ARABIC_FORM_ISOLATED,
};

enum glyphOrientation {
    GLYPH_ORIENTATION_HORIZONTAL,
    GLYPH_ORIENTATION_VERTICAL,
    GLYPH_ORIENTATION_BOTH
};

/*
 * SVG <glyph> element
 */

class SPGlyph : public SPObject {
public:
    SPGlyph();
    ~SPGlyph() override = default;

    // FIXME encapsulation
    Glib::ustring unicode;
    Glib::ustring glyph_name;
    char* d;
    glyphOrientation orientation;
    glyphArabicForm arabic_form;
    char* lang;
    double horiz_adv_x;
    double vert_origin_x;
    double vert_origin_y;
    double vert_adv_y;

protected:
    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
    void release() override;
    void set(SPAttr key, const char* value) override;
    void update(SPCtx* ctx, unsigned int flags) override;
    Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags) override;

};

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_GLYPH, SPGlyph)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_GLYPH, SPGlyph)

#endif // !SEEN_SP_GLYPH_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
