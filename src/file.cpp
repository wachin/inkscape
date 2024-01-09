// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * File/Print operations.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Chema Celorio <chema@celorio.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Bruno Dilly <bruno.dilly@gmail.com>
 *   Stephen Silver <sasilver@users.sourceforge.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Tavmjong Bah
 *
 * Copyright (C) 2006 Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) 1999-2016 Authors
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/** @file
 * @note This file needs to be cleaned up extensively.
 * What it probably needs is to have one .h file for
 * the API, and two or more .cpp files for the implementations.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <gtkmm.h>

#include "file.h"
#include "inkscape-application.h"
#include "inkscape-window.h"

#include "desktop.h"
#include "document-undo.h"
#include "event-log.h"
#include "id-clash.h"
#include "inkscape-version.h"
#include "inkscape.h"
#include "layer-manager.h"
#include "message-stack.h"
#include "page-manager.h"
#include "path-prefix.h"
#include "print.h"
#include "rdf.h"

#include "extension/db.h"
#include "extension/effect.h"
#include "extension/input.h"
#include "extension/output.h"

#include "io/file.h"
#include "io/resource.h"
#include "io/fix-broken-links.h"
#include "io/sys.h"

#include "object/sp-defs.h"
#include "object/sp-namedview.h"
#include "object/sp-page.h"
#include "object/sp-root.h"
#include "object/sp-use.h"
#include "page-manager.h"
#include "style.h"

#include "ui/dialog/filedialog.h"
#include "ui/icon-names.h"
#include "ui/interface.h"
#include "ui/tools/tool-base.h"
#include "widgets/desktop-widget.h"

#include "svg/svg.h" // for sp_svg_transform_write, used in sp_import_document
#include "xml/rebase-hrefs.h"
#include "xml/sp-css-attr.h"

using Inkscape::DocumentUndo;
using Inkscape::IO::Resource::TEMPLATES;
using Inkscape::IO::Resource::USER;

#ifdef _WIN32
#include <windows.h>
#endif

//#define INK_DUMP_FILENAME_CONV 1
#undef INK_DUMP_FILENAME_CONV

//#define INK_DUMP_FOPEN 1
#undef INK_DUMP_FOPEN

void dump_str(gchar const *str, gchar const *prefix);
void dump_ustr(Glib::ustring const &ustr);


/*######################
## N E W
######################*/

/**
 * Create a blank document and add it to the desktop
 * Input: empty string or template file name.
 */
SPDesktop *sp_file_new(const std::string &templ)
{
    auto *app = InkscapeApplication::instance();

    SPDocument* doc = app->document_new (templ);
    if (!doc) {
        std::cerr << "sp_file_new: failed to open document: " << templ << std::endl;
    }
    InkscapeWindow* win = app->window_open (doc);

    SPDesktop* desktop = win->get_desktop();

    return desktop;
}

std::string sp_file_default_template_uri()
{
    return Inkscape::IO::Resource::get_filename_string(TEMPLATES, "default.svg", true);
}

SPDesktop* sp_file_new_default()
{
    SPDesktop* desk = sp_file_new(sp_file_default_template_uri());
    //rdf_add_from_preferences( SP_ACTIVE_DOCUMENT );

    return desk;
}


/*######################
## D E L E T E
######################*/

/**
 *  Perform document closures preceding an exit()
 *
 *  Only used by OLD DBus interface.
 */
void sp_file_exit()
{
    if (SP_ACTIVE_DESKTOP == nullptr) {
        // We must be in console mode
        auto app = Gio::Application::get_default();
        g_assert(app);
        app->quit();
    } else {
        auto app = InkscapeApplication::instance();
        g_assert(app);
        app->destroy_all();
    }
}


/**
 *  Handle prompting user for "do you want to revert"?  Revert on "OK"
 */
void sp_file_revert_dialog()
{
    SPDesktop  *desktop = SP_ACTIVE_DESKTOP;
    g_assert(desktop != nullptr);

    SPDocument *doc = desktop->getDocument();
    g_assert(doc != nullptr);

    Inkscape::XML::Node *repr = doc->getReprRoot();
    g_assert(repr != nullptr);

    gchar const *filename = doc->getDocumentFilename();
    if (!filename) {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved yet.  Cannot revert."));
        return;
    }

    bool do_revert = true;
    if (doc->isModifiedSinceSave()) {
        Glib::ustring tmpString = Glib::ustring::compose(_("Changes will be lost! Are you sure you want to reload document %1?"), filename);
        bool response = desktop->warnDialog (tmpString);
        if (!response) {
            do_revert = false;
        }
    }

    bool reverted = false;
    if (do_revert) {
        auto *app = InkscapeApplication::instance();
        reverted = app->document_revert (doc);
    }

    if (reverted) {
        desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Document reverted."));
    } else {
        desktop->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not reverted."));
    }
}

void dump_str(gchar const *str, gchar const *prefix)
{
    Glib::ustring tmp;
    tmp = prefix;
    tmp += " [";
    size_t const total = strlen(str);
    for (unsigned i = 0; i < total; i++) {
        gchar *const tmp2 = g_strdup_printf(" %02x", (0x0ff & str[i]));
        tmp += tmp2;
        g_free(tmp2);
    }

    tmp += "]";
    g_message("%s", tmp.c_str());
}

