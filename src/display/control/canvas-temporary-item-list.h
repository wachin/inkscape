// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_CANVAS_TEMPORARY_ITEM_LIST_H
#define INKSCAPE_CANVAS_TEMPORARY_ITEM_LIST_H

/*
 * Authors:
 *   Johan Engelen
 *
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <list>

struct SPCanvasItem;
class SPDesktop;

namespace Inkscape {

class CanvasItem;

namespace Display {

class TemporaryItem;

/**
 * Provides a class that can contain active TemporaryItem[s] on a desktop.
 */
class TemporaryItemList  {
public:
    TemporaryItemList(SPDesktop *desktop);
    virtual ~TemporaryItemList();

    TemporaryItem* add_item (SPCanvasItem *item, unsigned int lifetime);
    TemporaryItem* add_item (CanvasItem *item, unsigned int lifetime);
    void           delete_item (TemporaryItem * tempitem);

protected:
    SPDesktop *desktop;   /** Desktop we are on. */

    std::list<TemporaryItem *> itemlist; /** list of temp items */ 

    void _item_timeout (TemporaryItem * tempitem);

private:
    TemporaryItemList(const TemporaryItemList&) = delete;
    TemporaryItemList& operator=(const TemporaryItemList&) = delete;
};

} //namespace Display
} //namespace Inkscape

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
