// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief SVG composite filter effect
 *//*
 * Authors:
 *   Hugo Rodrigues <haa.rodrigues@gmail.com>
 *
 * Copyright (C) 2006 Hugo Rodrigues
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SP_FECOMPOSITE_H_SEEN
#define SP_FECOMPOSITE_H_SEEN

#include "sp-filter-primitive.h"

#define SP_FECOMPOSITE(obj) (dynamic_cast<SPFeComposite*>((SPObject*)obj))
#define SP_IS_FECOMPOSITE(obj) (dynamic_cast<const SPFeComposite*>((SPObject*)obj) != NULL)

enum FeCompositeOperator {
    // Default value is 'over', but let's distinguish specifying the
    // default and implicitly using the default
    COMPOSITE_DEFAULT,
    COMPOSITE_OVER,              /* Source Over */
    COMPOSITE_IN,                /* Source In   */
    COMPOSITE_OUT,               /* Source Out  */
    COMPOSITE_ATOP,              /* Source Atop */
    COMPOSITE_XOR,
    COMPOSITE_ARITHMETIC,        /* Not a fundamental PorterDuff operator, nor Cairo */
#ifdef WITH_CSSCOMPOSITE
    // New in CSS
    COMPOSITE_CLEAR,
    COMPOSITE_COPY,              /* Source      */
    COMPOSITE_DESTINATION,
    COMPOSITE_DESTINATION_OVER,
    COMPOSITE_DESTINATION_IN,
    COMPOSITE_DESTINATION_OUT,
    COMPOSITE_DESTINATION_ATOP,
    COMPOSITE_LIGHTER,           /* Plus, Add (Not a fundamental PorterDuff operator  */
#endif
    COMPOSITE_ENDOPERATOR        /* Cairo Saturate is not included in CSS */
};

class SPFeComposite : public SPFilterPrimitive {
public:
	SPFeComposite();
	~SPFeComposite() override;

    FeCompositeOperator composite_operator;
    double k1, k2, k3, k4;
    int in2;

protected:
    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	void release() override;

	void set(SPAttr key, const gchar* value) override;

	void update(SPCtx* ctx, unsigned int flags) override;

	Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, guint flags) override;

	void build_renderer(Inkscape::Filters::Filter* filter) override;
};

#endif /* !SP_FECOMPOSITE_H_SEEN */

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
