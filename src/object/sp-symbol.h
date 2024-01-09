// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_SYMBOL_H
#define SEEN_SP_SYMBOL_H

/*
 * SVG <symbol> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 1999-2003 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/*
 * This is quite similar in logic to <svg>
 * Maybe we should merge them somehow (Lauris)
 */

#include <2geom/affine.h>
#include "sp-dimensions.h"
#include "sp-item-group.h"
#include "viewbox.h"

class SPSymbol final : public SPGroup, public SPViewBox, public SPDimensions {
public:
	SPSymbol();
	~SPSymbol() override;
	int tag() const override { return tag_of<decltype(*this)>; }

	void build(SPDocument *document, Inkscape::XML::Node *repr) override;
	void release() override;
	void set(SPAttr key, char const* value) override;
	void update(SPCtx *ctx, unsigned int flags) override;
    void unSymbol();
	Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;

	void modified(unsigned int flags) override;
	void child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) override;

    std::optional<Geom::PathVector> documentExactBounds() const override;
	Inkscape::DrawingItem* show(Inkscape::Drawing &drawing, unsigned int key, unsigned int flags) override;
	void print(SPPrintContext *ctx) override;
	Geom::OptRect bbox(Geom::Affine const &transform, SPItem::BBoxType type) const override;
	void hide (unsigned int key) override;

public:
    // reference point
    SVGLength refX;
    SVGLength refY;
};

#endif
