// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Rendering with Cairo.
 */
/*
 * Author:
 *   Miklos Erdelyi <erdelyim@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2006 Miklos Erdelyi
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#ifndef PANGO_ENABLE_BACKEND
#define PANGO_ENABLE_BACKEND
#endif

#ifndef PANGO_ENABLE_ENGINE
#define PANGO_ENABLE_ENGINE
#endif

#include "cairo-render-context.h"

#include <csignal>
#include <cerrno>
#include <2geom/pathvector.h>

#include <glib.h>
#include <glibmm/i18n.h>

#include "display/drawing.h"
#include "display/curve.h"
#include "display/cairo-utils.h"
#include "display/drawing-paintserver.h"

#include "object/sp-item.h"
#include "object/sp-item-group.h"
#include "object/sp-hatch.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-radial-gradient.h"
#include "object/sp-mesh-gradient.h"
#include "object/sp-pattern.h"
#include "object/sp-mask.h"
#include "object/sp-clippath.h"

#include "util/units.h"

#include "cairo-renderer.h"
#include "extension/system.h"

#include "io/sys.h"

#include "svg/stringstream.h"

#include <cairo.h>

// include support for only the compiled-in surface types
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif


#ifdef CAIRO_HAS_FT_FONT
#include <cairo-ft.h>
#endif
#ifdef CAIRO_HAS_WIN32_FONT
#include <pango/pangowin32.h>
#include <cairo-win32.h>
#endif

#include <pango/pangofc-fontmap.h>

//#define TRACE(_args) g_printf _args
//#define TRACE(_args) g_message _args
#define TRACE(_args)
//#define TEST(_args) _args
#define TEST(_args)

// FIXME: expose these from sp-clippath/mask.cpp
/*struct SPClipPathView {
    SPClipPathView *next;
    unsigned int key;
    Inkscape::DrawingItem *arenaitem;
    Geom::OptRect bbox;
};

struct SPMaskView {
    SPMaskView *next;
    unsigned int key;
    Inkscape::DrawingItem *arenaitem;
    Geom::OptRect bbox;
};*/