void dump_ustr(Glib::ustring const &ustr)
{
    char const *cstr = ustr.c_str();
    char const *data = ustr.data();
    Glib::ustring::size_type const byteLen = ustr.bytes();
    Glib::ustring::size_type const dataLen = ustr.length();
    Glib::ustring::size_type const cstrLen = strlen(cstr);

    g_message("   size: %lu\n   length: %lu\n   bytes: %lu\n    clen: %lu",
              gulong(ustr.size()), gulong(dataLen), gulong(byteLen), gulong(cstrLen) );
    g_message( "  ASCII? %s", (ustr.is_ascii() ? "yes":"no") );
    g_message( "  UTF-8? %s", (ustr.validate() ? "yes":"no") );

    try {
        Glib::ustring tmp;
        for (Glib::ustring::size_type i = 0; i < ustr.bytes(); i++) {
            tmp = "    ";
            if (i < dataLen) {
                Glib::ustring::value_type val = ustr.at(i);
                gchar* tmp2 = g_strdup_printf( (((val & 0xff00) == 0) ? "  %02x" : "%04x"), val );
                tmp += tmp2;
                g_free( tmp2 );
            } else {
                tmp += "    ";
            }

            if (i < byteLen) {
                int val = (0x0ff & data[i]);
                gchar *tmp2 = g_strdup_printf("    %02x", val);
                tmp += tmp2;
                g_free( tmp2 );
                if ( val > 32 && val < 127 ) {
                    tmp2 = g_strdup_printf( "   '%c'", (gchar)val );
                    tmp += tmp2;
                    g_free( tmp2 );
                } else {
                    tmp += "    . ";
                }
            } else {
                tmp += "       ";
            }

            if ( i < cstrLen ) {
                int val = (0x0ff & cstr[i]);
                gchar* tmp2 = g_strdup_printf("    %02x", val);
                tmp += tmp2;
                g_free(tmp2);
                if ( val > 32 && val < 127 ) {
                    tmp2 = g_strdup_printf("   '%c'", (gchar) val);
                    tmp += tmp2;
                    g_free( tmp2 );
                } else {
                    tmp += "    . ";
                }
            } else {
                tmp += "            ";
            }

            g_message( "%s", tmp.c_str() );
        }
    } catch (...) {
        g_message("XXXXXXXXXXXXXXXXXX Exception" );
    }
    g_message("---------------");
}

/**
 *  Display an file Open selector.  Open a document if OK is pressed.
 *  Can select single or multiple files for opening.
 */
