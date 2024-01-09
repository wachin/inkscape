// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Cairo integration helpers.
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2010 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_DISPLAY_CAIRO_UTILS_H
#define SEEN_INKSCAPE_DISPLAY_CAIRO_UTILS_H

#include <2geom/forward.h>
#include <cairomm/cairomm.h>
#include "style.h"

struct SPColor;
typedef struct _GdkPixbuf GdkPixbuf;

void ink_cairo_pixbuf_cleanup(unsigned char *, void *);

namespace Inkscape {

/** Class to hold image data for raster images.
 * Allows easy interoperation with GdkPixbuf and Cairo. */
class Pixbuf {
public:
    enum PixelFormat {
        PF_CAIRO = 1,
        PF_GDK = 2,
        PF_LAST
    };

    explicit Pixbuf(cairo_surface_t *s);
    explicit Pixbuf(GdkPixbuf *pb);
    Pixbuf(Inkscape::Pixbuf const &other);
    ~Pixbuf();

    Pixbuf *cropTo(const Geom::IntRect &area) const;

    GdkPixbuf *getPixbufRaw(bool convert_format = true);
    GdkPixbuf *getPixbufRaw() const;

    cairo_surface_t *getSurfaceRaw();
    cairo_surface_t *getSurfaceRaw() const;
    Cairo::RefPtr<Cairo::Surface> getSurface();

    int width() const;
    int height() const;
    int rowstride() const;
    guchar const *pixels() const;
    guchar *pixels();
    void markDirty();

    bool hasMimeData() const;
    guchar const *getMimeData(gsize &len, std::string &mimetype) const;
    std::string const &originalPath() const { return _path; }
    time_t modificationTime() const { return _mod_time; }

    PixelFormat pixelFormat() const { return _pixel_format; }
    void ensurePixelFormat(PixelFormat fmt);
    static void ensure_pixbuf(GdkPixbuf *pb);
    static void ensure_argb32(GdkPixbuf *pb);

    static Pixbuf *create_from_data_uri(gchar const *uri, double svgdpi = 0);
    static Pixbuf *create_from_file(std::string const &fn, double svgddpi = 0);
    static Pixbuf *create_from_buffer(std::string const &, double svgddpi = 0, std::string const &fn = "");

  private:
    static Pixbuf *create_from_buffer(gchar *&&, gsize, double svgddpi = 0, std::string const &fn = "");
    static Geom::Affine get_embedded_orientation(GdkPixbuf *buf);
    static GdkPixbuf *apply_embedded_orientation(GdkPixbuf *buf);

    void _ensurePixelsARGB32();
    void _ensurePixelsPixbuf();
    void _forceAlpha();
    void _setMimeData(guchar *data, gsize len, Glib::ustring const &format);

    GdkPixbuf *_pixbuf;
    cairo_surface_t *_surface;
    time_t _mod_time;
    std::string _path;
    PixelFormat _pixel_format;
    bool _cairo_store;
};

} // namespace Inkscape

// Atomic accessors to global variable governing number of filter threads.
int  get_num_filter_threads();
void set_num_filter_threads(int);

SPColorInterpolation get_cairo_surface_ci(cairo_surface_t *surface);
void set_cairo_surface_ci(cairo_surface_t *surface, SPColorInterpolation cif);
void copy_cairo_surface_ci(cairo_surface_t *in, cairo_surface_t *out);
void convert_cairo_surface_ci(cairo_surface_t *surface, SPColorInterpolation cif);

void ink_cairo_set_source_color(cairo_t *ct, SPColor const &color, double opacity);
void ink_cairo_set_source_rgba32(cairo_t *ct, guint32 rgba);
void ink_cairo_transform(cairo_t *ct, Geom::Affine const &m);
void ink_cairo_pattern_set_matrix(cairo_pattern_t *cp, Geom::Affine const &m);
void ink_cairo_set_hairline(cairo_t *ct);
void ink_cairo_pattern_set_dither(cairo_pattern_t *pattern, bool enabled);

void ink_matrix_to_2geom(Geom::Affine &, cairo_matrix_t const &);
void ink_matrix_to_cairo(cairo_matrix_t &, Geom::Affine const &);
cairo_operator_t ink_css_blend_to_cairo_operator(SPBlendMode blend_mode);
SPBlendMode ink_cairo_operator_to_css_blend(cairo_operator_t cairo_operator);
cairo_surface_t *ink_cairo_surface_copy(cairo_surface_t *s);
Cairo::RefPtr<Cairo::ImageSurface> ink_cairo_surface_copy(Cairo::RefPtr<Cairo::ImageSurface> surface);
cairo_surface_t *ink_cairo_surface_create_identical(cairo_surface_t *s);
cairo_surface_t *ink_cairo_surface_create_same_size(cairo_surface_t *s, cairo_content_t c);
cairo_surface_t *ink_cairo_extract_alpha(cairo_surface_t *s);
cairo_surface_t *ink_cairo_surface_create_output(cairo_surface_t *image, cairo_surface_t *bg);
void ink_cairo_surface_blit(cairo_surface_t *src, cairo_surface_t *dest);
int ink_cairo_surface_get_width(cairo_surface_t *surface);
int ink_cairo_surface_get_height(cairo_surface_t *surface);
guint32 ink_cairo_surface_average_color(cairo_surface_t *surface);
guint32 ink_cairo_pattern_get_argb32(cairo_pattern_t *pattern);
void ink_cairo_surface_average_color(cairo_surface_t *surface, double &r, double &g, double &b, double &a);
void ink_cairo_surface_average_color_premul(cairo_surface_t *surface, double &r, double &g, double &b, double &a);

