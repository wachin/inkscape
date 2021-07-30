// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Selector component (click and rubberband)
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_TOOL_SELECTOR_H
#define SEEN_UI_TOOL_SELECTOR_H

#include <memory>
#include <gdk/gdk.h>
#include <2geom/rect.h>
#include "ui/tool/manipulator.h"

class SPDesktop;

namespace Inkscape {

class CanvasItemRect;

namespace UI {

class SelectorPoint;

class Selector : public Manipulator {
public:
    Selector(SPDesktop *d);
    ~Selector() override;
    bool event(Inkscape::UI::Tools::ToolBase *, GdkEvent *) override;
    virtual bool doubleClicked();
    
    sigc::signal<void, Geom::Rect const &, GdkEventButton*> signal_area;
    sigc::signal<void, Geom::Point const &, GdkEventButton*> signal_point;
private:
    SelectorPoint *_dragger;
    Geom::Point _start;
    friend class SelectorPoint;
};

} // namespace UI
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