namespace Inkscape {
namespace Extension {
namespace Internal {

static cairo_status_t _write_callback(void *closure, const unsigned char *data, unsigned int length);

CairoRenderContext::CairoRenderContext(CairoRenderer *parent) :
    _dpi(72),
    _pdf_level(1),
    _is_pdf(false),
    _is_ps(false),
    _ps_level(1),
    _eps(false),
    _is_texttopath(FALSE),
    _is_omittext(FALSE),
    _is_filtertobitmap(FALSE),
    _is_show_page(false),
    _bitmapresolution(72),
    _stream(nullptr),
    _is_valid(FALSE),
    _vector_based_target(FALSE),
    _cr(nullptr), // Cairo context
    _surface(nullptr),
    _target(CAIRO_SURFACE_TYPE_IMAGE),
    _target_format(CAIRO_FORMAT_ARGB32),
    _layout(nullptr),
    _state(nullptr),
    _renderer(parent),
    _render_mode(RENDER_MODE_NORMAL),
    _clip_mode(CLIP_MODE_MASK),
    _omittext_state(EMPTY)
{
}

CairoRenderContext::~CairoRenderContext()
{
    for (std::map<gpointer, cairo_font_face_t *>::const_iterator iter = font_table.begin(); iter != font_table.end(); ++iter)
        font_data_free(iter->second);

    if (_cr) cairo_destroy(_cr);
    if (_surface) cairo_surface_destroy(_surface);
    if (_layout) g_object_unref(_layout);
}
void CairoRenderContext::font_data_free(gpointer data)
{
    cairo_font_face_t *font_face = (cairo_font_face_t *)data;
    if (font_face) {
        cairo_font_face_destroy(font_face);
    }
}

CairoRenderer* CairoRenderContext::getRenderer() const
{
    return _renderer;
}

CairoRenderState* CairoRenderContext::getCurrentState() const
{
    return _state;
}

CairoRenderState* CairoRenderContext::getParentState() const
{
    // if this is the root node just return it
    if (_state_stack.size() == 1) {
        return _state;
    } else {
        return _state_stack[_state_stack.size()-2];
    }
}

void CairoRenderContext::setStateForStyle(SPStyle const *style)
{
    // only opacity & overflow is stored for now
    _state->opacity = SP_SCALE24_TO_FLOAT(style->opacity.value);
    _state->has_overflow = (style->overflow.set && style->overflow.value != SP_CSS_OVERFLOW_VISIBLE);
    _state->has_filtereffect = (style->filter.set != 0) ? TRUE : FALSE;

    if (style->fill.isPaintserver() || style->stroke.isPaintserver())
        _state->merge_opacity = FALSE;

    // disable rendering of opacity if there's a stroke on the fill
    if (_state->merge_opacity
        && !style->fill.isNone()
        && !style->stroke.isNone())
        _state->merge_opacity = FALSE;
}

/**
 * \brief Creates a new render context which will be compatible with the given context's Cairo surface
 *
 * \param width     width of the surface to be created
 * \param height    height of the surface to be created
 */
CairoRenderContext*
CairoRenderContext::cloneMe(double width, double height) const
{
    g_assert( _is_valid );
    g_assert( width > 0.0 && height > 0.0 );

    CairoRenderContext *new_context = _renderer->createContext();
    cairo_surface_t *surface = cairo_surface_create_similar(cairo_get_target(_cr), CAIRO_CONTENT_COLOR_ALPHA,
                                                            (int)ceil(width), (int)ceil(height));
    new_context->_cr = cairo_create(surface);
    new_context->_surface = surface;
    new_context->_width = width;
    new_context->_height = height;
    new_context->_is_valid = TRUE;

    return new_context;
}

CairoRenderContext* CairoRenderContext::cloneMe() const
{
    g_assert( _is_valid );

    return cloneMe(_width, _height);
}

bool CairoRenderContext::setImageTarget(cairo_format_t format)
{
    // format cannot be set on an already initialized surface
    if (_is_valid)
        return false;

    switch (format) {
        case CAIRO_FORMAT_ARGB32:
        case CAIRO_FORMAT_RGB24:
        case CAIRO_FORMAT_A8:
        case CAIRO_FORMAT_A1:
            _target_format = format;
            _target = CAIRO_SURFACE_TYPE_IMAGE;
            return true;
            break;
        default:
            break;
    }

    return false;
}

bool CairoRenderContext::setPdfTarget(gchar const *utf8_fn)
{
#ifndef CAIRO_HAS_PDF_SURFACE
    return false;
#else
    _target = CAIRO_SURFACE_TYPE_PDF;
    _vector_based_target = TRUE;
#endif

    FILE *osf = nullptr;
    FILE *osp = nullptr;

    gsize bytesRead = 0;
    gsize bytesWritten = 0;
    GError *error = nullptr;
    gchar *local_fn = g_filename_from_utf8(utf8_fn,
                                           -1,  &bytesRead,  &bytesWritten, &error);
    gchar const *fn = local_fn;

    /* TODO: Replace the below fprintf's with something that does the right thing whether in
    * gui or batch mode (e.g. --print=blah).  Consider throwing an exception: currently one of
    * the callers (sp_print_document_to_file, "ret = mod->begin(doc)") wrongly ignores the
    * return code.
    */
    if (fn != nullptr) {
        if (*fn == '|') {
            fn += 1;
            while (isspace(*fn)) fn += 1;
#ifndef _WIN32
            osp = popen(fn, "w");
#else
            osp = _popen(fn, "w");
#endif
            if (!osp) {
                fprintf(stderr, "inkscape: popen(%s): %s\n",
                        fn, strerror(errno));
                return false;
            }
            _stream = osp;
        } else if (*fn == '>') {
            fn += 1;
            while (isspace(*fn)) fn += 1;
            Inkscape::IO::dump_fopen_call(fn, "K");
            osf = Inkscape::IO::fopen_utf8name(fn, "w+");
            if (!osf) {
                fprintf(stderr, "inkscape: fopen(%s): %s\n",
                        fn, strerror(errno));
                return false;
            }
            _stream = osf;
        } else {
            /* put cwd stuff in here */
            gchar *qn = ( *fn
                    ? g_strdup_printf("lpr -P %s", fn)  /* FIXME: quote fn */
                : g_strdup("lpr") );
#ifndef _WIN32
            osp = popen(qn, "w");
#else
            osp = _popen(qn, "w");
#endif
            if (!osp) {
                fprintf(stderr, "inkscape: popen(%s): %s\n",
                        qn, strerror(errno));
                return false;
            }
            g_free(qn);
            _stream = osp;
        }
    }

    g_free(local_fn);

    if (_stream) {
        /* fixme: this is kinda icky */
#if !defined(_WIN32) && !defined(__WIN32__)
        (void) signal(SIGPIPE, SIG_IGN);
#endif
    }

    return true;
}

bool CairoRenderContext::setPsTarget(gchar const *utf8_fn)
{
#ifndef CAIRO_HAS_PS_SURFACE
    return false;
#else
    _target = CAIRO_SURFACE_TYPE_PS;
    _vector_based_target = TRUE;
#endif

    FILE *osf = nullptr;
    FILE *osp = nullptr;

    gsize bytesRead = 0;
    gsize bytesWritten = 0;
    GError *error = nullptr;
    gchar *local_fn = g_filename_from_utf8(utf8_fn,
                                           -1,  &bytesRead,  &bytesWritten, &error);
    gchar const *fn = local_fn;

    /* TODO: Replace the below fprintf's with something that does the right thing whether in
    * gui or batch mode (e.g. --print=blah).  Consider throwing an exception: currently one of
    * the callers (sp_print_document_to_file, "ret = mod->begin(doc)") wrongly ignores the
    * return code.
    */
    if (fn != nullptr) {
        if (*fn == '|') {
            fn += 1;
            while (isspace(*fn)) fn += 1;
#ifndef _WIN32
            osp = popen(fn, "w");
#else
            osp = _popen(fn, "w");
#endif
            if (!osp) {
                fprintf(stderr, "inkscape: popen(%s): %s\n",
                        fn, strerror(errno));
                return false;
            }
            _stream = osp;
        } else if (*fn == '>') {
            fn += 1;
            while (isspace(*fn)) fn += 1;
            Inkscape::IO::dump_fopen_call(fn, "K");
            osf = Inkscape::IO::fopen_utf8name(fn, "w+");
            if (!osf) {
                fprintf(stderr, "inkscape: fopen(%s): %s\n",
                        fn, strerror(errno));
                return false;
            }
            _stream = osf;
        } else {
            /* put cwd stuff in here */
            gchar *qn = ( *fn
                    ? g_strdup_printf("lpr -P %s", fn)  /* FIXME: quote fn */
                : g_strdup("lpr") );
#ifndef _WIN32
            osp = popen(qn, "w");
#else
            osp = _popen(qn, "w");
#endif
            if (!osp) {
                fprintf(stderr, "inkscape: popen(%s): %s\n",
                        qn, strerror(errno));
                return false;
            }
            g_free(qn);
            _stream = osp;
        }
    }

    g_free(local_fn);

    if (_stream) {
        /* fixme: this is kinda icky */
#if !defined(_WIN32) && !defined(__WIN32__)
        (void) signal(SIGPIPE, SIG_IGN);
#endif
    }

    return true;
}

void CairoRenderContext::setPSLevel(unsigned int level)
{
    _ps_level = level;
    _is_pdf = false;
    _is_ps = true;
}

void CairoRenderContext::setEPS(bool eps)
{
    _eps = eps;
}

unsigned int CairoRenderContext::getPSLevel()
{
    return _ps_level;
}

void CairoRenderContext::setPDFLevel(unsigned int level)
{
    _pdf_level = level;
    _is_pdf = true;
    _is_ps = false;
}

void CairoRenderContext::setTextToPath(bool texttopath)
{
    _is_texttopath = texttopath;
}

void CairoRenderContext::setOmitText(bool omittext)
{
    _is_omittext = omittext;
}

bool CairoRenderContext::getOmitText()
{
    return _is_omittext;
}

void CairoRenderContext::setFilterToBitmap(bool filtertobitmap)
{
    _is_filtertobitmap = filtertobitmap;
}

bool CairoRenderContext::getFilterToBitmap()
{
    return _is_filtertobitmap;
}

void CairoRenderContext::setBitmapResolution(int resolution)
{
    _bitmapresolution = resolution;
}

int CairoRenderContext::getBitmapResolution()
{
    return _bitmapresolution;
}

cairo_surface_t*
CairoRenderContext::getSurface()
{
    g_assert( _is_valid );

    return _surface;
}

bool
CairoRenderContext::saveAsPng(const char *file_name)
{
    cairo_status_t status = cairo_surface_write_to_png(_surface, file_name);
    if (status)
        return false;
    else
        return true;
}

void
CairoRenderContext::setRenderMode(CairoRenderMode mode)
{
    switch (mode) {
        case RENDER_MODE_NORMAL:
        case RENDER_MODE_CLIP:
            _render_mode = mode;
            break;
        default:
            _render_mode = RENDER_MODE_NORMAL;
            break;
    }
}

CairoRenderContext::CairoRenderMode
CairoRenderContext::getRenderMode() const
{
    return _render_mode;
}

void
CairoRenderContext::setClipMode(CairoClipMode mode)
{
    switch (mode) {
        case CLIP_MODE_PATH: // Clip is rendered as a path for vector output
        case CLIP_MODE_MASK: // Clip is rendered as a bitmap for raster output.
            _clip_mode = mode;
            break;
        default:
            _clip_mode = CLIP_MODE_PATH;
            break;
    }
}

CairoRenderContext::CairoClipMode
CairoRenderContext::getClipMode() const
{
    return _clip_mode;
}

CairoRenderState* CairoRenderContext::_createState()
{
    CairoRenderState *state = static_cast<CairoRenderState*>(g_try_malloc(sizeof(CairoRenderState)));
    g_assert( state != nullptr );

    state->has_filtereffect = FALSE;
    state->merge_opacity = TRUE;
    state->opacity = 1.0;
    state->need_layer = FALSE;
    state->has_overflow = FALSE;
    state->parent_has_userspace = FALSE;
    state->clip_path = nullptr;
    state->mask = nullptr;

    return state;
}

void CairoRenderContext::pushLayer()
{
    g_assert( _is_valid );

    TRACE(("--pushLayer\n"));
    cairo_push_group(_cr);

    // clear buffer
    if (!_vector_based_target) {
        cairo_save(_cr);
        cairo_set_operator(_cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(_cr);
        cairo_restore(_cr);
    }
}

void
CairoRenderContext::popLayer(cairo_operator_t composite)
{
    g_assert( _is_valid );

    float opacity = _state->opacity;
    TRACE(("--popLayer w/ opacity %f\n", opacity));

    /*
     At this point, the Cairo source is ready. A Cairo mask must be created if required.
     Care must be taken of transformatons as Cairo, like PS and PDF, treats clip paths and
     masks independently of the objects they effect while in SVG the clip paths and masks
     are defined relative to the objects they are attached to.
     Notes:
     1. An SVG object may have both a clip path and a mask!
     2. An SVG clip path can be composed of an object with a clip path. This is not handled properly.
     3. An SVG clipped or masked object may be first drawn off the page and then translated onto
        the page (document). This is also not handled properly.
     4. The code converts all SVG masks to bitmaps. This shouldn't be necessary.
     5. Cairo expects a mask to use only the alpha channel. SVG masks combine the RGB luminance with
        alpha. This is handled here by doing a pixel by pixel conversion.
    */

    SPClipPath *clip_path = _state->clip_path;
    SPMask *mask = _state->mask;
    if (clip_path || mask) {

        CairoRenderContext *clip_ctx = nullptr;
        cairo_surface_t *clip_mask = nullptr;

        // Apply any clip path first
        if (clip_path) {
            TRACE(("  Applying clip\n"));
            if (_render_mode == RENDER_MODE_CLIP)
                mask = nullptr;    // disable mask when performing nested clipping

            if (_vector_based_target) {
                setClipMode(CLIP_MODE_PATH); // Vector
                if (!mask) {
                    cairo_pop_group_to_source(_cr);
                    _renderer->applyClipPath(this, clip_path); // Uses cairo_clip()
                    if (opacity == 1.0)
                        cairo_paint(_cr);
                    else
                        cairo_paint_with_alpha(_cr, opacity);

                } else {
                    // the clipPath will be applied before masking
                }
            } else {

                // setup a new rendering context
                clip_ctx = _renderer->createContext();
                clip_ctx->setImageTarget(CAIRO_FORMAT_A8);
                clip_ctx->setClipMode(CLIP_MODE_MASK);  // Raster
                // This code ties the clipping to the document coordinates. It doesn't allow
                // for a clipped object initially drawn off the page and then translated onto
                // the page.
                if (!clip_ctx->setupSurface(_width, _height)) {
                    TRACE(("clip: setupSurface failed\n"));
                    _renderer->destroyContext(clip_ctx);
                    return;
                }

                // clear buffer
                cairo_save(clip_ctx->_cr);
                cairo_set_operator(clip_ctx->_cr, CAIRO_OPERATOR_CLEAR);
                cairo_paint(clip_ctx->_cr);
                cairo_restore(clip_ctx->_cr);

                // If a mask won't be applied set opacity too. (The clip is represented by a solid Cairo mask.)
                if (!mask)
                    cairo_set_source_rgba(clip_ctx->_cr, 1.0, 1.0, 1.0, opacity);
                else
                    cairo_set_source_rgba(clip_ctx->_cr, 1.0, 1.0, 1.0, 1.0);

                // copy over the correct CTM
                // It must be stored in item_transform of current state after pushState.
                Geom::Affine item_transform;
                if (_state->parent_has_userspace)
                    item_transform = getParentState()->transform * _state->item_transform;
                else
                    item_transform = _state->item_transform;

                // apply the clip path
                clip_ctx->pushState();
                clip_ctx->getCurrentState()->item_transform = item_transform;
                _renderer->applyClipPath(clip_ctx, clip_path);
                clip_ctx->popState();

                clip_mask = clip_ctx->getSurface();
                TEST(clip_ctx->saveAsPng("clip_mask.png"));

                if (!mask) {
                    cairo_pop_group_to_source(_cr);
                    if (composite != CAIRO_OPERATOR_CLEAR){
                        cairo_set_operator(_cr, composite);
                    }
                    cairo_mask_surface(_cr, clip_mask, 0, 0);
                    _renderer->destroyContext(clip_ctx);
                }
            }
        }

        // Apply any mask second
        if (mask) {
            TRACE(("  Applying mask\n"));
            // create rendering context for mask
            CairoRenderContext *mask_ctx = _renderer->createContext();

            // Fix Me: This is a kludge. PDF and PS output is set to 72 dpi but the
            // Cairo surface is expecting the mask to be 96 dpi.
            float surface_width = _width;
            float surface_height = _height;
            if( _vector_based_target ) {
                surface_width *= 4.0/3.0;
                surface_height *= 4.0/3.0;
            }
            if (!mask_ctx->setupSurface( surface_width, surface_height )) {
                TRACE(("mask: setupSurface failed\n"));
                _renderer->destroyContext(mask_ctx);
                return;
            }
            TRACE(("mask surface: %f x %f at %i dpi\n", surface_width, surface_height, _dpi ));

            // Mask should start black, but it is created white.
            cairo_set_source_rgba(mask_ctx->_cr, 0.0, 0.0, 0.0, 1.0);
            cairo_rectangle(mask_ctx->_cr, 0, 0, surface_width, surface_height);
            cairo_fill(mask_ctx->_cr);

            // set rendering mode to normal
            setRenderMode(RENDER_MODE_NORMAL);

            // copy the correct CTM to mask context
            /*
            if (_state->parent_has_userspace)
                mask_ctx->setTransform(getParentState()->transform);
            else
                mask_ctx->setTransform(_state->transform);
            */
            // This is probably not correct... but it seems to do the trick.
            mask_ctx->setTransform(_state->item_transform);

            // render mask contents to mask_ctx
            _renderer->applyMask(mask_ctx, mask);

            TEST(mask_ctx->saveAsPng("mask.png"));

            // composite with clip mask
            if (clip_path && _clip_mode == CLIP_MODE_MASK) {
                cairo_mask_surface(mask_ctx->_cr, clip_mask, 0, 0);
                _renderer->destroyContext(clip_ctx);
            }

            cairo_surface_t *mask_image = mask_ctx->getSurface();
            int width = cairo_image_surface_get_width(mask_image);
            int height = cairo_image_surface_get_height(mask_image);
            int stride = cairo_image_surface_get_stride(mask_image);
            unsigned char *pixels = cairo_image_surface_get_data(mask_image);

            // In SVG, the rgb channels as well as the alpha channel is used in masking.
            // In Cairo, only the alpha channel is used thus requiring this conversion.
            // SVG specifies that RGB be converted to alpha using luminance-to-alpha.
            // Notes: This calculation assumes linear RGB values. VERIFY COLOR SPACE!
            // The incoming pixel values already include alpha, fill-opacity, etc.,
            // however, opacity must still be applied.
            TRACE(("premul w/ %f\n", opacity));
            const float coeff_r = 0.2125 / 255.0;
            const float coeff_g = 0.7154 / 255.0;
            const float coeff_b = 0.0721 / 255.0;
            for (int row = 0 ; row < height; row++) {
                unsigned char *row_data = pixels + (row * stride);
                for (int i = 0 ; i < width; i++) {
                    guint32 *pixel = reinterpret_cast<guint32 *>(row_data) + i;
                    float lum_alpha = (((*pixel & 0x00ff0000) >> 16) * coeff_r +
                                       ((*pixel & 0x0000ff00) >>  8) * coeff_g +
                                       ((*pixel & 0x000000ff)      ) * coeff_b );
                    // lum_alpha can be slightly greater than 1 due to rounding errors...
                    // but this should be OK since it doesn't matter what the lower
                    // six hexadecimal numbers of *pixel are.
                    *pixel = (guint32)(0xff000000 * lum_alpha * opacity);
                }
            }

            cairo_pop_group_to_source(_cr);
            if (composite != CAIRO_OPERATOR_CLEAR){
                cairo_set_operator(_cr, composite);
            }
            if (_clip_mode == CLIP_MODE_PATH) {
                // we have to do the clipping after cairo_pop_group_to_source
                _renderer->applyClipPath(this, clip_path);
            }
            // apply the mask onto the layer
            cairo_mask_surface(_cr, mask_image, 0, 0);
            _renderer->destroyContext(mask_ctx);
        }
    } else {
        // No clip path or mask
        cairo_pop_group_to_source(_cr);
        if (composite != CAIRO_OPERATOR_CLEAR){
            cairo_set_operator(_cr, composite);
        }
        if (opacity == 1.0)
            cairo_paint(_cr);
        else
            cairo_paint_with_alpha(_cr, opacity);
    }
}
void CairoRenderContext::tagBegin(const char* l){
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 15, 4)
    char* link = g_strdup_printf("uri='%s'", l);
    cairo_tag_begin(_cr, CAIRO_TAG_LINK, link);
    g_free(link);
#endif
}

void CairoRenderContext::tagEnd(){
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 15, 4)
    cairo_tag_end(_cr, CAIRO_TAG_LINK);
#endif
}


void
CairoRenderContext::addClipPath(Geom::PathVector const &pv, SPIEnum<SPWindRule> const *fill_rule)
{
    g_assert( _is_valid );

    // here it should be checked whether the current clip winding changed
    // so we could switch back to masked clipping
    if (fill_rule->value == SP_WIND_RULE_EVENODD) {
        cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_EVEN_ODD);
    } else {
        cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_WINDING);
    }
    addPathVector(pv);
}

