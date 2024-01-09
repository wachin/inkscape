// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * SPPage -- a page object.
 *//*
 * Authors:
 *   Martin Owens 2021
 * 
 * Copyright (C) 2021 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_PAGE_H
#define SEEN_SP_PAGE_H

#include <2geom/rect.h>
#include <vector>

#include "display/control/canvas-page.h"
#include "page-manager.h"
#include "sp-object.h"
#include "svg/svg-length.h"
#include "svg/svg-box.h"

class SPDesktop;
class SPItem;
namespace Inkscape {
    class ObjectSet;
}

class SPPage final : public SPObject
{
public:
    SPPage();
    ~SPPage() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    void movePage(Geom::Affine translate, bool with_objects);
    void swapPage(SPPage *other, bool with_objects);
    static void moveItems(Geom::Affine translate, std::vector<SPItem *> const &objects);

    // Canvas visualisation
    void showPage(Inkscape::CanvasItemGroup *fg, Inkscape::CanvasItemGroup *bg);
    void hidePage(Inkscape::UI::Widget::Canvas *canvas) { _canvas_item->remove(canvas); }
    void showPage() { _canvas_item->show(); }
    void hidePage() { _canvas_item->hide(); }
    void set_guides_visible(bool show);

    double getMarginSide(int side);
    const SVGBox &getMargin() const { return margin; }
    void setMargin(const std::string &value);
    void setMarginSide(int pos, double value, bool confine = false);
    void setMarginSide(int side, const std::string &value, bool confine = false);
    std::string getMarginLabel() const;

    const SVGBox &getBleed() const { return bleed; }
    void setBleed(const std::string &value);
    std::string getBleedLabel() const;

    void copyFrom(SPPage *page);
    void setSelected(bool selected);
    bool setDefaultAttributes();
    void setSizeLabel(std::string label);
    int getPageIndex() const;
    int getPagePosition() const { return getPageIndex() + 1; }
    bool setPageIndex(int index, bool swap_page);
    bool setPagePosition(int position, bool swap_page) { return setPageIndex(position - 1, swap_page); }
    bool isBarePage() const;

    // To sort the pages in the set by index/page number
    struct PageIndexOrder
    {
        bool operator()(const SPPage* Page1, const SPPage* Page2) const
        {
            return (Page1->getPageIndex() < Page2->getPageIndex());
        }
    };

    SPPage *getNextPage();
    SPPage *getPreviousPage();

    Geom::Rect getRect() const;
    Geom::Rect getDesktopRect() const;
    Geom::Rect getDesktopMargin() const;
    Geom::Rect getDesktopBleed() const;
    Geom::Rect getDocumentRect() const;
    Geom::Rect getDocumentMargin() const;
    Geom::Rect getDocumentBleed() const;
    Geom::Rect getSensitiveRect() const;
    void setRect(Geom::Rect rect);
    void setDocumentRect(Geom::Rect rect, bool add_margins = false);
    void setDesktopRect(Geom::Rect rect);
    void setSize(double width, double height);
    std::vector<SPItem *> getExclusiveItems(bool hidden = true, bool in_bleed = false, bool in_layers = true) const;
    std::vector<SPItem *> getOverlappingItems(bool hidden = true, bool in_bleed = false, bool in_layers = true) const;
    bool itemOnPage(SPItem *item, bool contains = false) const;
    bool isViewportPage() const;
    std::string getDefaultLabel() const;
    std::string getLabel() const;
    std::string getSizeLabel() const;

    Geom::Translate getDesktopAffine() const;

protected:
    void build(SPDocument *doc, Inkscape::XML::Node *repr) override;
    void release() override;
    void update(SPCtx *ctx, unsigned int flags) override;
    void set(SPAttr key, const char *value) override;
    Inkscape::XML::Node *write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) override;

    void update_relatives();
private:
    Inkscape::CanvasPage *_canvas_item = nullptr;

    SVGLength x;
    SVGLength y;
    SVGLength width;
    SVGLength height;
    SVGBox margin;
    SVGBox bleed;
    std::string _size_label;
};

#endif // SEEN_SP_PAGE_H

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
