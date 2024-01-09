// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Canvas item belonging to an SVG drawing element.
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_DISPLAY_DRAWING_ITEM_H
#define INKSCAPE_DISPLAY_DRAWING_ITEM_H

#include <memory>
#include <list>
#include <exception>

#include <boost/operators.hpp>
#include <boost/utility.hpp>
#include <boost/intrusive/list.hpp>

#include <2geom/rect.h>
#include <2geom/affine.h>

#include "style-enums.h"
#include "tags.h"

namespace Glib { class ustring; }
class SPStyle;
class SPItem;

namespace Inkscape {

class Drawing;
class DrawingCache;
class DrawingItem;
class DrawingPattern;
class DrawingContext;
namespace Filters { class Filter; }

struct RenderContext
{
    uint32_t outline_color;
    bool dithering = false;
};

struct UpdateContext
{
    Geom::Affine ctm;
};

struct CacheData;

struct CacheRecord : boost::totally_ordered<CacheRecord>
{
    bool operator<(CacheRecord const &other) const { return score < other.score; }
    bool operator==(CacheRecord const &other) const { return score == other.score; }
    operator DrawingItem*() const { return item; }
    double score;
    size_t cache_size;
    DrawingItem *item;
};
using CacheList = std::list<CacheRecord>;

struct InvalidItemException : std::exception
{
    char const *what() const noexcept override { return "Invalid item in drawing"; }
};

class DrawingItem
{
public:
    enum RenderFlags
    {
        RENDER_DEFAULT           = 0,
        RENDER_CACHE_ONLY        = 1 << 0,
        RENDER_BYPASS_CACHE      = 1 << 1,
        RENDER_FILTER_BACKGROUND = 1 << 2,
        RENDER_OUTLINE           = 1 << 3,
        RENDER_NO_FILTERS        = 1 << 4,
        RENDER_VISIBLE_HAIRLINES = 1 << 5
    };
    enum StateFlags
    {
        STATE_NONE       = 0,
        STATE_BBOX       = 1 << 0, // bounding boxes are up-to-date
        STATE_CACHE      = 1 << 1, // cache extents and clean area are up-to-date
        STATE_PICK       = 1 << 2, // can process pick requests
        STATE_RENDER     = 1 << 3, // can be rendered
        STATE_BACKGROUND = 1 << 4, // filter background data is up to date
        STATE_ALL        = (1 << 5) - 1,
        STATE_TOTAL_INV  = 1 << 5, // used as a reset flag only
    };
    enum PickFlags
    {
        PICK_NORMAL  = 0,      // normal pick
        PICK_STICKY  = 1 << 0, // sticky pick - ignore visibility and sensitivity
        PICK_AS_CLIP = 1 << 1, // pick with no stroke and opaque fill regardless of item style
        PICK_OUTLINE = 1 << 2  // pick in outline mode
    };

    DrawingItem(Drawing &drawing);
    DrawingItem(DrawingItem const &) = delete;
    DrawingItem &operator=(DrawingItem const &) = delete;
    void unlink(); /// Unlink this node and its subtree from the rendering tree and destroy.
    virtual int tag() const { return tag_of<decltype(*this)>; }

    Geom::OptIntRect const &bbox() const { return _bbox; }
    Geom::OptIntRect const &drawbox() const { return _drawbox; }
    Geom::OptRect const &itemBounds() const { return _item_bbox; }
    Geom::Affine const &ctm() const { return _ctm; }
    Geom::Affine transform() const { return _transform ? *_transform : Geom::identity(); }
    Drawing &drawing() const { return _drawing; }
    DrawingItem *parent() const { return _parent; }
    bool isAncestorOf(DrawingItem const *item) const;
    int getUpdateComplexity() const { return _update_complexity; }
    bool unisolatedBlend() const;

    void appendChild(DrawingItem *item);
    void prependChild(DrawingItem *item);
    void clearChildren();

    bool visible() const { return _visible; }
    void setVisible(bool visible);
    bool sensitive() const { return _sensitive; }
    void setSensitive(bool sensitive);

    virtual void setStyle(SPStyle const *style, SPStyle const *context_style = nullptr);
    virtual void setChildrenStyle(SPStyle const *context_style);
    void setOpacity(float opacity);
    void setAntialiasing(unsigned antialias);
    unsigned antialiasing() const { return _antialias; }
    void setIsolation(bool isolation); // CSS Compositing and Blending
    void setBlendMode(SPBlendMode blend_mode);
    void setTransform(Geom::Affine const &trans);
    void setClip(DrawingItem *item);
    void setMask(DrawingItem *item);
    void setFillPattern(DrawingPattern *pattern);
    void setStrokePattern(DrawingPattern *pattern);
    void setZOrder(unsigned zorder);
    void setItemBounds(Geom::OptRect const &bounds);
    void setFilterRenderer(std::unique_ptr<Filters::Filter> renderer);

    void setKey(unsigned key) { _key = key; }
    unsigned key() const { return _key; }
    void setItem(SPItem *item) { _item = item; }
    SPItem *getItem() const { return _item; } // SPItem

