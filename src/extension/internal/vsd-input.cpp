// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *  This file came from libwpg as a source, their utility wpg2svg
 *  specifically.  It has been modified to work as an Inkscape extension.
 *  The Inkscape extension code is covered by this copyright, but the
 *  rest is covered by the one below.
 *
 * Authors:
 *   Fridrich Strba (fridrich.strba@bluewin.ch)
 *
 * Copyright (C) 2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <cstdio>

#include "vsd-input.h"

#ifdef WITH_LIBVISIO

#include <string>
#include <cstring>

#include <libvisio/libvisio.h>

#include <librevenge-stream/librevenge-stream.h>

using librevenge::RVNGString;
using librevenge::RVNGFileStream;
using librevenge::RVNGStringVector;

#include <gtkmm/spinbutton.h>

#include "extension/system.h"
#include "extension/input.h"

#include "document.h"
#include "inkscape.h"

#include "ui/dialog-events.h"
#include <glibmm/i18n.h>

#include "ui/view/svg-view-widget.h"

#include "object/sp-root.h"

#include "util/units.h"

namespace Inkscape {
namespace Extension {
namespace Internal {


class VsdImportDialog : public Gtk::Dialog {
public:
     VsdImportDialog(const std::vector<RVNGString> &vec);
     ~VsdImportDialog() override;

     bool showDialog();
     unsigned getSelectedPage();
     void getImportSettings(Inkscape::XML::Node *prefs);

private:
     void _setPreviewPage();

     // Signal handlers
     void _onPageNumberChanged();
     void _onSpinButtonPress(GdkEventButton* button_event);
     void _onSpinButtonRelease(GdkEventButton* button_event);

     class Gtk::Box * vbox1;
     class Inkscape::UI::View::SVGViewWidget * _previewArea;
     class Gtk::Button * cancelbutton;
     class Gtk::Button * okbutton;

     class Gtk::Box  * _page_selector_box;
     class Gtk::Label * _labelSelect;
     class Gtk::Label * _labelTotalPages;
     class Gtk::SpinButton * _pageNumberSpin;

     const std::vector<RVNGString> &_vec;  // Document to be imported
     unsigned _current_page;               // Current selected page
     bool _spinning;                       // whether SpinButton is pressed (i.e. we're "spinning")
};

VsdImportDialog::VsdImportDialog(const std::vector<RVNGString> &vec)
    : _previewArea(nullptr)
    , _vec(vec)
    , _current_page(1)
    , _spinning(false)
{
     int num_pages = _vec.size();
     if ( num_pages <= 1 )
          return;


     // Dialog settings
     this->set_title(_("Page Selector"));
     this->set_modal(true);
     sp_transientize(GTK_WIDGET(this->gobj()));  //Make transient
     this->property_window_position().set_value(Gtk::WIN_POS_NONE);
     this->set_resizable(true);
     this->property_destroy_with_parent().set_value(false);

     // Preview area
     vbox1 = Gtk::manage(new class Gtk::Box(Gtk::ORIENTATION_VERTICAL));
     this->get_content_area()->pack_start(*vbox1);

     // CONTROLS
     _page_selector_box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));

     // Labels
     _labelSelect = Gtk::manage(new class Gtk::Label(_("Select page:")));
     _labelTotalPages = Gtk::manage(new class Gtk::Label());
     _labelSelect->set_line_wrap(false);
     _labelSelect->set_use_markup(false);
     _labelSelect->set_selectable(false);
     _page_selector_box->pack_start(*_labelSelect, Gtk::PACK_SHRINK);

     // Adjustment + spinner
     auto _pageNumberSpin_adj = Gtk::Adjustment::create(1, 1, _vec.size(), 1, 10, 0);
     _pageNumberSpin = Gtk::manage(new Gtk::SpinButton(_pageNumberSpin_adj, 1, 0));
     _pageNumberSpin->set_can_focus();
     _pageNumberSpin->set_update_policy(Gtk::UPDATE_ALWAYS);
     _pageNumberSpin->set_numeric(true);
     _pageNumberSpin->set_wrap(false);
     _page_selector_box->pack_start(*_pageNumberSpin, Gtk::PACK_SHRINK);

