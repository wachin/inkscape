// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_EXTENSION_INTERNAL_PDFINPUT_H
#define SEEN_EXTENSION_INTERNAL_PDFINPUT_H

/*
 * Authors:
 *   miklos erdelyi
 *
 * Copyright (C) 2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#ifdef HAVE_POPPLER
#include <gtkmm.h>
#include <gtkmm/dialog.h>

#include "../../implementation/implementation.h"
#include "poppler-transition-api.h"
#include "poppler-utils.h"
#include "svg-builder.h"

#ifdef HAVE_POPPLER_CAIRO
struct _PopplerDocument;
typedef struct _PopplerDocument            PopplerDocument;
#endif

struct _GdkEventExpose;
typedef _GdkEventExpose GdkEventExpose;

class Page;
class PDFDoc;

namespace Gtk {
  class Button;
  class CheckButton;
  class ComboBoxText;
  class DrawingArea;
  class Frame;
  class Scale;
  class RadioButton;
  class Box;
  class Label;
  class Entry;
}

namespace Inkscape {

namespace UI {
namespace Widget {
  class SpinButton;
  class Frame;
}
}

enum class PdfImportType : unsigned char
{
    PDF_IMPORT_INTERNAL,
    PDF_IMPORT_CAIRO,
};

namespace Extension {
namespace Internal {

class FontModelColumns;

/**
 * PDF import using libpoppler.
 */
class PdfImportDialog : public Gtk::Dialog
{
public:
    PdfImportDialog(std::shared_ptr<PDFDoc> doc, const gchar *uri);
    ~PdfImportDialog() override;

    bool showDialog();
    std::string getSelectedPages();
    PdfImportType getImportMethod();
    void getImportSettings(Inkscape::XML::Node *prefs);
    FontStrategies getFontStrategies();
    void setFontStrategies(const FontStrategies &fs);

private:
    void _fontRenderChanged();
    void _setPreviewPage(int page);
    void _setFonts(const FontList &fonts);

    // Signal handlers
    bool _onDraw(const Cairo::RefPtr<Cairo::Context>& cr);
    void _onPageNumberChanged();
    void _onPrecisionChanged();

    Glib::RefPtr<Gtk::Builder> _builder;

    Gtk::Entry &_page_numbers;
    Gtk::DrawingArea &_preview_area;
    Gtk::CheckButton &_embed_images;
    Gtk::Scale &_mesh_slider;
    Gtk::Label &_mesh_label;
    Gtk::Button &_next_page;
    Gtk::Button &_prev_page;
    Gtk::Label &_current_page;
    Glib::RefPtr<Gtk::ListStore> _font_model;
    FontModelColumns *_font_col;

    std::shared_ptr<PDFDoc> _pdf_doc;   // Document to be imported
    std::string _current_pages;  // Current selected pages
    FontList _font_list;         // List of fonts and the pages they appear on
    int _total_pages = 0;
    int _preview_page = 1;
    Page *_previewed_page;    // Currently previewed page
    unsigned char *_thumb_data; // Thumbnail image data
    int _thumb_width, _thumb_height;    // Thumbnail size
    int _thumb_rowstride;
    int _preview_width, _preview_height;    // Size of the preview area
    bool _render_thumb;     // Whether we can/shall render thumbnails
#ifdef HAVE_POPPLER_CAIRO
    cairo_surface_t *_cairo_surface = nullptr;
    PopplerDocument *_poppler_doc = nullptr;
#endif
};

    
class PdfInput: public Inkscape::Extension::Implementation::Implementation {
    PdfInput () = default;;
public:
    SPDocument *open( Inkscape::Extension::Input *mod,
                                const gchar *uri ) override;
    static void         init( );
private:
    void add_builder_page(
        std::shared_ptr<PDFDoc> pdf_doc,
        SvgBuilder *builder, SPDocument *doc,
        int page_num);
};

} // namespace Implementation
} // namespace Extension
} // namespace Inkscape

#endif // HAVE_POPPLER

#endif // SEEN_EXTENSION_INTERNAL_PDFINPUT_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
