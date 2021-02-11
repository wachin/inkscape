// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for file handling tied to the application and without GUI.
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include <iostream>

#include <giomm.h>  // Not <gtkmm.h>! To eventually allow a headless version!
#include <glibmm/i18n.h>

#include "actions-file.h"
#include "actions-helper.h"
#include "inkscape-application.h"

#include "inkscape.h"             // Inkscape::Application
#include "helper/action-context.h"

// Actions for file handling (should be integrated with file dialog).

void
file_open(const Glib::VariantBase& value, InkscapeApplication *app)
{
    Glib::Variant<Glib::ustring> s = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring> >(value);

    Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(s.get());
    if (!file->query_exists()) {
        std::cerr << "file_open: file '" << s.get() << "' does not exist." << std::endl;
        return;
    }

    SPDocument *document = app->document_open(file);
    INKSCAPE.add_document(document);

    Inkscape::ActionContext context = INKSCAPE.action_context_for_document(document);
    app->set_active_document(document);
    app->set_active_selection(context.getSelection());
    app->set_active_view(context.getView());

    document->ensureUpToDate();
}

void
file_new(const Glib::VariantBase& value, InkscapeApplication *app)
{
    Glib::Variant<Glib::ustring> s = Glib::VariantBase::cast_dynamic<Glib::Variant<Glib::ustring> >(value);

    SPDocument *document = app->document_new(s.get());
    INKSCAPE.add_document(document);

    Inkscape::ActionContext context = INKSCAPE.action_context_for_document(document);
    app->set_active_document(document);
    app->set_active_selection(context.getSelection());
    app->set_active_view(context.getView());

    document->ensureUpToDate();
}

// Need to redo document_revert so that it doesn't depend on windows.
// void
// file_revert(InkscapeApplication *app)
// {
//     app->document_revert(app->get_current_document());
// }

void
file_close(InkscapeApplication *app)
{
    SPDocument *document = app->get_active_document();
    app->document_close(document);

    app->set_active_document(nullptr);
    app->set_active_selection(nullptr);
    app->set_active_view(nullptr);
}

// TODO:
// file_open
// file_new

// The following might be best tied to the file rather than app.
// file_revert
// file_save
// file_saveas
// file_saveacopy
// file_print
// file_vacuum
// file_import
// file_close
// file_quit ... should just be quit
// file_template

std::vector<std::vector<Glib::ustring>> raw_data_file =
{
    {"file-open",                 "FileOpen",                "File",       N_("Open file.")                                         },
    {"file-new",                  "FileNew",                 "File",       N_("Open new document using template.")                  },
    {"file-close",                "FileClose",               "File",       N_("Close active document.")                             }
};

template <class T>
void
add_actions_file(ConcreteInkscapeApplication<T>* app)
{
    Glib::VariantType Bool(  Glib::VARIANT_TYPE_BOOL);
    Glib::VariantType Int(   Glib::VARIANT_TYPE_INT32);
    Glib::VariantType Double(Glib::VARIANT_TYPE_DOUBLE);
    Glib::VariantType String(Glib::VARIANT_TYPE_STRING);
    Glib::VariantType BString(Glib::VARIANT_TYPE_BYTESTRING);

    // Debian 9 has 2.50.0
#if GLIB_CHECK_VERSION(2, 52, 0)

    app->add_action_with_parameter( "file-open",                 String, sigc::bind<InkscapeApplication*>(sigc::ptr_fun(&file_open),           app));
    app->add_action_with_parameter( "file-new",                  String, sigc::bind<InkscapeApplication*>(sigc::ptr_fun(&file_new),            app));
    app->add_action(                "file-close",                        sigc::bind<InkscapeApplication*>(sigc::ptr_fun(&file_close),          app));
#else
    std::cerr << "add_actions: Some actions require Glibmm 2.52, compiled with: " << glib_major_version << "." << glib_minor_version << std::endl;
#endif

    app->get_action_extra_data().add_data(raw_data_file);
}


template void add_actions_file(ConcreteInkscapeApplication<Gio::Application>* app);
template void add_actions_file(ConcreteInkscapeApplication<Gtk::Application>* app);



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