double srgb_to_linear( const double c );
int ink_cairo_surface_srgb_to_linear(cairo_surface_t *surface);
int ink_cairo_surface_linear_to_srgb(cairo_surface_t *surface);

cairo_pattern_t *ink_cairo_pattern_create_checkerboard(guint32 rgba = 0xC4C4C4FF, bool use_alpha = false);
// draw drop shadow around the 'rect' with given 'size' and 'color'; shadow extends to the right and bottom of rect
void ink_cairo_draw_drop_shadow(const Cairo::RefPtr<Cairo::Context> &ctx, const Geom::Rect& rect, double size, guint32 color, double color_alpha);

GdkPixbuf *ink_pixbuf_create_from_cairo_surface(cairo_surface_t *s);
void convert_pixels_pixbuf_to_argb32(guchar *data, int w, int h, int rs);
void convert_pixels_argb32_to_pixbuf(guchar *data, int w, int h, int rs, guint32 bgcolor=0);

G_GNUC_CONST guint32 argb32_from_pixbuf(guint32 in);
G_GNUC_CONST guint32 pixbuf_from_argb32(guint32 in, guint32 bgcolor=0);
const guchar* pixbuf_to_png(guchar const**rows, guchar* px, int nrows, int ncols, int stride, int color_type, int bit_depth);

/** Convert a pixel in 0xRRGGBBAA format to Cairo ARGB32 format. */
G_GNUC_CONST guint32 argb32_from_rgba(guint32 in);
/** Convert a pixel in 0xAARRGGBB format to 0xRRGGBBAA format. */
G_GNUC_CONST guint32 rgba_from_argb32(guint32 in);


G_GNUC_CONST inline guint32
premul_alpha(const guint32 color, const guint32 alpha)
{
    const guint32 temp = alpha * color + 128;
    return (temp + (temp >> 8)) >> 8;
}
G_GNUC_CONST inline guint32
unpremul_alpha(const guint32 color, const guint32 alpha)
{
    if (color >= alpha)
        return 0xff;
    return (255 * color + alpha/2) / alpha;
}

// TODO: move those to 2Geom
void feed_pathvector_to_cairo (cairo_t *ct, Geom::PathVector const &pathv, Geom::Affine trans, Geom::OptRect area, bool optimize_stroke, double stroke_width);
void feed_pathvector_to_cairo (cairo_t *ct, Geom::PathVector const &pathv);

std::optional<Geom::PathVector> extract_pathvector_from_cairo(cairo_t *ct);

#define EXTRACT_ARGB32(px,a,r,g,b) \
    guint32 a, r, g, b; \
    a = ((px) & 0xff000000) >> 24; \
    r = ((px) & 0x00ff0000) >> 16; \
    g = ((px) & 0x0000ff00) >> 8;  \
    b = ((px) & 0x000000ff);

#define ASSEMBLE_ARGB32(px,a,r,g,b) \
    guint32 px = (a << 24) | (r << 16) | (g << 8) | b;

inline double srgb_to_linear( const double c ) {
    if( c < 0.04045 ) {
        return c / 12.92;
    } else {
        return pow( (c+0.055)/1.055, 2.4 );
    }
}


namespace Inkscape {

namespace Display
{

inline void ExtractARGB32(guint32 px, guint32 &a, guint32 &r, guint32 &g, guint &b)
{
    a = ((px) & 0xff000000) >> 24;
    r = ((px) & 0x00ff0000) >> 16;
    g = ((px) & 0x0000ff00) >> 8;
    b = ((px) & 0x000000ff);
}

inline void ExtractRGB32(guint32 px, guint32 &r, guint32 &g, guint &b)
{
    r = ((px) & 0x00ff0000) >> 16;
    g = ((px) & 0x0000ff00) >> 8;
    b = ((px) & 0x000000ff);
}

inline guint AssembleARGB32(guint32 a, guint32 r, guint32 g, guint32 b)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

} // namespace Display

} // namespace Inkscape

#endif // SEEN_INKSCAPE_DISPLAY_CAIRO_UTILS_H

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