void
CairoRenderContext::addClippingRect(double x, double y, double width, double height)
{
    g_assert( _is_valid );

    cairo_rectangle(_cr, x, y, width, height);
    cairo_clip(_cr);
}

bool
CairoRenderContext::setupSurface(double width, double height)
{
    // Is the surface already set up?
    if (_is_valid)
        return true;

    if (_vector_based_target && _stream == nullptr)
        return false;

    _width = width;
    _height = height;

    cairo_surface_t *surface = nullptr;
    cairo_matrix_t ctm;
    cairo_matrix_init_identity (&ctm);
    switch (_target) {
        case CAIRO_SURFACE_TYPE_IMAGE:
            surface = cairo_image_surface_create(_target_format, (int)ceil(width), (int)ceil(height));
            break;
#ifdef CAIRO_HAS_PDF_SURFACE
        case CAIRO_SURFACE_TYPE_PDF:
            surface = cairo_pdf_surface_create_for_stream(Inkscape::Extension::Internal::_write_callback, _stream, width, height);
            cairo_pdf_surface_restrict_to_version(surface, (cairo_pdf_version_t)_pdf_level);
            break;
#endif
#ifdef CAIRO_HAS_PS_SURFACE
        case CAIRO_SURFACE_TYPE_PS:
            surface = cairo_ps_surface_create_for_stream(Inkscape::Extension::Internal::_write_callback, _stream, width, height);
            if(CAIRO_STATUS_SUCCESS != cairo_surface_status(surface)) {
                return FALSE;
            }
            cairo_ps_surface_restrict_to_level(surface, (cairo_ps_level_t)_ps_level);
            cairo_ps_surface_set_eps(surface, (cairo_bool_t) _eps);
            break;
#endif
        default:
            return false;
            break;
    }

    _setSurfaceMetadata(surface);

    return _finishSurfaceSetup (surface, &ctm);
}

