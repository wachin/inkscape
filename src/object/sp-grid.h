// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape SPGrid implementation
 *
 * Authors:
 * James Ferrarelli
 * Johan Engelen <johan@shouraizou.nl>
 * Lauris Kaplinski
 * Abhishek Sharma
 * Jon A. Cruz <jon@joncruz.org>
 * Tavmong Bah <tavmjong@free.fr>
 * see git history
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_GRID_H_
#define SEEN_SP_GRID_H_

#include "display/control/canvas-item-ptr.h"
#include "object/sp-object.h"
#include "svg/svg-bool.h"
#include "svg/svg-length.h"
#include "svg/svg-angle.h"
#include <memory>
#include <vector>

class SPDesktop;

namespace Inkscape {
    class CanvasItemGrid;
    class Snapper;

    namespace Util {
        class Unit;
    }
} // namespace Inkscape

enum class GridType
{
    RECTANGULAR,
    AXONOMETRIC
};

class SPGrid final : public SPObject {
public:
    SPGrid();
    ~SPGrid() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    static void create_new(SPDocument *doc, Inkscape::XML::Node *parent, GridType type);

    void setPrefValues();

    void show(SPDesktop *desktop);
    void hide(SPDesktop const *desktop);

    bool isEnabled() const;
    void setEnabled(bool v);

    bool isVisible() const { return isEnabled() && _visible; }
    void setVisible(bool v);

    bool isDotted() const { return _dotted; }
    void setDotted(bool v);

    bool getSnapToVisibleOnly() const { return _snap_to_visible_only; }
    void setSnapToVisibleOnly(bool v);

    guint32 getMajorColor() const { return _major_color; }
    void setMajorColor(const guint32 color);

    guint32 getMinorColor() const { return _minor_color; }
    void setMinorColor(const guint32 color);

    Geom::Point getOrigin() const;
    void setOrigin(Geom::Point const &new_origin);

    Geom::Point getSpacing() const;
    void setSpacing(Geom::Point const &spacing);

    guint32 getMajorLineInterval() const { return _major_line_interval; }
    void setMajorLineInterval(guint32 interval);

    double getAngleX() const { return _angle_x.computed; }
    void setAngleX(double deg);

    double getAngleZ() const { return _angle_z.computed; }
    void setAngleZ(double deg);

    const char *typeName() const;
    const char *displayName() const;

    GridType getType() const { return _grid_type; }
    const char *getSVGType() const;
    void setSVGType(const char *svgtype);

    void setUnit(const Glib::ustring &units);
    const Inkscape::Util::Unit *getUnit() const;

    bool isPixel() const { return _pixel; }
    bool isLegacy() const { return _legacy; }

    void scale(const Geom::Scale &scale);
    Inkscape::CanvasItemGrid *getAssociatedView(SPDesktop const *desktop);

    Inkscape::Snapper *snapper();

    std::pair<Geom::Point, Geom::Point> getEffectiveOriginAndSpacing() const;

    std::vector<CanvasItemPtr<Inkscape::CanvasItemGrid>> views;

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void set(SPAttr key, const char *value) override;
    void release() override;
    void modified(unsigned int flags) override;
    void update(SPCtx *ctx, unsigned int flags) override;

private:
    void _checkOldGrid(SPDocument *doc, Inkscape::XML::Node *repr);
    void _recreateViews();

    SVGBool _visible;
    SVGBool _enabled;
    SVGBool _snap_to_visible_only;
    SVGBool _dotted;
    SVGLength _origin_x;
    SVGLength _origin_y;
    SVGLength _spacing_x;
    SVGLength _spacing_y;
    SVGAngle _angle_x; // only for axonomgrid, stored in degrees
    SVGAngle _angle_z; // only for axonomgrid, stored in degrees

    guint32 _major_line_interval;

    guint32 _major_color;
    guint32 _minor_color;

    bool _pixel;        // is in user units
    bool _legacy;       // a grid from versions prior to inkscape 0.98

    GridType _grid_type;

    std::unique_ptr<Inkscape::Snapper> _snapper;

    Inkscape::Util::Unit const *_display_unit;

    sigc::connection _page_selected_connection;
    sigc::connection _page_modified_connection;
};

#endif // SEEN_SP_GRID_H_