    void update(Geom::IntRect const &area = Geom::IntRect::infinite(), UpdateContext const &ctx = UpdateContext(), unsigned flags = STATE_ALL, unsigned reset = 0);
    unsigned render(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area, unsigned flags = 0, DrawingItem const *stop_at = nullptr) const;
    unsigned render(DrawingContext &dc, Geom::IntRect const &area, unsigned flags = 0) const;
    void clip(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area) const;
    DrawingItem *pick(Geom::Point const &p, double delta, unsigned flags = 0);

    Glib::ustring name() const; // For debugging
    void recursivePrintTree(unsigned level = 0) const;  // For debugging

protected:
    enum class ChildType : unsigned char
    {
        ORPHAN = 0, // No parent - implies !parent.
        NORMAL = 1, // Contained in children of parent.
        CLIP   = 2, // Referenced by clip of parent.
        MASK   = 3, // Referenced by mask of parent.
        FILL   = 4, // Referenced by fill pattern of parent.
        STROKE = 5, // Referenced by stroke pattern of parent.
        ROOT   = 6  // Referenced by root of drawing.
    };
    enum RenderResult
    {
        RENDER_OK   = 0,
        RENDER_STOP = 1
    };
    virtual ~DrawingItem(); // Private to prevent deletion of items that are still in use by a snapshot.
    void _renderOutline(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area, unsigned flags) const;
    void _markForUpdate(unsigned state, bool propagate);
    void _markForRendering();
    void _invalidateFilterBackground(Geom::IntRect const &area);
    double _cacheScore();
    Geom::OptIntRect _cacheRect() const;
    void _setCached(bool cached, bool persistent = false);
    virtual unsigned _updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset) { return 0; }
    virtual unsigned _renderItem(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area, unsigned flags, DrawingItem const *stop_at) const { return RENDER_OK; }
    virtual void _clipItem(DrawingContext &dc, RenderContext &rc, Geom::IntRect const &area) const {}
    virtual DrawingItem *_pickItem(Geom::Point const &p, double delta, unsigned flags) { return nullptr; }
    virtual bool _canClip() const { return false; }
    virtual void _dropPatternCache() {}

    Drawing &_drawing;
    DrawingItem *_parent;

    using ListHook = boost::intrusive::list_member_hook<>;
    ListHook _child_hook;

    using ChildrenList = boost::intrusive::list<
        DrawingItem,
        boost::intrusive::member_hook<DrawingItem, ListHook, &DrawingItem::_child_hook>
        >;
    ChildrenList _children;

    // Todo: Try to get rid of all of these variables, moving them into the object tree.
    unsigned _key; ///< Auxiliary key used by the object tree for showing clips/masks/patterns.
    SPItem *_item; ///< Used to associate DrawingItems with SPItems that created them
    SPStyle const *_style; // Not used by DrawingGlyphs
    SPStyle const *_context_style; // Used for 'context-fill', 'context-stroke'

    float _opacity;
    std::unique_ptr<Geom::Affine> _transform; ///< Incremental transform from parent to this item's coords
    Geom::Affine _ctm; ///< Total transform from item coords to display coords
    Geom::OptIntRect _bbox; ///< Bounding box in display (pixel) coords including stroke
    Geom::OptIntRect _drawbox; ///< Full visual bounding box - enlarged by filters, shrunk by clips and masks
    Geom::OptRect _item_bbox; ///< Geometric bounding box in item's user space.
                              ///  This is used to compute the filter effect region and render in
                              ///  objectBoundingBox units.

    DrawingItem *_clip;
    DrawingItem *_mask;
    DrawingPattern *_fill_pattern;
    DrawingPattern *_stroke_pattern;
    std::unique_ptr<Inkscape::Filters::Filter> _filter;
    std::unique_ptr<CacheData> _cache;
    int _update_complexity = 0;
    bool _contains_unisolated_blend : 1;

    CacheList::iterator _cache_iterator;

    bool style_vector_effect_size   : 1;
    bool style_vector_effect_rotate : 1;
    bool style_vector_effect_fixed  : 1;

    unsigned _state : 8;
    unsigned _propagate_state : 8;
    ChildType _child_type : 3;
    unsigned _background_new : 1; ///< Whether enable-background: new is set for this element
    unsigned _background_accumulate : 1; ///< Whether this element accumulates background 
                                         ///  (has any ancestor with enable-background: new)
    unsigned _visible : 1;
    unsigned _sensitive : 1; ///< Whether this item responds to events
    unsigned _cached_persistent : 1; ///< If set, will always be cached regardless of score
    unsigned _has_cache_iterator : 1; ///< If set, _cache_iterator is valid
    unsigned _pick_children : 1; ///< For groups: if true, children are returned from pick(),
                                 ///  otherwise the group is returned
    unsigned _antialias : 2; ///< antialiasing level (NONE/FAST/GOOD(DEFAULT)/BEST)

    bool _isolation : 1;
    SPBlendMode _blend_mode;

    template<typename F>
    void defer(F &&f)
    {
        // Introduce artificial dependence on a template parameter to allow definition with Drawing forward-declared.
        auto &drawing = static_cast<std::enable_if_t<(sizeof(F) > 0), Drawing&>>(_drawing);
        drawing.defer(std::forward<F>(f));
    }

    friend class Drawing;
};

/// Apply antialias setting to Cairo.
void apply_antialias(DrawingContext &dc, int antialias);

} // namespace Inkscape

#endif // INKSCAPE_DISPLAY_DRAWING_ITEM_H

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