bool
CairoRenderContext::setSurfaceTarget(cairo_surface_t *surface, bool is_vector, cairo_matrix_t *ctm)
{
    if (_is_valid || !surface)
        return false;

    _vector_based_target = is_vector;
    bool ret = _finishSurfaceSetup (surface, ctm);
    if (ret)
        cairo_surface_reference (surface);
    return ret;
}

bool
CairoRenderContext::_finishSurfaceSetup(cairo_surface_t *surface, cairo_matrix_t *ctm)
{
    if(surface == nullptr) {
        return false;
    }
    if(CAIRO_STATUS_SUCCESS != cairo_surface_status(surface)) {
        return false;
    }

    _cr = cairo_create(surface);
    if(CAIRO_STATUS_SUCCESS != cairo_status(_cr)) {
        return false;
    }
    if (ctm)
        cairo_set_matrix(_cr, ctm);
    _surface = surface;

    if (_vector_based_target) {
        cairo_scale(_cr, Inkscape::Util::Quantity::convert(1, "px", "pt"), Inkscape::Util::Quantity::convert(1, "px", "pt"));
    } else if (cairo_surface_get_content(_surface) != CAIRO_CONTENT_ALPHA) {
        // set background color on non-alpha surfaces
        // TODO: bgcolor should be derived from SPDocument (see IconImpl)
        cairo_set_source_rgb(_cr, 1.0, 1.0, 1.0);
        cairo_rectangle(_cr, 0, 0, _width, _height);
        cairo_fill(_cr);
    }

    _is_valid = TRUE;

    return true;
}

void
CairoRenderContext::_setSurfaceMetadata(cairo_surface_t *surface)
{
    switch (_target) {
#if defined CAIRO_HAS_PDF_SURFACE && CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 15, 4)
        case CAIRO_SURFACE_TYPE_PDF:
            if (!_metadata.title.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_TITLE, _metadata.title.c_str());
            }
            if (!_metadata.author.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_AUTHOR, _metadata.author.c_str());
            }
            if (!_metadata.subject.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_SUBJECT, _metadata.subject.c_str());
            }
            if (!_metadata.keywords.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_KEYWORDS, _metadata.keywords.c_str());
            }
            if (!_metadata.creator.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_CREATOR, _metadata.creator.c_str());
            }
            if (!_metadata.cdate.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_CREATE_DATE, _metadata.cdate.c_str());
            }
            if (!_metadata.mdate.empty()) {
                cairo_pdf_surface_set_metadata(surface, CAIRO_PDF_METADATA_MOD_DATE, _metadata.mdate.c_str());
            }
            break;
#endif
#if defined CAIRO_HAS_PS_SURFACE
        case CAIRO_SURFACE_TYPE_PS:
            if (!_metadata.title.empty()) {
                cairo_ps_surface_dsc_comment(surface, (Glib::ustring("%%Title: ") + _metadata.title).c_str());
            }
            if (!_metadata.copyright.empty()) {
                cairo_ps_surface_dsc_comment(surface, (Glib::ustring("%%Copyright: ") + _metadata.copyright).c_str());
            }
            break;
#endif
        default:
            g_warning("unsupported target %d\n", _target);
    }
}

/**
 * Each page that's made should call finishPage to complete it.
 */
bool
CairoRenderContext::finishPage()
{
    g_assert(_is_valid);
    if (!_vector_based_target)
        return false;

    // Protect against finish() showing one too many pages.
    if (!_is_show_page) {
        cairo_show_page(_cr);
        _is_show_page = true;
    }

    auto status = cairo_status(_cr);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_critical("error while rendering page: %s", cairo_status_to_string(status));
        return false;
    }
    return true;
}

/**
 * When writing multiple pages, resize the next page.
 */
bool
CairoRenderContext::nextPage(double width, double height, char const *label)
{
    g_assert(_is_valid);
    if (!_vector_based_target)
        return false;

    _width = width;
    _height = height;
    _is_show_page = false;

    if (_is_pdf) {
        cairo_pdf_surface_set_size(_surface, width, height);

        if (label) {
            cairo_pdf_surface_set_page_label(_surface, label);
        }
    }
    if (_is_ps) {
        cairo_ps_surface_set_size(_surface, width, height);
    }

    auto status = cairo_surface_status(_surface);
    if (status != CAIRO_STATUS_SUCCESS) {
        g_critical("error while sizing page: %s", cairo_status_to_string(status));
        return false;
    }
    return true;
}


bool
CairoRenderContext::finish(bool finish_surface)
{
    g_assert( _is_valid );

    if (_vector_based_target && !_is_show_page && finish_surface)
        cairo_show_page(_cr);

    cairo_status_t status = cairo_status(_cr);
    if (status != CAIRO_STATUS_SUCCESS)
        g_critical("error while rendering output: %s", cairo_status_to_string(status));

    cairo_destroy(_cr);
    _cr = nullptr;

    if (finish_surface)
        cairo_surface_finish(_surface);
    status = cairo_surface_status(_surface);
    cairo_surface_destroy(_surface);
    _surface = nullptr;

    if (_layout)
        g_object_unref(_layout);

    _is_valid = FALSE;

    if (_vector_based_target && _stream) {
        /* Flush stream to be sure. */
        (void) fflush(_stream);

        fclose(_stream);
        _stream = nullptr;
    }

    if (status == CAIRO_STATUS_SUCCESS)
        return true;
    else
        return false;
}

void
CairoRenderContext::transform(Geom::Affine const &transform)
{
    g_assert( _is_valid );

    cairo_matrix_t matrix;
    _initCairoMatrix(&matrix, transform);
    cairo_transform(_cr, &matrix);

    // store new CTM
    _state->transform = getTransform();
}

void
CairoRenderContext::setTransform(Geom::Affine const &transform)
{
    g_assert( _is_valid );

    cairo_matrix_t matrix;
    _initCairoMatrix(&matrix, transform);
    cairo_set_matrix(_cr, &matrix);
    _state->transform = transform;
}

Geom::Affine CairoRenderContext::getTransform() const
{
    g_assert( _is_valid );

    cairo_matrix_t ctm;
    cairo_get_matrix(_cr, &ctm);
    Geom::Affine ret;
    ret[0] = ctm.xx;
    ret[1] = ctm.yx;
    ret[2] = ctm.xy;
    ret[3] = ctm.yy;
    ret[4] = ctm.x0;
    ret[5] = ctm.y0;
    return ret;
}

Geom::Affine CairoRenderContext::getParentTransform() const
{
    g_assert( _is_valid );

    CairoRenderState *parent_state = getParentState();
    return parent_state->transform;
}

void CairoRenderContext::pushState()
{
    g_assert( _is_valid );

    cairo_save(_cr);

    CairoRenderState *new_state = _createState();
    // copy current state's transform
    new_state->transform = _state->transform;
    _state_stack.push_back(new_state);
    _state = new_state;
}

void CairoRenderContext::popState()
{
    g_assert( _is_valid );

    cairo_restore(_cr);

    g_free(_state_stack.back());
    _state_stack.pop_back();

    g_assert( !_state_stack.empty());
    _state = _state_stack.back();
}

static bool pattern_hasItemChildren(SPPattern *pat)
{
    for (auto& child: pat->children) {
        if (is<SPItem>(&child)) {
            return true;
        }
    }
    return false;
}

