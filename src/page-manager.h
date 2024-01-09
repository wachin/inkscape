// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape::PageManager - Multi-Page management.
 *
 * Copyright 2021 Martin Owens <doctormo@geek-2.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_PAGE_MANAGER_H
#define SEEN_INKSCAPE_PAGE_MANAGER_H

#include <vector>

#include "color-rgba.h"
#include "document.h"
#include "object/sp-namedview.h"
#include "svg/svg-bool.h"

class SPDesktop;
class SPPage;

namespace Inkscape {
class Selection;
class ObjectSet;
class CanvasPage;
namespace UI {
namespace Dialog {
class DocumentProperties;
}
} // namespace UI

class PageManager
{
public:
    PageManager(SPDocument *document);
    ~PageManager();

    static bool move_objects();
    const std::vector<SPPage *> &getPages() const { return pages; }
    std::vector<SPPage *> getPages(const std::string &pages, bool inverse) const;
    std::vector<SPPage *> getPages(std::set<unsigned int> indexes, bool inverse = false) const;

    void addPage(SPPage *page);
    void removePage(Inkscape::XML::Node *child);
    void reorderPage(Inkscape::XML::Node *child);

    // Returns None if no page selected
    SPPage *getSelected() const { return _selected_page; }
    SPPage *getPage(int index) const;
    SPPage *getPageAt(Geom::Point pos) const;
    SPPage *getFirstPage() const { return getPage(0); }
    SPPage *getLastPage() const { return getPage(pages.size() - 1); }
    SPPage *getViewportPage() const;
    std::vector<SPPage *> getPagesFor(SPItem *item, bool contains) const;
    SPPage *getPageFor(SPItem *item, bool contains) const;
    Geom::OptRect getDesktopRect() const;
    bool hasPages() const { return !pages.empty(); }
    int getPageCount() const { return pages.size(); }
    int getPageIndex(const SPPage *page) const;
    int getSelectedPageIndex() const;
    Geom::Rect getSelectedPageRect() const;
    Geom::Affine getSelectedPageAffine() const;
    Geom::Point nextPageLocation() const;

    void enablePages();
    void disablePages();
    void pagesChanged();
    bool selectPage(SPPage *page);
    bool selectPage(SPItem *item, bool contains);
    bool selectPage(int index) { return selectPage(getPage(index)); }
    bool selectNextPage() { return selectPage(getSelectedPageIndex() + 1); }
    bool selectPrevPage() { return selectPage(getSelectedPageIndex() - 1); }
    bool hasNextPage() const { return getSelectedPageIndex() + 1 < pages.size(); }
    bool hasPrevPage() const { return getSelectedPageIndex() - 1 >= 0; }

    ColorRGBA getDefaultBackgroundColor() const { return ColorRGBA(background_color); }

    void zoomToPage(SPDesktop *desktop, SPPage *page, bool width_only = false);
    void zoomToSelectedPage(SPDesktop *desktop, bool width_only = false) { zoomToPage(desktop, _selected_page, width_only); };
    void centerToPage(SPDesktop *desktop, SPPage *page);
    void centerToSelectedPage(SPDesktop *desktop) { centerToPage(desktop, _selected_page); };

    SPPage *newPage();
    SPPage *newPage(double width, double height);
    SPPage *newPage(Geom::Rect rect, bool first_page = false);
    SPPage *newDesktopPage(Geom::Rect rect, bool first_page = false);
    SPPage *newDocumentPage(Geom::Rect rect, bool first_page = false);
    void deletePage(SPPage *page, bool contents = false);
    void deletePage(bool contents = false);
    void resizePage(double width, double height);
    void resizePage(SPPage *page, double width, double height);
    void changeOrientation();
    void fitToSelection(ObjectSet *selection, bool add_margins = true);
    void fitToRect(Geom::OptRect box, SPPage *page, bool add_margins = false);

    bool subset(SPAttr key, const gchar *value);
    bool setDefaultAttributes(CanvasPage *item);
    bool showDefaultLabel() const { return label_style == "below"; }
    std::string getSizeLabel(SPPage *page = nullptr);
    std::string getSizeLabel(double width, double height);

    static void enablePages(SPDocument *document) { document->getPageManager().enablePages(); }
    static void disablePages(SPDocument *document) { document->getPageManager().disablePages(); }
    static SPPage *newPage(SPDocument *document) { return document->getPageManager().newPage(); }

    sigc::connection connectPageSelected(const sigc::slot<void (SPPage *)> &slot)
    {
        return _page_selected_signal.connect(slot);
    }
    sigc::connection connectPageModified(const sigc::slot<void (SPPage *)> &slot)
    {
        return _page_modified_signal.connect(slot);
    }
    sigc::connection connectPagesChanged(const sigc::slot<void ()> &slot)
    {
        return _pages_changed_signal.connect(slot);
    }

    // Access from export.cpp and others for the guint32
    guint32 background_color = 0xffffff00;
    guint32 margin_color = 0x1699d751;
    guint32 bleed_color = 0xbe310e31;

    void movePages(Geom::Affine tr);
    std::vector<SPItem *> getOverlappingItems(SPDesktop *desktop, SPPage *page, bool hidden = true, bool in_bleed = false, bool in_layers = true);

protected:
    friend class Inkscape::UI::Dialog::DocumentProperties;

    // Default settings from sp-namedview
    SVGBool border_show;
    SVGBool border_on_top;
    SVGBool shadow_show;
    SVGBool checkerboard;

    guint32 border_color = 0x0000003f;
    std::string label_style = "default";

private:
    SPDocument *_document;
    SPPage *_selected_page = nullptr;
    std::vector<SPPage *> pages;

    sigc::signal<void (SPPage *)> _page_selected_signal;
    sigc::signal<void (SPPage *)> _page_modified_signal;
    sigc::signal<void ()> _pages_changed_signal;

    sigc::connection _page_modified_connection;
};

} // namespace Inkscape

#endif
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
