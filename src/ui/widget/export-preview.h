// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_EXPORT_PREVIEW_H
#define INKSCAPE_UI_WIDGET_EXPORT_PREVIEW_H

#include <cstdint>
#include <2geom/rect.h>
#include <gtkmm.h>
#include "display/drawing.h"
#include "helper/auto-connection.h"
#include "async/channel.h"

class SPDocument;
class SPObject;
class SPItem;

namespace Inkscape {
class Drawing;

namespace UI {
namespace Dialog {
class ExportPreview;

class PreviewDrawing
{
public:
    PreviewDrawing(SPDocument *document);
    ~PreviewDrawing();

    bool render(ExportPreview *widget, uint32_t bg, SPItem *item, unsigned size, Geom::OptRect const &dboxIn);
    void set_shown_items(std::vector<SPItem*> &&list = {});

private:
    void destruct();
    void construct();

    SPDocument *_document = nullptr;
    std::shared_ptr<Inkscape::Drawing> _drawing;
    unsigned _visionkey = 0;
    bool _to_destruct = false;

    std::vector<SPItem*> _shown_items;
    Inkscape::auto_connection _construct_idle;
};

class ExportPreview final : public Gtk::Image
{
public:
    ExportPreview() = default;
    ExportPreview(BaseObjectType *cobj, Glib::RefPtr<Gtk::Builder> const &) : Gtk::Image(cobj) {}
    ~ExportPreview() override;

    void setDrawing(std::shared_ptr<PreviewDrawing> drawing);
    void setItem(SPItem *item);
    void setBox(Geom::Rect const &bbox);
    void queueRefresh();
    void resetPixels(bool new_size = false);
    void setSize(int newSize);
    void setPreview(Cairo::RefPtr<Cairo::ImageSurface>);
    void setBackgroundColor(uint32_t bg_color);

    static std::shared_ptr<Inkscape::Drawing> makeDrawing(SPDocument *doc);

private:
    int size = 128; // size of preview image
    sigc::connection refresh_conn;

    SPItem *_item = nullptr;
    Geom::OptRect _dbox;

    std::shared_ptr<PreviewDrawing> _drawing;
    uint32_t _bg_color = 0;

    Inkscape::auto_connection _render_idle;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_EXPORT_PREVIEW_H

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