cairo_pattern_t*
CairoRenderContext::_createPatternPainter(SPPaintServer const *const paintserver, Geom::OptRect const &pbox)
{
    g_assert( is<SPPattern>(paintserver) );

    SPPattern *pat = const_cast<SPPattern*>(cast<SPPattern>(paintserver));

    Geom::Affine ps2user, pcs2dev;
    ps2user = Geom::identity();
    pcs2dev = Geom::identity();

    double x = pat->x();
    double y = pat->y();
    double width = pat->width();
    double height = pat->height();
    double bbox_width_scaler;
    double bbox_height_scaler;

    TRACE(("%f x %f pattern\n", width, height));

    if (pbox && pat->patternUnits() == SPPattern::UNITS_OBJECTBOUNDINGBOX) {
        bbox_width_scaler = pbox->width();
        bbox_height_scaler = pbox->height();
        ps2user[4] = x * bbox_width_scaler + pbox->left();
        ps2user[5] = y * bbox_height_scaler + pbox->top();
    } else {
        bbox_width_scaler = 1.0;
        bbox_height_scaler = 1.0;
        ps2user[4] = x;
        ps2user[5] = y;
    }

    // apply pattern transformation
    Geom::Affine pattern_transform(pat->getTransform());
    ps2user *= pattern_transform;
    Geom::Point ori (ps2user[4], ps2user[5]);

    // create pattern contents coordinate system
    if (pat->viewBox_set) {
        Geom::Rect view_box = *pat->viewbox();

        double x, y, w, h;
        x = 0;
        y = 0;
        w = width * bbox_width_scaler;
        h = height * bbox_height_scaler;

        //calculatePreserveAspectRatio(pat->aspect_align, pat->aspect_clip, view_width, view_height, &x, &y, &w, &h);
        pcs2dev[0] = w / view_box.width();
        pcs2dev[3] = h / view_box.height();
        pcs2dev[4] = x - view_box.left() * pcs2dev[0];
        pcs2dev[5] = y - view_box.top() * pcs2dev[3];
    } else if (pbox && pat->patternContentUnits() == SPPattern::UNITS_OBJECTBOUNDINGBOX) {
        pcs2dev[0] = pbox->width();
        pcs2dev[3] = pbox->height();
    }

    // Calculate the size of the surface which has to be created
#define SUBPIX_SCALE 100
    // Cairo requires an integer pattern surface width/height.
    // Subtract 0.5 to prevent small rounding errors from increasing pattern size by one pixel.
    // Multiply by SUBPIX_SCALE to allow for less than a pixel precision
    double surface_width = MAX(ceil(SUBPIX_SCALE * bbox_width_scaler * width - 0.5), 1);
    double surface_height = MAX(ceil(SUBPIX_SCALE * bbox_height_scaler * height - 0.5), 1);
    TRACE(("pattern surface size: %f x %f\n", surface_width, surface_height));
    // create new rendering context
    CairoRenderContext *pattern_ctx = cloneMe(surface_width, surface_height);

    // adjust the size of the painted pattern to fit exactly the created surface
    // this has to be done because of the rounding to obtain an integer pattern surface width/height
    double scale_width = surface_width / (bbox_width_scaler * width);
    double scale_height = surface_height / (bbox_height_scaler * height);
    if (scale_width != 1.0 || scale_height != 1.0 || _vector_based_target) {
        TRACE(("needed to scale with %f %f\n", scale_width, scale_height));
        pcs2dev *= Geom::Scale(SUBPIX_SCALE,SUBPIX_SCALE);
        ps2user *= Geom::Scale(1.0/SUBPIX_SCALE,1.0/SUBPIX_SCALE);
    }

    // despite scaling up/down by subpixel scaler, the origin point of the pattern must be the same
    ps2user[4] = ori[Geom::X];
    ps2user[5] = ori[Geom::Y];

    pattern_ctx->setTransform(pcs2dev);
    pattern_ctx->pushState();

    // create drawing and group
    Inkscape::Drawing drawing;
    unsigned dkey = SPItem::display_key_new(1);

    // show items and render them
    for (SPPattern *pat_i = pat; pat_i != nullptr; pat_i = pat_i->ref.getObject()) {
        if (pat_i && pattern_hasItemChildren(pat_i)) { // find the first one with item children
            for (auto& child: pat_i->children) {
                if (is<SPItem>(&child)) {
                    cast<SPItem>(&child)->invoke_show(drawing, dkey, SP_ITEM_REFERENCE_FLAGS);
                    _renderer->renderItem(pattern_ctx, cast<SPItem>(&child));
                }
            }
            break; // do not go further up the chain if children are found
        }
    }

    pattern_ctx->popState();

    // setup a cairo_pattern_t
    cairo_surface_t *pattern_surface = pattern_ctx->getSurface();
    TEST(pattern_ctx->saveAsPng("pattern.png"));
    cairo_pattern_t *result = cairo_pattern_create_for_surface(pattern_surface);
    cairo_pattern_set_extend(result, CAIRO_EXTEND_REPEAT);

    // set pattern transformation
    cairo_matrix_t pattern_matrix;
    _initCairoMatrix(&pattern_matrix, ps2user);
    cairo_matrix_invert(&pattern_matrix);
    cairo_pattern_set_matrix(result, &pattern_matrix);

    delete pattern_ctx;

    // hide all items
    for (SPPattern *pat_i = pat; pat_i != nullptr; pat_i = pat_i->ref.getObject()) {
        if (pat_i && pattern_hasItemChildren(pat_i)) { // find the first one with item children
            for (auto& child: pat_i->children) {
                if (is<SPItem>(&child)) {
                    cast<SPItem>(&child)->invoke_hide(dkey);
                }
            }
            break; // do not go further up the chain if children are found
        }
    }

    return result;
}

cairo_pattern_t*
CairoRenderContext::_createHatchPainter(SPPaintServer const *const paintserver, Geom::OptRect const &pbox) {
    SPHatch const *hatch = cast<SPHatch>(paintserver);
    g_assert( hatch );

    g_assert(hatch->pitch() > 0);

    // create drawing and group
    Inkscape::Drawing drawing;
    unsigned dkey = SPItem::display_key_new(1);

    // TODO need to refactor 'evil' referenced code for const correctness.
    SPHatch *evil = const_cast<SPHatch *>(hatch);
    evil->show(drawing, dkey, pbox);

    SPHatch::RenderInfo render_info = hatch->calculateRenderInfo(dkey);
    Geom::Rect tile_rect = render_info.tile_rect;

    // Cairo requires an integer pattern surface width/height.
    // Subtract 0.5 to prevent small rounding errors from increasing pattern size by one pixel.
    // Multiply by SUBPIX_SCALE to allow for less than a pixel precision
    const int subpix_scale = 10;
    double surface_width = MAX(ceil(subpix_scale * tile_rect.width() - 0.5), 1);
    double surface_height = MAX(ceil(subpix_scale * tile_rect.height() - 0.5), 1);
    Geom::Affine drawing_scale = Geom::Scale(surface_width / tile_rect.width(), surface_height / tile_rect.height());
    Geom::Affine drawing_transform = Geom::Translate(-tile_rect.min()) * drawing_scale;

    Geom::Affine child_transform = render_info.child_transform;
    child_transform *= drawing_transform;

    //The rendering of hatch overflow is implemented by repeated drawing
    //of hatch paths over one strip. Within each iteration paths are moved by pitch value.
    //The movement progresses from right to left. This gives the same result
    //as drawing whole strips in left-to-right order.
    gdouble overflow_right_strip = 0.0;
    int overflow_steps = 1;
    Geom::Affine overflow_transform;
    if (hatch->style->overflow.computed == SP_CSS_OVERFLOW_VISIBLE) {
        Geom::Interval bounds = hatch->bounds();
        overflow_right_strip = floor(bounds.max() / hatch->pitch()) * hatch->pitch();
        overflow_steps = ceil((overflow_right_strip - bounds.min()) / hatch->pitch()) + 1;
        overflow_transform = Geom::Translate(hatch->pitch(), 0.0);
    }

    CairoRenderContext *pattern_ctx = cloneMe(surface_width, surface_height);
    pattern_ctx->setTransform(child_transform);
    pattern_ctx->transform(Geom::Translate(-overflow_right_strip, 0.0));
    pattern_ctx->pushState();

    std::vector<SPHatchPath *> children(evil->hatchPaths());

    for (int i = 0; i < overflow_steps; i++) {
        for (auto path : children) {
            _renderer->renderHatchPath(pattern_ctx, *path, dkey);
        }
        pattern_ctx->transform(overflow_transform);
    }

    pattern_ctx->popState();

    // setup a cairo_pattern_t
    cairo_surface_t *pattern_surface = pattern_ctx->getSurface();
    TEST(pattern_ctx->saveAsPng("hatch.png"));
    cairo_pattern_t *result = cairo_pattern_create_for_surface(pattern_surface);
    cairo_pattern_set_extend(result, CAIRO_EXTEND_REPEAT);

    Geom::Affine pattern_transform;
    pattern_transform = render_info.pattern_to_user_transform.inverse() * drawing_transform;
    ink_cairo_pattern_set_matrix(result, pattern_transform);

    evil->hide(dkey);

    delete pattern_ctx;
    return result;
}