void
sp_file_open_dialog(Gtk::Window &parentWindow, gpointer /*object*/, gpointer /*data*/)
{
    //# Get the current directory for finding files
    static Glib::ustring open_path;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if(open_path.empty())
    {
        Glib::ustring attr = prefs->getString("/dialogs/open/path");
        if (!attr.empty()) open_path = attr;
    }

    //# Test if the open_path directory exists
    if (!Inkscape::IO::file_test(open_path.c_str(),
              (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
        open_path = "";

#ifdef _WIN32
    //# If no open path, default to our win32 documents folder
    if (open_path.empty())
    {
        // The path to the My Documents folder is read from the
        // value "HKEY_CURRENT_USER\Software\Windows\CurrentVersion\Explorer\Shell Folders\Personal"
        HKEY key = NULL;
        if(RegOpenKeyExA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
            0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            WCHAR utf16path[_MAX_PATH];
            DWORD value_type;
            DWORD data_size = sizeof(utf16path);
            if(RegQueryValueExW(key, L"Personal", NULL, &value_type,
                (BYTE*)utf16path, &data_size) == ERROR_SUCCESS)
            {
                g_assert(value_type == REG_SZ);
                gchar *utf8path = g_utf16_to_utf8(
                    (const gunichar2*)utf16path, -1, NULL, NULL, NULL);
                if(utf8path)
                {
                    open_path = Glib::ustring(utf8path);
                    g_free(utf8path);
                }
            }
        }
    }
#endif

    //# If no open path, default to our home directory
    if (open_path.empty())
    {
        open_path = g_get_home_dir();
        open_path.append(G_DIR_SEPARATOR_S);
    }

    //# Create a dialog
    Inkscape::UI::Dialog::FileOpenDialog *openDialogInstance =
              Inkscape::UI::Dialog::FileOpenDialog::create(
                 parentWindow, open_path,
                 Inkscape::UI::Dialog::SVG_TYPES,
                 _("Select file to open"));

    //# Show the dialog
    bool const success = openDialogInstance->show();

    //# Save the folder the user selected for later
    open_path = openDialogInstance->getCurrentDirectory();

    if (!success)
    {
        delete openDialogInstance;
        return;
    }

    // FIXME: This is silly to have separate code paths for opening one vs many files!

    //# User selected something.  Get name and type
    Glib::ustring fileName = openDialogInstance->getFilename();

    // Inkscape::Extension::Extension *fileType =
    //         openDialogInstance->getExtension();

    //# Code to check & open if multiple files.
    std::vector<Glib::ustring> flist = openDialogInstance->getFilenames();

    //# We no longer need the file dialog object - delete it
    delete openDialogInstance;
    openDialogInstance = nullptr;

    auto *app = InkscapeApplication::instance();

    //# Iterate through filenames if more than 1
    if (flist.size() > 1)
    {
        for (const auto & i : flist)
        {
            fileName = i;

            Glib::ustring newFileName = Glib::filename_to_utf8(fileName);
            if ( newFileName.size() > 0 )
                fileName = newFileName;
            else
                g_warning( "ERROR CONVERTING OPEN FILENAME TO UTF-8" );

#ifdef INK_DUMP_FILENAME_CONV
            g_message("Opening File %s\n", fileName.c_str());
#endif

            Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(fileName);
            app->create_window (file);
        }

        return;
    }


    if (!fileName.empty())
    {
        Glib::ustring newFileName = Glib::filename_to_utf8(fileName);

        if ( newFileName.size() > 0)
            fileName = newFileName;
        else
            g_warning( "ERROR CONVERTING OPEN FILENAME TO UTF-8" );

        open_path = Glib::path_get_dirname (fileName);
        open_path.append(G_DIR_SEPARATOR_S);
        prefs->setString("/dialogs/open/path", open_path);

        Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(fileName);
        app->create_window (file);
    }

    return;
}


/*######################
## V A C U U M
######################*/

/**
 * Remove unreferenced defs from the defs section of the document.
 */
void sp_file_vacuum(SPDocument *doc)
{
    unsigned int diff = doc->vacuumDocument();

    DocumentUndo::done(doc, _("Clean up document"), INKSCAPE_ICON("document-cleanup"));

    SPDesktop *dt = SP_ACTIVE_DESKTOP;
    if (dt != nullptr) {
        // Show status messages when in GUI mode
        if (diff > 0) {
            dt->messageStack()->flashF(Inkscape::NORMAL_MESSAGE,
                    ngettext("Removed <b>%i</b> unused definition in &lt;defs&gt;.",
                            "Removed <b>%i</b> unused definitions in &lt;defs&gt;.",
                            diff),
                    diff);
        } else {
            dt->messageStack()->flash(Inkscape::NORMAL_MESSAGE,  _("No unused definitions in &lt;defs&gt;."));
        }
    }
}



/*######################
## S A V E
######################*/

/**
 * This 'save' function called by the others below
 *
 * \param    official  whether to set :output_module and :modified in the
 *                     document; is true for normal save, false for temporary saves
 */
static bool
file_save(Gtk::Window &parentWindow, SPDocument *doc, const Glib::ustring &uri,
          Inkscape::Extension::Extension *key, bool checkoverwrite, bool official,
          Inkscape::Extension::FileSaveMethod save_method)
{
    if (!doc || uri.size()<1) //Safety check
        return false;

    Inkscape::Version save = doc->getRoot()->version.inkscape;
    doc->getReprRoot()->setAttribute("inkscape:version", Inkscape::version_string);
    try {
        Inkscape::Extension::save(key, doc, uri.c_str(),
                                  checkoverwrite, official,
                                  save_method);
    } catch (Inkscape::Extension::Output::no_extension_found &e) {
        gchar *safeUri = Inkscape::IO::sanitizeString(uri.c_str());
        gchar *text = g_strdup_printf(_("No Inkscape extension found to save document (%s).  This may have been caused by an unknown filename extension."), safeUri);
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        g_free(safeUri);
        // Restore Inkscape version
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (Inkscape::Extension::Output::file_read_only &e) {
        gchar *safeUri = Inkscape::IO::sanitizeString(uri.c_str());
        gchar *text = g_strdup_printf(_("File %s is write protected. Please remove write protection and try again."), safeUri);
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        g_free(safeUri);
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (Inkscape::Extension::Output::save_failed &e) {
        gchar *safeUri = Inkscape::IO::sanitizeString(uri.c_str());
        gchar *text = g_strdup_printf(_("File %s could not be saved."), safeUri);
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        g_free(safeUri);
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (Inkscape::Extension::Output::save_cancelled &e) {
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (Inkscape::Extension::Output::export_id_not_found &e) {
        gchar *text = g_strdup_printf(_("File could not be saved:\nNo object with ID '%s' found."), e.id);
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (Inkscape::Extension::Output::no_overwrite &e) {
        return sp_file_save_dialog(parentWindow, doc, save_method);
    } catch (std::exception &e) {
        gchar *safeUri = Inkscape::IO::sanitizeString(uri.c_str());
        gchar *text = g_strdup_printf(_("File %s could not be saved.\n\n"
                                        "The following additional information was returned by the output extension:\n"
                                        "'%s'"), safeUri, e.what());
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        g_free(safeUri);
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    } catch (...) {
        g_critical("Extension '%s' threw an unspecified exception.", key->get_id());
        gchar *safeUri = Inkscape::IO::sanitizeString(uri.c_str());
        gchar *text = g_strdup_printf(_("File %s could not be saved."), safeUri);
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::ERROR_MESSAGE, _("Document not saved."));
        sp_ui_error_dialog(text);
        g_free(text);
        g_free(safeUri);
        doc->getReprRoot()->setAttribute("inkscape:version", sp_version_to_string( save ));
        return false;
    }

    if (SP_ACTIVE_DESKTOP) {
        if (! SP_ACTIVE_DESKTOP->messageStack()) {
            g_message("file_save: ->messageStack() == NULL. please report to bug #967416");
        }
    } else {
        g_message("file_save: SP_ACTIVE_DESKTOP == NULL. please report to bug #967416");
    }

    doc->get_event_log()->rememberFileSave();
    Glib::ustring msg;
    if (doc->getDocumentFilename() == nullptr) {
        msg = Glib::ustring::format(_("Document saved."));
    } else {
        msg = Glib::ustring::format(_("Document saved."), " ", doc->getDocumentFilename());
    }
    SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::NORMAL_MESSAGE, msg.c_str());
    return true;
}


/**
 *  Display a SaveAs dialog.  Save the document if OK pressed.
 */
bool
sp_file_save_dialog(Gtk::Window &parentWindow, SPDocument *doc, Inkscape::Extension::FileSaveMethod save_method)
{
    Inkscape::Extension::Output *extension = nullptr;
    bool is_copy = (save_method == Inkscape::Extension::FILE_SAVE_METHOD_SAVE_COPY);

    // Note: default_extension has the format "org.inkscape.output.svg.inkscape", whereas
    //       filename_extension only uses ".svg"
    Glib::ustring default_extension;
    Glib::ustring filename_extension = ".svg";

    default_extension= Inkscape::Extension::get_file_save_extension(save_method);
    //g_message("%s: extension name: '%s'", __FUNCTION__, default_extension);

    extension = dynamic_cast<Inkscape::Extension::Output *>
        (Inkscape::Extension::db.get(default_extension.c_str()));

    if (extension)
        filename_extension = extension->get_extension();

    Glib::ustring save_path = Inkscape::Extension::get_file_save_path(doc, save_method);

    if (!Inkscape::IO::file_test(save_path.c_str(),
          (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
        save_path.clear();

    if (save_path.empty())
        save_path = g_get_home_dir();

    Glib::ustring save_loc = save_path;
    save_loc.append(G_DIR_SEPARATOR_S);

    int i = 1;
    if ( !doc->getDocumentFilename() ) {
        // We are saving for the first time; create a unique default filename
        save_loc = save_loc + _("drawing") + filename_extension;

        while (Inkscape::IO::file_test(save_loc.c_str(), G_FILE_TEST_EXISTS)) {
            save_loc = save_path;
            save_loc.append(G_DIR_SEPARATOR_S);
            save_loc = save_loc + Glib::ustring::compose(_("drawing-%1"), i++) + filename_extension;
        }
    } else {
        save_loc.append(Glib::path_get_basename(doc->getDocumentFilename()));
    }

    // convert save_loc from utf-8 to locale
    // is this needed any more, now that everything is handled in
    // Inkscape::IO?
    Glib::ustring save_loc_local = Glib::filename_from_utf8(save_loc);

    if (!save_loc_local.empty())
        save_loc = save_loc_local;

    //# Show the SaveAs dialog
    char const * dialog_title;
    if (is_copy) {
        dialog_title = (char const *) _("Select file to save a copy to");
    } else {
        dialog_title = (char const *) _("Select file to save to");
    }
    gchar* doc_title = doc->getRoot()->title();
    Inkscape::UI::Dialog::FileSaveDialog *saveDialog =
        Inkscape::UI::Dialog::FileSaveDialog::create(
            parentWindow,
            save_loc,
            Inkscape::UI::Dialog::SVG_TYPES,
            dialog_title,
            default_extension,
            doc_title ? doc_title : "",
            save_method
            );

    saveDialog->setExtension(extension);

    bool success = saveDialog->show();
    if (!success) {
        delete saveDialog;
        if(doc_title) g_free(doc_title);
        return success;
    }

    // set new title here (call RDF to ensure metadata and title element are updated)
    rdf_set_work_entity(doc, rdf_find_entity("title"), saveDialog->getDocTitle().c_str());

    Glib::ustring fileName = saveDialog->getFilename();
    Inkscape::Extension::Extension *selectionType = saveDialog->getExtension();

    delete saveDialog;
    saveDialog = nullptr;
    if(doc_title) g_free(doc_title);

    if (!fileName.empty()) {
        Glib::ustring newFileName = Glib::filename_to_utf8(fileName);

        if (!newFileName.empty())
            fileName = newFileName;
        else
            g_warning( "Error converting filename for saving to UTF-8." );

        // FIXME: does the argument !is_copy really convey the correct meaning here?
        success = file_save(parentWindow, doc, fileName, selectionType, TRUE, !is_copy, save_method);

        if (success && doc->getDocumentFilename()) {
            // getDocumentFilename does not return an actual filename... it is an UTF-8 encoded filename (!)
            std::string filename = Glib::filename_from_utf8(doc->getDocumentFilename());
            Glib::ustring uri = Glib::filename_to_uri(filename);

            Glib::RefPtr<Gtk::RecentManager> recent = Gtk::RecentManager::get_default();
            recent->add_item(uri);
        }

        save_path = Glib::path_get_dirname(fileName);
        Inkscape::Extension::store_save_path_in_prefs(save_path, save_method);

        return success;
    }


    return false;
}


/**
 * Save a document, displaying a SaveAs dialog if necessary.
 */
bool
sp_file_save_document(Gtk::Window &parentWindow, SPDocument *doc)
{
    bool success = true;

    if (doc->isModifiedSinceSave()) {
        if ( doc->getDocumentFilename() == nullptr )
        {
            // In this case, an argument should be given that indicates that the document is the first
            // time saved, so that .svg is selected as the default and not the last one "Save as ..." extension used
            return sp_file_save_dialog(parentWindow, doc, Inkscape::Extension::FILE_SAVE_METHOD_INKSCAPE_SVG);
        } else {
            Glib::ustring extension = Inkscape::Extension::get_file_save_extension(Inkscape::Extension::FILE_SAVE_METHOD_SAVE_AS);
            Glib::ustring fn = g_strdup(doc->getDocumentFilename());
            // Try to determine the extension from the filename; this may not lead to a valid extension,
            // but this case is caught in the file_save method below (or rather in Extension::save()
            // further down the line).
            Glib::ustring ext = "";
            Glib::ustring::size_type pos = fn.rfind('.');
            if (pos != Glib::ustring::npos) {
                // FIXME: this could/should be more sophisticated (see FileSaveDialog::appendExtension()),
                // but hopefully it's a reasonable workaround for now
                ext = fn.substr( pos );
            }
            success = file_save(parentWindow, doc, fn, Inkscape::Extension::db.get(ext.c_str()), FALSE, TRUE, Inkscape::Extension::FILE_SAVE_METHOD_SAVE_AS);
            if (success == false) {
                // give the user the chance to change filename or extension
                return sp_file_save_dialog(parentWindow, doc, Inkscape::Extension::FILE_SAVE_METHOD_INKSCAPE_SVG);
            }
        }
    } else {
        Glib::ustring msg;
        if (doc->getDocumentFilename() == nullptr )
        {
            msg = Glib::ustring::format(_("No changes need to be saved."));
        } else {
            msg = Glib::ustring::format(_("No changes need to be saved."), " ", doc->getDocumentFilename());
        }
        SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::WARNING_MESSAGE, msg.c_str());
        success = TRUE;
    }

    return success;
}


/**
 * Save a document.
 */
bool
sp_file_save(Gtk::Window &parentWindow, gpointer /*object*/, gpointer /*data*/)
{
    if (!SP_ACTIVE_DOCUMENT)
        return false;

    SP_ACTIVE_DESKTOP->messageStack()->flash(Inkscape::IMMEDIATE_MESSAGE, _("Saving document..."));

    sp_namedview_document_from_window(SP_ACTIVE_DESKTOP);
    return sp_file_save_document(parentWindow, SP_ACTIVE_DOCUMENT);
}


/**
 *  Save a document, always displaying the SaveAs dialog.
 */
bool
sp_file_save_as(Gtk::Window &parentWindow, gpointer /*object*/, gpointer /*data*/)
{
    if (!SP_ACTIVE_DOCUMENT)
        return false;
    sp_namedview_document_from_window(SP_ACTIVE_DESKTOP);
    return sp_file_save_dialog(parentWindow, SP_ACTIVE_DOCUMENT, Inkscape::Extension::FILE_SAVE_METHOD_SAVE_AS);
}



/**
 *  Save a copy of a document, always displaying a sort of SaveAs dialog.
 */
bool
sp_file_save_a_copy(Gtk::Window &parentWindow, gpointer /*object*/, gpointer /*data*/)
{
    if (!SP_ACTIVE_DOCUMENT)
        return false;
    sp_namedview_document_from_window(SP_ACTIVE_DESKTOP);
    return sp_file_save_dialog(parentWindow, SP_ACTIVE_DOCUMENT, Inkscape::Extension::FILE_SAVE_METHOD_SAVE_COPY);
}

/**
 *  Save a copy of a document as template.
 */
bool
sp_file_save_template(Gtk::Window &parentWindow, Glib::ustring name,
    Glib::ustring author, Glib::ustring description, Glib::ustring keywords,
    bool isDefault)
{
    if (!SP_ACTIVE_DOCUMENT || name.length() == 0)
        return true;

    auto document = SP_ACTIVE_DOCUMENT;

    DocumentUndo::ScopedInsensitive _no_undo(document);

    auto root = document->getReprRoot();
    auto xml_doc = document->getReprDoc();

    auto templateinfo_node = xml_doc->createElement("inkscape:templateinfo");
    Inkscape::GC::release(templateinfo_node);

    auto element_node = xml_doc->createElement("inkscape:name");
    Inkscape::GC::release(element_node);

    element_node->appendChild(xml_doc->createTextNode(name.c_str()));
    templateinfo_node->appendChild(element_node);

    if (author.length() != 0) {

        element_node = xml_doc->createElement("inkscape:author");
        Inkscape::GC::release(element_node);

        element_node->appendChild(xml_doc->createTextNode(author.c_str()));
        templateinfo_node->appendChild(element_node);
    }

    if (description.length() != 0) {

        element_node = xml_doc->createElement("inkscape:shortdesc");
        Inkscape::GC::release(element_node);

        element_node->appendChild(xml_doc->createTextNode(description.c_str()));
        templateinfo_node->appendChild(element_node);

    }

    element_node = xml_doc->createElement("inkscape:date");
    Inkscape::GC::release(element_node);

    element_node->appendChild(xml_doc->createTextNode(
        Glib::DateTime::create_now_local().format("%F").c_str()));
    templateinfo_node->appendChild(element_node);

    if (keywords.length() != 0) {

        element_node = xml_doc->createElement("inkscape:keywords");
        Inkscape::GC::release(element_node);

        element_node->appendChild(xml_doc->createTextNode(keywords.c_str()));
        templateinfo_node->appendChild(element_node);

    }

    root->appendChild(templateinfo_node);

    // Escape filenames for windows users, but filenames are not URIs so
    // Allow UTF-8 and don't escape spaces witch are popular chars.
    auto encodedName = Glib::uri_escape_string(name, " ", true);
    encodedName.append(".svg");

    auto filename = Inkscape::IO::Resource::get_path_ustring(USER, TEMPLATES, encodedName.c_str());

    auto operation_confirmed = sp_ui_overwrite_file(filename.c_str());

    if (operation_confirmed) {

        file_save(parentWindow, document, filename,
            Inkscape::Extension::db.get(".svg"), false, false,
            Inkscape::Extension::FILE_SAVE_METHOD_INKSCAPE_SVG);

        if (isDefault) {
            // save as "default.svg" by default (so it works independently of UI language), unless
            // a localized template like "default.de.svg" is already present (which overrides "default.svg")
            Glib::ustring default_svg_localized = Glib::ustring("default.") + _("en") + ".svg";
            filename = Inkscape::IO::Resource::get_path_ustring(USER, TEMPLATES, default_svg_localized.c_str());

            if (!Inkscape::IO::file_test(filename.c_str(), G_FILE_TEST_EXISTS)) {
                filename = Inkscape::IO::Resource::get_path_ustring(USER, TEMPLATES, "default.svg");
            }

            file_save(parentWindow, document, filename,
                Inkscape::Extension::db.get(".svg"), false, false,
                Inkscape::Extension::FILE_SAVE_METHOD_INKSCAPE_SVG);
        }
    }
    
    // remove this node from current document after saving it as template
    root->removeChild(templateinfo_node);

    return operation_confirmed;
}



/*######################
## I M P O R T
######################*/

/**
 * Paste the contents of a document into the active desktop.
 * @param clipdoc The document to paste
 * @param in_place Whether to paste the selection where it was when copied
 * @pre @c clipdoc is not empty and items can be added to the current layer
 */
void sp_import_document(SPDesktop *desktop, SPDocument *clipdoc, bool in_place, bool on_page)
{
    //TODO: merge with file_import()

    SPDocument *target_document = desktop->getDocument();
    Inkscape::XML::Node *root = clipdoc->getReprRoot();
    Inkscape::XML::Node *target_parent = desktop->layerManager().currentLayer()->getRepr();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // Get page manager for on_page pasting, this must be done before selection changes
    Inkscape::PageManager &pm = target_document->getPageManager();
    SPPage *to_page = pm.getSelected();

    auto *node_after = desktop->getSelection()->topRepr();
    if (node_after && prefs->getBool("/options/paste/aboveselected", true) && node_after != target_parent) {
        target_parent = node_after->parent();
    } else {
        node_after = target_parent->lastChild();
    }

    // copy definitions
    desktop->doc()->importDefs(clipdoc);

    Inkscape::XML::Node* clipboard = nullptr;
    // copy objects
    std::vector<Inkscape::XML::Node*> pasted_objects;
    for (Inkscape::XML::Node *obj = root->firstChild() ; obj ; obj = obj->next()) {
        // Don't copy metadata, defs, named views and internal clipboard contents to the document
        if (!strcmp(obj->name(), "svg:defs")) {
            continue;
        }
        if (!strcmp(obj->name(), "svg:metadata")) {
            continue;
        }
        if (!strcmp(obj->name(), "sodipodi:namedview")) {
            continue;
        }
        if (!strcmp(obj->name(), "inkscape:clipboard")) {
            clipboard = obj;
            continue;
        }

        Inkscape::XML::Node *obj_copy = obj->duplicate(target_document->getReprDoc());
        target_parent->addChild(obj_copy, node_after);
        node_after = obj_copy;
        Inkscape::GC::release(obj_copy);

        pasted_objects.push_back(obj_copy);

        // if we are pasting a clone to an already existing object, its
        // transform is relative to the document, not to its original (see ui/clipboard.cpp)
        auto spobject = target_document->getObjectByRepr(obj_copy);
        auto use = cast<SPUse>(spobject);
        if (use) {
            SPItem *original = use->get_original();
            if (original) {
                Geom::Affine relative_use_transform = original->transform.inverse() * use->transform;
                obj_copy->setAttributeOrRemoveIfEmpty("transform", sp_svg_transform_write(relative_use_transform));
            }
        }
    }

    std::vector<Inkscape::XML::Node*> pasted_objects_not;
    auto layer = desktop->layerManager().currentLayer();
    Geom::Affine doc2parent = layer->i2doc_affine().inverse();

    Geom::OptRect from_page;
    if (clipboard) {
        if (clipboard->attribute("page-min")) {
            from_page = Geom::OptRect(clipboard->getAttributePoint("page-min"), clipboard->getAttributePoint("page-max"));
        }

        for (Inkscape::XML::Node *obj = clipboard->firstChild(); obj; obj = obj->next()) {
            if (target_document->getObjectById(obj->attribute("id")))
                continue;
            Inkscape::XML::Node *obj_copy = obj->duplicate(target_document->getReprDoc());
            layer->appendChildRepr(obj_copy);
            Inkscape::GC::release(obj_copy);
            pasted_objects_not.push_back(obj_copy);
        }
    }
    target_document->ensureUpToDate();
    Inkscape::Selection *selection = desktop->getSelection();
    selection->setReprList(pasted_objects_not);

    selection->deleteItems(true);

    // Change the selection to the freshly pasted objects
    selection->setReprList(pasted_objects);
    for (auto item : selection->items()) {
        auto pasted_lpe_item = cast<SPLPEItem>(item);
        if (pasted_lpe_item) {
            sp_lpe_item_enable_path_effects(pasted_lpe_item, false);
        }
    }
    // Apply inverse of parent transform
    selection->applyAffine(desktop->dt2doc() * doc2parent * desktop->doc2dt(), true, false, false);

    // Update (among other things) all curves in paths, for bounds() to work
    target_document->ensureUpToDate();


    // move selection either to original position (in_place) or to mouse pointer
    Geom::OptRect sel_bbox = selection->visualBounds();
    if (sel_bbox) {
        // get offset of selection to original position of copied elements
        Geom::Point pos_original;
        Inkscape::XML::Node *clipnode = sp_repr_lookup_name(root, "inkscape:clipboard", 1);
        if (clipnode) {
            Geom::Point min, max;
            min = clipnode->getAttributePoint("min", min);
            max = clipnode->getAttributePoint("max", max);
            pos_original = Geom::Point(min[Geom::X], max[Geom::Y]);
        }
        Geom::Point offset = pos_original - sel_bbox->corner(3);

        if (!in_place) {
            SnapManager &m = desktop->namedview->snap_manager;
            m.setup(desktop);
            desktop->event_context->discard_delayed_snap_event();

            // get offset from mouse pointer to bbox center, snap to grid if enabled
            Geom::Point mouse_offset = desktop->point() - sel_bbox->midpoint();
            offset = m.multipleOfGridPitch(mouse_offset - offset, sel_bbox->midpoint() + offset) + offset;
            // Integer align for mouse pasting
            offset = offset.round();
            m.unSetup();
        } else if (on_page && from_page && to_page) {
            // Moving to the same location on a different page requires us to remove the original page translation
            offset *= Geom::Translate(from_page->min()).inverse();
            // Then add the new page's transform on top.
            offset *= Geom::Translate(to_page->getDesktopRect().min());
        }

        selection->moveRelative(offset);
        for (auto po : pasted_objects) {
            auto lpeitem = cast<SPLPEItem>(target_document->getObjectByRepr(po));
            if (lpeitem) {
                sp_lpe_item_enable_path_effects(lpeitem, true);
            }
        }
    }
    target_document->emitReconstructionFinish();
}


/**
 *  Import a resource.  Called by sp_file_import()
 */
SPObject *
file_import(SPDocument *in_doc, const Glib::ustring &uri,
               Inkscape::Extension::Extension *key)
{
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    bool cancelled = false;
    auto prefs = Inkscape::Preferences::get();
    bool onimport = prefs->getBool("/options/onimport", true);

    // store mouse pointer location before opening any dialogs, so we can drop the item where initially intended
    auto pointer_location = desktop->point();

    //DEBUG_MESSAGE( fileImport, "file_import( in_doc:%p uri:[%s], key:%p", in_doc, uri, key );
    SPDocument *doc;
    try {
        doc = Inkscape::Extension::open(key, uri.c_str());
    } catch (Inkscape::Extension::Input::no_extension_found &e) {
        doc = nullptr;
    } catch (Inkscape::Extension::Input::open_failed &e) {
        doc = nullptr;
    } catch (Inkscape::Extension::Input::open_cancelled &e) {
        doc = nullptr;
        cancelled = true;
    }

    if (onimport && !prefs->getBool("/options/onimport", true)) {
        // Opened instead of imported (onimport set to false in Svg::open)
        prefs->setBool("/options/onimport", true);
        return nullptr;
    } else if (doc != nullptr) {
        // Always preserve any imported text kerning / formatting
        auto root_repr = in_doc->getReprRoot();
        root_repr->setAttribute("xml:space", "preserve");

        Inkscape::XML::rebase_hrefs(doc, in_doc->getDocumentBase(), false);
        Inkscape::XML::Document *xml_in_doc = in_doc->getReprDoc();
        prevent_id_clashes(doc, in_doc, true);
        sp_file_fix_lpe(doc);

        in_doc->importDefs(doc);

        // The extension should set it's pages enabled or disabled when opening
        // in order to indicate if pages are being imported or if objects are.
        if (doc->getPageManager().hasPages()) {
            file_import_pages(in_doc, doc);
            DocumentUndo::done(in_doc, _("Import Pages"), INKSCAPE_ICON("document-import"));
            // This return is only used by dbus in document-interface.cpp (now removed).
            return nullptr;
        }

        SPCSSAttr *style = sp_css_attr_from_object(doc->getRoot());

        // Count the number of top-level items in the imported document.
        guint items_count = 0;
        SPObject *o = nullptr;
        for (auto& child: doc->getRoot()->children) {
            if (is<SPItem>(&child)) {
                items_count++;
                o = &child;
            }
        }

        //ungroup if necessary
        bool did_ungroup = false;
        while(items_count==1 && o && is<SPGroup>(o) && o->children.size()==1){
            std::vector<SPItem *>v;
            sp_item_group_ungroup(cast<SPGroup>(o), v);
            o = v.empty() ? nullptr : v[0];
            did_ungroup=true;
        }

        // Create a new group if necessary.
        Inkscape::XML::Node *newgroup = nullptr;
        const auto & al = style->attributeList();
        if ((style && !al.empty()) || items_count > 1) {
            newgroup = xml_in_doc->createElement("svg:g");
            sp_repr_css_set(newgroup, style, "style");
        }

        // Determine the place to insert the new object.
        // This will be the current layer, if possible.
        // FIXME: If there's no desktop (command line run?) we need
        //        a document:: method to return the current layer.
        //        For now, we just use the root in this case.
        SPObject *place_to_insert;
        if (desktop) {
            place_to_insert = desktop->layerManager().currentLayer();
        } else {
            place_to_insert = in_doc->getRoot();
        }

        // Construct a new object representing the imported image,
        // and insert it into the current document.
        SPObject *new_obj = nullptr;
        for (auto& child: doc->getRoot()->children) {
            if (is<SPItem>(&child)) {
                Inkscape::XML::Node *newitem = did_ungroup ? o->getRepr()->duplicate(xml_in_doc) : child.getRepr()->duplicate(xml_in_doc);

                // convert layers to groups, and make sure they are unlocked
                // FIXME: add "preserve layers" mode where each layer from
                //        import is copied to the same-named layer in host
                newitem->removeAttribute("inkscape:groupmode");
                newitem->removeAttribute("sodipodi:insensitive");

                if (newgroup) newgroup->appendChild(newitem);
                else new_obj = place_to_insert->appendChildRepr(newitem);
            }

            // don't lose top-level defs or style elements
            else if (child.getRepr()->type() == Inkscape::XML::NodeType::ELEMENT_NODE) {
                const gchar *tag = child.getRepr()->name();
                if (!strcmp(tag, "svg:style")) {
                    in_doc->getRoot()->appendChildRepr(child.getRepr()->duplicate(xml_in_doc));
                }
            }
        }
        in_doc->emitReconstructionFinish();
        if (newgroup) new_obj = place_to_insert->appendChildRepr(newgroup);

        // release some stuff
        if (newgroup) Inkscape::GC::release(newgroup);
        if (style) sp_repr_css_attr_unref(style);

        // select and move the imported item
        if (new_obj && is<SPItem>(new_obj)) {
            Inkscape::Selection *selection = desktop->getSelection();
            selection->set(cast<SPItem>(new_obj));

            // preserve parent and viewBox transformations
            // c2p is identity matrix at this point unless ensureUpToDate is called
            doc->ensureUpToDate();
            Geom::Affine affine = doc->getRoot()->c2p * cast<SPItem>(place_to_insert)->i2doc_affine().inverse();
            selection->applyAffine(desktop->dt2doc() * affine * desktop->doc2dt(), true, false, false);

            // move to mouse pointer
            {
                desktop->getDocument()->ensureUpToDate();
                Geom::OptRect sel_bbox = selection->visualBounds();
                if (sel_bbox) {
                    Geom::Point m( pointer_location - sel_bbox->midpoint() );
                    selection->moveRelative(m, false);
                }
            }
        }
        
        DocumentUndo::done(in_doc, _("Import"), INKSCAPE_ICON("document-import"));
        return new_obj;
    } else if (!cancelled) {
        gchar *text = g_strdup_printf(_("Failed to load the requested file %s"), uri.c_str());
        sp_ui_error_dialog(text);
        g_free(text);
    }

    return nullptr;
}

/**
 * Import the given document as a set of multiple pages and append to this one.
 *
 * @param this_doc - Our current document, to be changed
 * @param that_doc - The documennt that contains our importable pages
 */
void file_import_pages(SPDocument *this_doc, SPDocument *that_doc)
{
    auto &this_pm = this_doc->getPageManager();
    auto &that_pm = that_doc->getPageManager();
    auto this_root = this_doc->getReprRoot();
    auto that_root = that_doc->getReprRoot();

    // Make sure objects have visualBounds created for import
    that_doc->ensureUpToDate();
    this_pm.enablePages();

    Geom::Affine tr = Geom::Translate(this_pm.nextPageLocation() * this_doc->getDocumentScale());
    for (auto &that_page : that_pm.getPages()) {
        auto this_page = this_pm.newDocumentPage(that_page->getDocumentRect() * tr);
        // Set the margin, bleed, etc
        this_page->copyFrom(that_page);
    }

    // Unwind the document scales for the imported objects
    tr = this_doc->getDocumentScale().inverse() * that_doc->getDocumentScale() * tr;
    Inkscape::ObjectSet set(this_doc);
    for (Inkscape::XML::Node *that_repr = that_root->firstChild(); that_repr; that_repr = that_repr->next()) {
        // Don't copy metadata, defs, named views and internal clipboard contents to the document
        if (!strcmp(that_repr->name(), "svg:defs") ||
            !strcmp(that_repr->name(), "svg:metadata") ||
            !strcmp(that_repr->name(), "sodipodi:namedview")) {
            continue;
        }

        auto this_repr = that_repr->duplicate(this_doc->getReprDoc());
        this_root->addChild(this_repr, this_root->lastChild());
        Inkscape::GC::release(this_repr);
        if (auto this_item = this_doc->getObjectByRepr(this_repr)) {
            set.add(this_item);
        }
    }
    set.applyAffine(tr, true, false, true);
}

/**
 *  Display an Open dialog, import a resource if OK pressed.
 */
void
sp_file_import(Gtk::Window &parentWindow)
{
    static Glib::ustring import_path;

    SPDocument *doc = SP_ACTIVE_DOCUMENT;
    if (!doc)
        return;

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if(import_path.empty())
    {
        Glib::ustring attr = prefs->getString("/dialogs/import/path");
        if (!attr.empty()) import_path = attr;
    }

    //# Test if the import_path directory exists
    if (!Inkscape::IO::file_test(import_path.c_str(),
              (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
        import_path = "";

    //# If no open path, default to our home directory
    if (import_path.empty())
    {
        import_path = g_get_home_dir();
        import_path.append(G_DIR_SEPARATOR_S);
    }

    // Create new dialog (don't use an old one, because parentWindow has probably changed)
    Inkscape::UI::Dialog::FileOpenDialog *importDialogInstance =
             Inkscape::UI::Dialog::FileOpenDialog::create(
                 parentWindow,
                 import_path,
                 Inkscape::UI::Dialog::IMPORT_TYPES,
                 (char const *)_("Select file to import"));

    bool success = importDialogInstance->show();
    if (!success) {
        delete importDialogInstance;
        return;
    }

    typedef std::vector<Glib::ustring> pathnames;
    pathnames flist(importDialogInstance->getFilenames());

    // Get file name and extension type
    Glib::ustring fileName = importDialogInstance->getFilename();
    Inkscape::Extension::Extension *selection = importDialogInstance->getExtension();

    delete importDialogInstance;
    importDialogInstance = nullptr;

    //# Iterate through filenames if more than 1
    if (flist.size() > 1)
    {
        for (const auto & i : flist)
        {
            fileName = i;

            Glib::ustring newFileName = Glib::filename_to_utf8(fileName);
            if (!newFileName.empty())
                fileName = newFileName;
            else
                g_warning("ERROR CONVERTING IMPORT FILENAME TO UTF-8");

#ifdef INK_DUMP_FILENAME_CONV
            g_message("Importing File %s\n", fileName.c_str());
#endif
            file_import(doc, fileName, selection);
        }

        return;
    }


    if (!fileName.empty()) {

        Glib::ustring newFileName = Glib::filename_to_utf8(fileName);

        if (!newFileName.empty())
            fileName = newFileName;
        else
            g_warning("ERROR CONVERTING IMPORT FILENAME TO UTF-8");

        import_path = Glib::path_get_dirname(fileName);
        import_path.append(G_DIR_SEPARATOR_S);
        prefs->setString("/dialogs/import/path", import_path);

        file_import(doc, fileName, selection);
    }

    return;
}

/*######################
## P R I N T
######################*/


/**
 *  Print the current document, if any.
 */
void
sp_file_print(Gtk::Window& parentWindow)
{
    SPDocument *doc = SP_ACTIVE_DOCUMENT;
    if (doc)
        sp_print_document(parentWindow, doc);
}


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