     _labelTotalPages->set_line_wrap(false);
     _labelTotalPages->set_use_markup(false);
     _labelTotalPages->set_selectable(false);
     gchar *label_text = g_strdup_printf(_("out of %i"), num_pages);
     _labelTotalPages->set_label(label_text);
     g_free(label_text);
     _page_selector_box->pack_start(*_labelTotalPages, Gtk::PACK_SHRINK);

     vbox1->pack_end(*_page_selector_box, Gtk::PACK_SHRINK);

     // Buttons
     cancelbutton = Gtk::manage(new Gtk::Button(_("_Cancel"), true));
     okbutton     = Gtk::manage(new Gtk::Button(_("_OK"),     true));
     this->add_action_widget(*cancelbutton, Gtk::RESPONSE_CANCEL);
     this->add_action_widget(*okbutton, Gtk::RESPONSE_OK);

     // Show all widgets in dialog
     this->show_all();

     // Connect signals
     _pageNumberSpin->signal_value_changed().connect(sigc::mem_fun(*this, &VsdImportDialog::_onPageNumberChanged));
     _pageNumberSpin->signal_button_press_event().connect_notify(sigc::mem_fun(*this, &VsdImportDialog::_onSpinButtonPress));
     _pageNumberSpin->signal_button_release_event().connect_notify(sigc::mem_fun(*this, &VsdImportDialog::_onSpinButtonRelease));

     _setPreviewPage();
}

VsdImportDialog::~VsdImportDialog() = default;

bool VsdImportDialog::showDialog()
{
     show();
     gint b = run();
     hide();
     if (b == Gtk::RESPONSE_OK || b == Gtk::RESPONSE_ACCEPT) {
          return TRUE;
     } else {
          return FALSE;
     }
}

unsigned VsdImportDialog::getSelectedPage()
{
     return _current_page;
}

void VsdImportDialog::_onPageNumberChanged()
{
     unsigned page = static_cast<unsigned>(_pageNumberSpin->get_value_as_int());
     _current_page = CLAMP(page, 1U, _vec.size());
     _setPreviewPage();
}

void VsdImportDialog::_onSpinButtonPress(GdkEventButton* /*button_event*/)
{
     _spinning = true;
}

void VsdImportDialog::_onSpinButtonRelease(GdkEventButton* /*button_event*/)
{
     _spinning = false;
     _setPreviewPage();
}

/**
 * \brief Renders the given page's thumbnail
 */
void VsdImportDialog::_setPreviewPage()
{
    if (_spinning) {
        return;
    }

    SPDocument *doc = SPDocument::createNewDocFromMem(_vec[_current_page-1].cstr(), strlen(_vec[_current_page-1].cstr()), false);
    if(!doc) {
        g_warning("VSD import: Could not create preview for page %d", _current_page);
        gchar const *no_preview_template = R"A(
          <svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'>
            <path d='M 82,10 18,74 m 0,-64 64,64' style='fill:none;stroke:#ff0000;stroke-width:2px;'/>
            <rect x='18' y='10' width='64' height='64' style='fill:none;stroke:#000000;stroke-width:1.5px;'/>
            <text x='50' y='92' style='font-size:10px;text-anchor:middle;font-family:sans-serif;'>%s</text>
          </svg>
        )A";
        gchar * no_preview = g_strdup_printf(no_preview_template, _("No preview"));
        doc = SPDocument::createNewDocFromMem(no_preview, strlen(no_preview), false);
        g_free(no_preview);
    }

    if (!doc) {
        std::cerr << "VsdImportDialog::_setPreviewPage: No document!" << std::endl;
        return;
    }

    if (_previewArea) {
        _previewArea->setDocument(doc);
    } else {
        _previewArea = Gtk::manage(new Inkscape::UI::View::SVGViewWidget(doc));
        vbox1->pack_start(*_previewArea, Gtk::PACK_EXPAND_WIDGET, 0);
    }

    _previewArea->setResize(400, 400);
    _previewArea->show_all();
}