cairo_pattern_t*
CairoRenderContext::_createPatternForPaintServer(SPPaintServer const *const paintserver,
                                                 Geom::OptRect const &pbox, float alpha)
{
    cairo_pattern_t *pattern = nullptr;
    bool apply_bbox2user = FALSE;

    auto const paintserver_mutable = const_cast<SPPaintServer *>(paintserver);

    if (auto lg = cast<SPLinearGradient>(paintserver_mutable)) {

            lg->ensureVector(); // when exporting from commandline, vector is not built

            Geom::Point p1 (lg->x1.computed, lg->y1.computed);
            Geom::Point p2 (lg->x2.computed, lg->y2.computed);
            if (pbox && lg->getUnits() == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
                // convert to userspace
                Geom::Affine bbox2user(pbox->width(), 0, 0, pbox->height(), pbox->left(), pbox->top());
                p1 *= bbox2user;
                p2 *= bbox2user;
            }

            // create linear gradient pattern
            pattern = cairo_pattern_create_linear(p1[Geom::X], p1[Geom::Y], p2[Geom::X], p2[Geom::Y]);

            // add stops
            for (gint i = 0; unsigned(i) < lg->vector.stops.size(); i++) {
                float rgb[3];
                lg->vector.stops[i].color.get_rgb_floatv(rgb);
                cairo_pattern_add_color_stop_rgba(pattern, lg->vector.stops[i].offset, rgb[0], rgb[1], rgb[2], lg->vector.stops[i].opacity * alpha);
            }
    } else if (auto rg = cast<SPRadialGradient>(paintserver_mutable)) {

        rg->ensureVector(); // when exporting from commandline, vector is not built

        Geom::Point c (rg->cx.computed, rg->cy.computed);
        Geom::Point f (rg->fx.computed, rg->fy.computed);
        double r = rg->r.computed;
        double fr = rg->fr.computed;
        if (pbox && rg->getUnits() == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX)
            apply_bbox2user = true;

        // create radial gradient pattern
        pattern = cairo_pattern_create_radial(f[Geom::X], f[Geom::Y], fr, c[Geom::X], c[Geom::Y], r);

        // add stops
        for (gint i = 0; unsigned(i) < rg->vector.stops.size(); i++) {
            float rgb[3];
            rg->vector.stops[i].color.get_rgb_floatv(rgb);
            cairo_pattern_add_color_stop_rgba(pattern, rg->vector.stops[i].offset, rgb[0], rgb[1], rgb[2], rg->vector.stops[i].opacity * alpha);
        }
    } else if (auto mg = cast<SPMeshGradient>(paintserver_mutable)) {
        pattern = mg->create_drawing_paintserver()->create_pattern(_cr, pbox, 1.0);
    } else if (is<SPPattern>(paintserver)) {
        pattern = _createPatternPainter(paintserver, pbox);
    } else if (is<SPHatch>(paintserver) ) {
        pattern = _createHatchPainter(paintserver, pbox);
    } else {
        return nullptr;
    }

    if (pattern && is<SPGradient>(paintserver)) {
        auto g = cast<SPGradient>(paintserver_mutable);

        // set extend type
        SPGradientSpread spread = g->fetchSpread();
        switch (spread) {
            case SP_GRADIENT_SPREAD_REPEAT: {
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
                break;
            }
            case SP_GRADIENT_SPREAD_REFLECT: {      // not supported by cairo-pdf yet
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REFLECT);
                break;
            }
            case SP_GRADIENT_SPREAD_PAD: {    // not supported by cairo-pdf yet
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
                break;
            }
            default: {
                cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
                break;
            }
        }

        cairo_matrix_t pattern_matrix;
        if (g->gradientTransform_set) {
            // apply gradient transformation
            cairo_matrix_init(&pattern_matrix,
                g->gradientTransform[0], g->gradientTransform[1],
                g->gradientTransform[2], g->gradientTransform[3],
                g->gradientTransform[4], g->gradientTransform[5]);
        } else {
            cairo_matrix_init_identity (&pattern_matrix);
        }

        if (apply_bbox2user) {
            // convert to userspace
            cairo_matrix_t bbox2user;
            cairo_matrix_init (&bbox2user, pbox->width(), 0, 0, pbox->height(), pbox->left(), pbox->top());
            cairo_matrix_multiply (&pattern_matrix, &bbox2user, &pattern_matrix);
        }
        cairo_matrix_invert(&pattern_matrix);   // because Cairo expects a userspace->patternspace matrix
        cairo_pattern_set_matrix(pattern, &pattern_matrix);
    }

    return pattern;
}

void
CairoRenderContext::_setFillStyle(SPStyle const *const style, Geom::OptRect const &pbox)
{
    g_return_if_fail( !style->fill.set
                      || style->fill.isColor()
                      || style->fill.isPaintserver() );

    float alpha = SP_SCALE24_TO_FLOAT(style->fill_opacity.value);
    if (_state->merge_opacity) {
        alpha *= _state->opacity;
        TRACE(("merged op=%f\n", alpha));
    }

    SPPaintServer const *paint_server = style->getFillPaintServer();
    if (paint_server && paint_server->isValid()) {

        g_assert(is<SPGradient>(SP_STYLE_FILL_SERVER(style))
                 || is<SPPattern>(SP_STYLE_FILL_SERVER(style))
                 || cast<SPHatch>(SP_STYLE_FILL_SERVER(style)));

        cairo_pattern_t *pattern = _createPatternForPaintServer(paint_server, pbox, alpha);
        if (pattern) {
            cairo_set_source(_cr, pattern);
            cairo_pattern_destroy(pattern);
        }
    } else if (style->fill.colorSet) {
        float rgb[3];
        style->fill.value.color.get_rgb_floatv(rgb);

        cairo_set_source_rgba(_cr, rgb[0], rgb[1], rgb[2], alpha);

    } else { // unset fill is black
        g_assert(!style->fill.set
                || (paint_server && !paint_server->isValid()));

        cairo_set_source_rgba(_cr, 0, 0, 0, alpha);
    }
}

void
CairoRenderContext::_setStrokeStyle(SPStyle const *style, Geom::OptRect const &pbox)
{
    float alpha = SP_SCALE24_TO_FLOAT(style->stroke_opacity.value);
    if (_state->merge_opacity)
        alpha *= _state->opacity;

    if (style->stroke.isColor() || (style->stroke.isPaintserver() && !style->getStrokePaintServer()->isValid())) {
        float rgb[3];
        style->stroke.value.color.get_rgb_floatv(rgb);

        cairo_set_source_rgba(_cr, rgb[0], rgb[1], rgb[2], alpha);
    } else {
        g_assert( style->stroke.isPaintserver()
                  || is<SPGradient>(SP_STYLE_STROKE_SERVER(style))
                  || is<SPPattern>(SP_STYLE_STROKE_SERVER(style))
                  || cast<SPHatch>(SP_STYLE_STROKE_SERVER(style)));

        cairo_pattern_t *pattern = _createPatternForPaintServer(SP_STYLE_STROKE_SERVER(style), pbox, alpha);

        if (pattern) {
            cairo_set_source(_cr, pattern);
            cairo_pattern_destroy(pattern);
        }
    }

    if (!style->stroke_dasharray.values.empty() && style->stroke_dasharray.is_valid())
    {
        size_t ndashes = style->stroke_dasharray.values.size();
        double* dashes =(double*)malloc(ndashes*sizeof(double));
        for( unsigned i = 0; i < ndashes; ++i ) {
            dashes[i] = style->stroke_dasharray.values[i].value;
        }
        cairo_set_dash(_cr, dashes, ndashes, style->stroke_dashoffset.value);
        free(dashes);
    } else {
        cairo_set_dash(_cr, nullptr, 0, 0.0);  // disable dashing
    }

    // This allows hairlines to be drawn properly in PDF, PS, Win32-Print, etc.
    // It requires the following pull request in Cairo:
    // https://gitlab.freedesktop.org/cairo/cairo/merge_requests/21
    if (style->stroke_extensions.hairline) {
        ink_cairo_set_hairline(_cr);
    } else {
        cairo_set_line_width(_cr, style->stroke_width.computed);
    }

    // set line join type
    cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
    switch (style->stroke_linejoin.computed) {
        case SP_STROKE_LINEJOIN_MITER:
            join = CAIRO_LINE_JOIN_MITER;
            break;
        case SP_STROKE_LINEJOIN_ROUND:
            join = CAIRO_LINE_JOIN_ROUND;
            break;
        case SP_STROKE_LINEJOIN_BEVEL:
            join = CAIRO_LINE_JOIN_BEVEL;
            break;
    }
    cairo_set_line_join(_cr, join);

    // set line cap type
    cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
    switch (style->stroke_linecap.computed) {
        case SP_STROKE_LINECAP_BUTT:
            cap = CAIRO_LINE_CAP_BUTT;
            break;
        case SP_STROKE_LINECAP_ROUND:
            cap = CAIRO_LINE_CAP_ROUND;
            break;
        case SP_STROKE_LINECAP_SQUARE:
            cap = CAIRO_LINE_CAP_SQUARE;
            break;
    }
    cairo_set_line_cap(_cr, cap);
    cairo_set_miter_limit(_cr, MAX(1, style->stroke_miterlimit.value));
}

