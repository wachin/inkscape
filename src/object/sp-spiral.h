// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_SPIRAL_H
#define SEEN_SP_SPIRAL_H
/*
 * Authors:
 *   Mitsuru Oka <oka326@parkcity.ne.jp>
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "sp-shape.h"

#define noSPIRAL_VERBOSE

#define SP_EPSILON       1e-5
#define SP_EPSILON_2     (SP_EPSILON * SP_EPSILON)
#define SP_HUGE          1e5

#define SPIRAL_TOLERANCE 3.0
#define SAMPLE_STEP      (1.0/4.0) ///< step per 2PI
#define SAMPLE_SIZE      8         ///< sample size per one bezier


/**
 * A spiral Shape.
 *
 * The Spiral shape is defined as:
 * \verbatim
   x(t) = rad * t^exp cos(2 * Pi * revo*t + arg) + cx
   y(t) = rad * t^exp sin(2 * Pi * revo*t + arg) + cy    \endverbatim
 * where spiral curve is drawn for {t | t0 <= t <= 1}. The  rad and arg 
 * parameters can also be represented by transformation. 
 *
 * \todo Should I remove these attributes?
 */
class SPSpiral : public SPShape {
public:
	SPSpiral();
	~SPSpiral() override;

	float cx, cy;
	float exp;  ///< Spiral expansion factor
	float revo; ///< Spiral revolution factor
	float rad;  ///< Spiral radius
	float arg;  ///< Spiral argument
	float t0;

	/* Lowlevel interface */
	void setPosition(double cx, double cy, double exp, double revo, double rad, double arg, double t0);
	Geom::Affine set_transform(Geom::Affine const& xform) override;

	Geom::Point getXY(double t) const;

	void getPolar(double t, double* rad, double* arg) const;

	bool isInvalid() const;

	void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;
	void update(SPCtx *ctx, unsigned int flags) override;
	void set(SPAttr key, char const* value) override;

	void snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) const override;
        const char* typeName() const override;
        const char* displayName() const override;
	char* description() const override;
    void update_patheffect(bool write) override;
	void set_shape() override;

private:
	Geom::Point getTangent(double t) const;
	void fitAndDraw(SPCurve* c, double dstep, Geom::Point darray[], Geom::Point const& hat1, Geom::Point& hat2, double* t) const;
};

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_SPIRAL, SPSpiral)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_SPIRAL, SPSpiral)

#endif // SEEN_SP_SPIRAL_H
