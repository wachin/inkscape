// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_SP_NAMEDVIEW_H
#define INKSCAPE_SP_NAMEDVIEW_H

/*
 * <sodipodi:namedview> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) Lauris Kaplinski 2000-2002
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "attributes.h"
#include "sp-object-group.h"
#include "snap.h"
#include "document.h"
#include "util/units.h"
#include "svg/svg-bool.h"
#include <vector>

namespace Inkscape {
    class CanvasPage;
    namespace Util {
        class Unit;
    }
}

class SPGrid;

typedef unsigned int guint32;
typedef guint32 GQuark;

enum {
    SP_BORDER_LAYER_BOTTOM,
    SP_BORDER_LAYER_TOP
};

class SPNamedView final : public SPObjectGroup {
public:
    SPNamedView();
    ~SPNamedView() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    unsigned int editable : 1;

    SVGBool showguides;
    SVGBool lockguides;
    SVGBool grids_visible;
    SVGBool clip_to_page; // if true, clip rendered content to pages' boundaries
    guint32 desk_color;
    SVGBool desk_checkerboard;

    double zoom;
    double rotation; // Document rotation in degrees (positive is clockwise)
    double cx;
    double cy;
    int window_width;
    int window_height;
    int window_x;
    int window_y;
    int window_maximized;

    SnapManager snap_manager;

    Inkscape::Util::Unit const *display_units;   // Units used for the UI (*not* the same as units of SVG coordinates)
    // Inkscape::Util::Unit const *page_size_units; // Only used in "Custom size" part of Document Properties dialog 
    
    GQuark default_layer_id;

    double connector_spacing;

    guint32 guidecolor;
    guint32 guidehicolor;

    std::vector<SPGuide *> guides;
    std::vector<SPGrid *> grids;
    std::vector<SPDesktop *> views;

    int viewcount;

    void show(SPDesktop *desktop);
    void hide(SPDesktop const *desktop);
    void setDefaultAttribute(std::string attribute, std::string preference, std::string fallback);
    void activateGuides(void* desktop, bool active);
    char const *getName() const;
    std::vector<SPDesktop *> const getViewList() const;
    Inkscape::Util::Unit const * getDisplayUnit() const;
    void setDisplayUnit(std::string unit);
    void setDisplayUnit(Inkscape::Util::Unit const *unit);

    void translateGuides(Geom::Translate const &translation);
    void translateGrids(Geom::Translate const &translation);
    void scrollAllDesktops(double dx, double dy);

    bool getShowGrids();
    void setShowGrids(bool v);

    void toggleShowGuides();
    void toggleLockGuides();
    void toggleShowGrids();

    bool getLockGuides();
    void setLockGuides(bool v);

    void setShowGuides(bool v);
    bool getShowGuides();

    void updateViewPort();

    // page background, border, desk colors
    void change_color(unsigned int rgba, SPAttr color_key, SPAttr opacity_key = SPAttr::INVALID);
    // show border, border on top, anti-aliasing, ...
    void change_bool_setting(SPAttr key, bool value);
    // sync desk colors
    void set_desk_color(SPDesktop* desktop);
    // turn clip to page mode on/off
    void set_clip_to_page(SPDesktop* desktop, bool enable);
    // immediate show/hide guides request, not recorded in a named view
    void temporarily_show_guides(bool show);

    SPGrid *getFirstEnabledGrid();

private:
    void updateGuides();
    void updateGrids();

    void setShowGuideSingle(SPGuide *guide);

    friend class SPDocument;

    Inkscape::CanvasPage *_viewport = nullptr;

protected:
    void build(SPDocument *document, Inkscape::XML::Node *repr) override;
    void release() override;
    void modified(unsigned int flags) override;
    void update(SPCtx *ctx, unsigned int flags) override;
    void set(SPAttr key, char const* value) override;

    void child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) override;
    void remove_child(Inkscape::XML::Node* child) override;
    void order_changed(Inkscape::XML::Node *child, Inkscape::XML::Node *old_repr,
                       Inkscape::XML::Node *new_repr) override;

    Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;
};


void sp_namedview_window_from_document(SPDesktop *desktop);
void sp_namedview_zoom_and_view_from_document(SPDesktop *desktop);
void sp_namedview_document_from_window(SPDesktop *desktop);
void sp_namedview_update_layers_from_document (SPDesktop *desktop);

const Inkscape::Util::Unit* sp_parse_document_units(const char* unit);


#endif /* !INKSCAPE_SP_NAMEDVIEW_H */

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