void
CairoRenderContext::_prepareRenderGraphic()
{
    // Only PDFLaTeX supports importing a single page of a graphics file,
    // so only PDF backend gets interleaved text/graphics
    if (_is_omittext && _target == CAIRO_SURFACE_TYPE_PDF && _render_mode != RENDER_MODE_CLIP) {
        if (_omittext_state == NEW_PAGE_ON_GRAPHIC) {
            // better set this immediately (not sure if masks applied during "popLayer" could call
            // this function, too, triggering the same code again in error
            _omittext_state = GRAPHIC_ON_TOP;

            // As we can not emit the page in the middle of a layer (aka group) - it will not be fully painted yet! -
            // the following basically mirrors the calls in CairoRenderer::renderItem (but in reversed order)
            // - first traverse all saved states in reversed order (i.e. from deepest nesting to the top)
            //   and apply clipping / masking to layers on the way (this is done in popLayer)
            // - then emit the page using cairo_show_page()
            // - finally restore the previous state with proper transforms and appropriate layers again
            //
            // TODO: While this appears to be an ugly hack it seems to work
            //       Somebody with a more intimate understanding of cairo and the renderer implementation might
            //       be able to implement this in a cleaner way, though.
            int stack_size = _state_stack.size();
            for (int i = stack_size-1; i > 0; i--) {
                if (_state_stack[i]->need_layer)
                    popLayer();
                cairo_restore(_cr);
                _state = _state_stack[i-1];
            }

            cairo_show_page(_cr);

            for (int i = 1; i < stack_size; i++) {
                cairo_save(_cr);
                _state = _state_stack[i];
                if (_state->need_layer)
                    pushLayer();
                setTransform(_state->transform);
            }
        }
        _omittext_state = GRAPHIC_ON_TOP;
    }
}

void
CairoRenderContext::_prepareRenderText()
{
    // Only PDFLaTeX supports importing a single page of a graphics file,
    // so only PDF backend gets interleaved text/graphics
    if (_is_omittext && _target == CAIRO_SURFACE_TYPE_PDF) {
        if (_omittext_state == GRAPHIC_ON_TOP)
            _omittext_state = NEW_PAGE_ON_GRAPHIC;
    }
}

/*  We need CairoPaintOrder as markers are rendered in a separate step and may be rendered
 *  in between fill and stroke.
 */
bool
CairoRenderContext::renderPathVector(Geom::PathVector const & pathv, SPStyle const *style, Geom::OptRect const &pbox, CairoPaintOrder order)
{
    g_assert( _is_valid );

    _prepareRenderGraphic();

    if (_render_mode == RENDER_MODE_CLIP) {
        if (_clip_mode == CLIP_MODE_PATH) {
            addClipPath(pathv, &style->fill_rule);
        } else {
            setPathVector(pathv);
            if (style->fill_rule.computed == SP_WIND_RULE_EVENODD) {
                cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_EVEN_ODD);
            } else {
                cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_WINDING);
            }
            if (style->mix_blend_mode.set && style->mix_blend_mode.value) {
                cairo_set_operator(_cr, ink_css_blend_to_cairo_operator(style->mix_blend_mode.value));
            }
            cairo_fill(_cr);
            TEST(cairo_surface_write_to_png (_surface, "pathmask.png"));
        }
        return true;
    }

    bool no_fill = style->fill.isNone() || style->fill_opacity.value == 0 ||
        order == STROKE_ONLY;
    bool no_stroke = style->stroke.isNone() || (!style->stroke_extensions.hairline && style->stroke_width.computed < 1e-9) ||
                     style->stroke_opacity.value == 0 || order == FILL_ONLY;

    if (no_fill && no_stroke)
        return true;

    bool need_layer = ( !_state->merge_opacity && !_state->need_layer &&
                        ( _state->opacity != 1.0 || _state->clip_path != nullptr || _state->mask != nullptr ) );
    bool blend = false;
    if (style->mix_blend_mode.set && style->mix_blend_mode.value != SP_CSS_BLEND_NORMAL) {
        need_layer = true;
        blend = true;
    }
    if (!need_layer)
        cairo_save(_cr);
    else
        pushLayer();

    if (!no_fill) {
        if (style->fill_rule.computed == SP_WIND_RULE_EVENODD) {
            cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_EVEN_ODD);
        } else {
            cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_WINDING);
        }
    }

    setPathVector(pathv);

    if (!no_fill && (order == STROKE_OVER_FILL || order == FILL_ONLY)) {
        _setFillStyle(style, pbox);

        if (no_stroke)
            cairo_fill(_cr);
        else
            cairo_fill_preserve(_cr);
    }

    if (!no_stroke) {
        _setStrokeStyle(style, pbox);

        if (no_fill || order == STROKE_OVER_FILL)
            cairo_stroke(_cr);
        else
            cairo_stroke_preserve(_cr);
    }

    if (!no_fill && order == FILL_OVER_STROKE) {
        _setFillStyle(style, pbox);

            cairo_fill(_cr);
    }

    if (need_layer) {
        if (blend) {
            popLayer(ink_css_blend_to_cairo_operator(style->mix_blend_mode.value));
        } else {
            popLayer();
        }
    } else {
        cairo_restore(_cr);
    }

    return true;
}

bool CairoRenderContext::renderImage(Inkscape::Pixbuf const *pb,
                                     Geom::Affine const &image_transform, SPStyle const *style)
{
    g_assert( _is_valid );

    if (_render_mode == RENDER_MODE_CLIP) {
        return true;
    }

    _prepareRenderGraphic();

    int w = pb->width();
    int h = pb->height();

    // TODO: reenable merge_opacity if useful

    cairo_surface_t const *image_surface = pb->getSurfaceRaw();
    if (cairo_surface_status(const_cast<cairo_surface_t*>(image_surface))) { // cairo_surface_status does not modify argument
        TRACE(("Image surface creation failed:\n%s\n", cairo_status_to_string(cairo_surface_status(image_surface))));
        return false;
    }

    cairo_save(_cr);

    // scaling by width & height is not needed because it will be done by Cairo
    transform(image_transform);

    // cairo_set_source_surface only modifies refcount of 'image_surface', which is an implementation detail
    cairo_set_source_surface(_cr, const_cast<cairo_surface_t*>(image_surface), 0.0, 0.0);

    // set clip region so that the pattern will not be repeated (bug in Cairo-PDF)
    if (_vector_based_target) {
        cairo_new_path(_cr);
        cairo_rectangle(_cr, 0, 0, w, h);
        cairo_clip(_cr);
    }

    // Cairo filter method will be mapped to PS/PDF 'interpolate' true/false).
    // See cairo-pdf-surface.c
    if (style) {
        // See: http://www.w3.org/TR/SVG/painting.html#ImageRenderingProperty
        //      https://drafts.csswg.org/css-images-3/#the-image-rendering
        //      style.h/style.cpp, drawing-image.cpp
        //
        // CSS 3 defines:
        //   'optimizeSpeed' as alias for "pixelated"
        //   'optimizeQuality' as alias for "smooth"
        switch (style->image_rendering.computed) {
            case SP_CSS_IMAGE_RENDERING_OPTIMIZESPEED:
            case SP_CSS_IMAGE_RENDERING_PIXELATED:
            // we don't have an implementation for crisp-edges, but it should *not* smooth or blur
            case SP_CSS_IMAGE_RENDERING_CRISPEDGES:
                cairo_pattern_set_filter(cairo_get_source(_cr), CAIRO_FILTER_NEAREST);
                break;
            case SP_CSS_IMAGE_RENDERING_OPTIMIZEQUALITY:
            case SP_CSS_IMAGE_RENDERING_AUTO:
            default:
                cairo_pattern_set_filter(cairo_get_source(_cr), CAIRO_FILTER_BEST);
                break;
        }
    }

    if (style->mix_blend_mode.set && style->mix_blend_mode.value) {
        cairo_set_operator(_cr, ink_css_blend_to_cairo_operator(style->mix_blend_mode.value));
    }

    cairo_paint(_cr);

    cairo_restore(_cr);
    return true;
}

