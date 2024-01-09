// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SVG <image> implementation
 *//*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Edward Flick (EAF)
 *
 * Copyright (C) 1999-2005 Authors
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_SP_IMAGE_H
#define SEEN_INKSCAPE_SP_IMAGE_H

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <glibmm/ustring.h>
#include "svg/svg-length.h"
#include "sp-item.h"
#include "viewbox.h"
#include "sp-dimensions.h"
#include "display/curve.h"

#include <memory>

#define SP_IMAGE_HREF_MODIFIED_FLAG SP_OBJECT_USER_MODIFIED_FLAG_A

namespace Inkscape { class Pixbuf; }
class SPImage final : public SPItem, public SPViewBox, public SPDimensions {
public:
    SPImage();
    ~SPImage() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    Geom::Rect clipbox;
    double sx, sy;
    double ox, oy;
    double dpi;
    double prev_width, prev_height;

    std::optional<SPCurve> curve; // This curve is at the image's boundary for snapping

    char *href;
    char *color_profile;

    std::shared_ptr<Inkscape::Pixbuf const> pixbuf;
    bool missing = true;

    void build(SPDocument *document, Inkscape::XML::Node *repr) override;
    void release() override;
    void set(SPAttr key, char const* value) override;
    void update(SPCtx *ctx, unsigned int flags) override;
    Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;
    void modified(unsigned int flags) override;

    Geom::OptRect bbox(Geom::Affine const &transform, SPItem::BBoxType type) const override;
    void print(SPPrintContext *ctx) override;
    const char* typeName() const override;
    const char* displayName() const override;
    char* description() const override;
    Inkscape::DrawingItem* show(Inkscape::Drawing &drawing, unsigned int key, unsigned int flags) override;
    void snappoints(std::vector<Inkscape::SnapCandidatePoint> &p, Inkscape::SnapPreferences const *snapprefs) const override;
    Geom::Affine set_transform(Geom::Affine const &transform) override;

    void apply_profile(Inkscape::Pixbuf *pixbuf);

    SPCurve const *get_curve() const;
    void refresh_if_outdated();
    bool cropToArea(Geom::Rect area);
    bool cropToArea(const Geom::IntRect &area);
private:
    static Inkscape::Pixbuf *readImage(gchar const *href, gchar const *absref, gchar const *base, double svgdpi = 0);
    static Inkscape::Pixbuf *getBrokenImage(double width, double height);
};

/* Return duplicate of curve or NULL */
void sp_embed_image(Inkscape::XML::Node *imgnode, Inkscape::Pixbuf *pb);
void sp_embed_svg(Inkscape::XML::Node *image_node, std::string const &fn);

#endif
