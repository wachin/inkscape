// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SVG_RENDERER_H
#define SEEN_SVG_RENDERER_H

#include <cstdint>
#include <memory>
#include <optional>
#include <gtkmm.h>
#include <gdkmm/rgba.h>

#include "document.h"
#include "color.h"

namespace Inkscape {

// utilities for set_style: 
// Gdk color to CSS rgb (no alpha)
Glib::ustring rgba_to_css_color(const Gdk::RGBA& color);
Glib::ustring rgba_to_css_color(const SPColor& color);
// double to low precision string
Glib::ustring double_to_css_value(double value);
class Pixbuf;

class svg_renderer
{
public:
    // load SVG document from file (abs path)
    svg_renderer(const char* svg_file_path);
    // pass in document to render
    svg_renderer(std::shared_ptr<SPDocument> document);

    // set inline style on selected elements; return number of elements modified
    size_t set_style(const Glib::ustring& selector, const char* name, const Glib::ustring& value);

    // render document at given scale
    Glib::RefPtr<Gdk::Pixbuf> render(double scale);
    Cairo::RefPtr<Cairo::Surface> render_surface(double scale);

    // if set, draw checkerboard pattern before image
    void set_checkerboard_color(uint32_t rgba);

    double get_width_px() const;
    double get_height_px() const;

private:
    Pixbuf* do_render(double scale);
    std::shared_ptr<SPDocument> _document;
    SPRoot* _root = nullptr;
    std::optional<uint32_t> _checkerboard;
};

}

#endif