#define GLYPH_ARRAY_SIZE 64

// TODO investigate why the font is being ignored:
unsigned int CairoRenderContext::_showGlyphs(cairo_t *cr, PangoFont * /*font*/, std::vector<CairoGlyphInfo> const &glyphtext, bool path)
{
    cairo_glyph_t glyph_array[GLYPH_ARRAY_SIZE];
    cairo_glyph_t *glyphs = glyph_array;
    unsigned int num_glyphs = glyphtext.size();
    if (num_glyphs > GLYPH_ARRAY_SIZE) {
        glyphs = (cairo_glyph_t*)g_try_malloc(sizeof(cairo_glyph_t) * num_glyphs);
        if(glyphs == nullptr) {
            g_warning("CairorenderContext::_showGlyphs: can not allocate memory for %d glyphs.", num_glyphs);
            return 0;
        }
    }

    unsigned int num_invalid_glyphs = 0;
    unsigned int i = 0; // is a counter for indexing the glyphs array, only counts the valid glyphs
    for (const auto & it_info : glyphtext) {
        // skip glyphs which are PANGO_GLYPH_EMPTY (0x0FFFFFFF)
        // or have the PANGO_GLYPH_UNKNOWN_FLAG (0x10000000) set
        if (it_info.index == 0x0FFFFFFF || it_info.index & 0x10000000) {
            TRACE(("INVALID GLYPH found\n"));
            g_message("Invalid glyph found, continuing...");
            num_invalid_glyphs++;
            continue;
        }
        glyphs[i].index = it_info.index;
        glyphs[i].x     = it_info.x;
        glyphs[i].y     = it_info.y;
        i++;
    }

    if (path) {
        cairo_glyph_path(cr, glyphs, num_glyphs - num_invalid_glyphs);
    } else {
        cairo_show_glyphs(cr, glyphs, num_glyphs - num_invalid_glyphs);
    }

    if (num_glyphs > GLYPH_ARRAY_SIZE) {
        g_free(glyphs);
    }

    return num_glyphs - num_invalid_glyphs;
}

/**
 * Called by Layout-TNG-Output, this function decides how to apply styles and
 * write out the final shapes of a set of glyphs to the target.
 *
 * font - The PangoFont to use in cairo.
 * font_matrix - The specific text transform to apply to these glyphs.
 * glyphtext - A list of glyphs to write or render out.
 * style - The style from the span or text node in context.
 * second_pass - True if this is being called in a second pass.
 *
 * Returns true if a second pass is required for fill over stroke paint order.
 */
bool
CairoRenderContext::renderGlyphtext(PangoFont *font, Geom::Affine const &font_matrix,
                                    std::vector<CairoGlyphInfo> const &glyphtext, SPStyle const *style,
                                    bool second_pass)
{
    _prepareRenderText();
    if (_is_omittext)
        return false;

    gpointer fonthash = (gpointer)font;
    cairo_font_face_t *font_face = nullptr;
    if(font_table.find(fonthash)!=font_table.end())
        font_face = font_table[fonthash];

    FcPattern *fc_pattern = nullptr;

# ifdef CAIRO_HAS_FT_FONT
    PangoFcFont *fc_font = PANGO_FC_FONT(font);
    fc_pattern = fc_font->font_pattern;
    if(font_face == nullptr) {
        font_face = cairo_ft_font_face_create_for_pattern(fc_pattern);
        font_table[fonthash] = font_face;
    }
# endif

    cairo_save(_cr);
    cairo_set_font_face(_cr, font_face);

    // set the given font matrix
    cairo_matrix_t matrix;
    _initCairoMatrix(&matrix, font_matrix);
    cairo_set_font_matrix(_cr, &matrix);

    if (_render_mode == RENDER_MODE_CLIP) {
        if (_clip_mode == CLIP_MODE_MASK) {
            if (style->fill_rule.computed == SP_WIND_RULE_EVENODD) {
                cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_EVEN_ODD);
            } else {
                cairo_set_fill_rule(_cr, CAIRO_FILL_RULE_WINDING);
            }
            _showGlyphs(_cr, font, glyphtext, FALSE);
        } else {
            // just add the glyph paths to the current context
            _showGlyphs(_cr, font, glyphtext, TRUE);
        }
        cairo_restore(_cr);
        return false;
    }

    if (style->mix_blend_mode.set && style->mix_blend_mode.value) {
        cairo_set_operator(_cr, ink_css_blend_to_cairo_operator(style->mix_blend_mode.value));
    }

    bool fill = style->fill.isColor() || style->fill.isPaintserver();
    bool stroke = style->stroke.isColor() || style->stroke.isPaintserver();
    if (!fill && !stroke) {
        cairo_restore(_cr);
        return false;
    }

    // Text never has markers, and no-fill doesn't matter.
    bool stroke_over_fill = style->paint_order.get_order(SP_CSS_PAINT_ORDER_STROKE)
                          > style->paint_order.get_order(SP_CSS_PAINT_ORDER_FILL)
                          || !fill || !stroke;

    bool fill_pass = fill && stroke_over_fill != second_pass;
    bool stroke_pass = stroke && !second_pass;

    if (fill_pass) {
        _setFillStyle(style, Geom::OptRect());
        _showGlyphs(_cr, font, glyphtext, _is_texttopath);
        if (_is_texttopath)
            cairo_fill_preserve(_cr);
    }

    // Stroke paths are generated for texttopath AND glyph output
    // because PDF text output doesn't support stroke and fill
    if (stroke_pass) {
        // And now we don't have a path to stroke, so make one.
        if (!_is_texttopath || !fill_pass)
            _showGlyphs(_cr, font, glyphtext, true);
        _setStrokeStyle(style, Geom::OptRect());
        cairo_stroke(_cr);
    }

    cairo_restore(_cr);
    return !stroke_over_fill && !second_pass;
}

/* Helper functions */

void
CairoRenderContext::setPathVector(Geom::PathVector const &pv)
{
    cairo_new_path(_cr);
    addPathVector(pv);
}

void
CairoRenderContext::addPathVector(Geom::PathVector const &pv)
{
    feed_pathvector_to_cairo(_cr, pv);
}

void
CairoRenderContext::_concatTransform(cairo_t *cr, double xx, double yx, double xy, double yy, double x0, double y0)
{
    cairo_matrix_t matrix;

    cairo_matrix_init(&matrix, xx, yx, xy, yy, x0, y0);
    cairo_transform(cr, &matrix);
}

void
CairoRenderContext::_initCairoMatrix(cairo_matrix_t *matrix, Geom::Affine const &transform)
{
    matrix->xx = transform[0];
    matrix->yx = transform[1];
    matrix->xy = transform[2];
    matrix->yy = transform[3];
    matrix->x0 = transform[4];
    matrix->y0 = transform[5];
}

void
CairoRenderContext::_concatTransform(cairo_t *cr, Geom::Affine const &transform)
{
    _concatTransform(cr, transform[0], transform[1],
                         transform[2], transform[3],
                         transform[4], transform[5]);
}

static cairo_status_t
_write_callback(void *closure, const unsigned char *data, unsigned int length)
{
    size_t written;
    FILE *file = (FILE*)closure;

    written = fwrite (data, 1, length, file);

    if (written == length)
    return CAIRO_STATUS_SUCCESS;
    else
    return CAIRO_STATUS_WRITE_ERROR;
}

#include "clear-n_.h"

}  /* namespace Internal */
}  /* namespace Extension */
}  /* namespace Inkscape */

#undef TRACE
#undef TEST


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