SPDocument *VsdInput::open(Inkscape::Extension::Input * /*mod*/, const gchar * uri)
{
     #ifdef _WIN32
          // RVNGFileStream uses fopen() internally which unfortunately only uses ANSI encoding on Windows
          // therefore attempt to convert uri to the system codepage
          // even if this is not possible the alternate short (8.3) file name will be used if available
          gchar * converted_uri = g_win32_locale_filename_from_utf8(uri);
          RVNGFileStream input(converted_uri);
          g_free(converted_uri);
     #else
          RVNGFileStream input(uri);
     #endif

     if (!libvisio::VisioDocument::isSupported(&input)) {
          return nullptr;
     }

     RVNGStringVector output;
     librevenge::RVNGSVGDrawingGenerator generator(output, "svg");

     if (!libvisio::VisioDocument::parse(&input, &generator)) {
          return nullptr;
     }

     if (output.empty()) {
          return nullptr;
     }

     std::vector<RVNGString> tmpSVGOutput;
     for (unsigned i=0; i<output.size(); ++i) {
          RVNGString tmpString("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
          tmpString.append(output[i]);
          tmpSVGOutput.push_back(tmpString);
     }

     unsigned page_num = 1;

     // If only one page is present, import that one without bothering user
     if (tmpSVGOutput.size() > 1) {
          VsdImportDialog *dlg = nullptr;
          if (INKSCAPE.use_gui()) {
               dlg = new VsdImportDialog(tmpSVGOutput);
               if (!dlg->showDialog()) {
                    delete dlg;
                    throw Input::open_cancelled();
               }
          }

          // Get needed page
          if (dlg) {
               page_num = dlg->getSelectedPage();
               if (page_num < 1)
                    page_num = 1;
               if (page_num > tmpSVGOutput.size())
                    page_num = tmpSVGOutput.size();
          }
     }

     SPDocument * doc = SPDocument::createNewDocFromMem(tmpSVGOutput[page_num-1].cstr(), strlen(tmpSVGOutput[page_num-1].cstr()), TRUE);

     // Set viewBox if it doesn't exist
     if (doc && !doc->getRoot()->viewBox_set) {
         doc->setViewBox(Geom::Rect::from_xywh(0, 0, doc->getWidth().value(doc->getDisplayUnit()), doc->getHeight().value(doc->getDisplayUnit())));
     }
     return doc;
}

#include "clear-n_.h"

void VsdInput::init()
{
    // clang-format off
    /* VSD */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("VSD Input") "</name>\n"
            "<id>org.inkscape.input.vsd</id>\n"
            "<input>\n"
                "<extension>.vsd</extension>\n"
                "<mimetype>application/vnd.visio</mimetype>\n"
                "<filetypename>" N_("Microsoft Visio Diagram (*.vsd)") "</filetypename>\n"
                "<filetypetooltip>" N_("File format used by Microsoft Visio 6 and later") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new VsdInput());

    /* VDX */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("VDX Input") "</name>\n"
            "<id>org.inkscape.input.vdx</id>\n"
            "<input>\n"
                "<extension>.vdx</extension>\n"
                "<mimetype>application/vnd.visio</mimetype>\n"
                "<filetypename>" N_("Microsoft Visio XML Diagram (*.vdx)") "</filetypename>\n"
                "<filetypetooltip>" N_("File format used by Microsoft Visio 2010 and later") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new VsdInput());

    /* VSDM */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("VSDM Input") "</name>\n"
            "<id>org.inkscape.input.vsdm</id>\n"
            "<input>\n"
                "<extension>.vsdm</extension>\n"
                "<mimetype>application/vnd.visio</mimetype>\n"
                "<filetypename>" N_("Microsoft Visio 2013 drawing (*.vsdm)") "</filetypename>\n"
                "<filetypetooltip>" N_("File format used by Microsoft Visio 2013 and later") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new VsdInput());

    /* VSDX */
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("VSDX Input") "</name>\n"
            "<id>org.inkscape.input.vsdx</id>\n"
            "<input>\n"
                "<extension>.vsdx</extension>\n"
                "<mimetype>application/vnd.visio</mimetype>\n"
                "<filetypename>" N_("Microsoft Visio 2013 drawing (*.vsdx)") "</filetypename>\n"
                "<filetypetooltip>" N_("File format used by Microsoft Visio 2013 and later") "</filetypetooltip>\n"
            "</input>\n"
        "</inkscape-extension>", new VsdInput());
    // clang-format on

     return;

} // init

} } }  /* namespace Inkscape, Extension, Implementation */
#endif /* WITH_LIBVISIO */

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
