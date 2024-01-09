// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions Related to Editing which require document
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-edit-document.h"
#include "actions/actions-extra-data.h"
#include "actions-helper.h"
#include "inkscape-application.h"
#include "document.h"
#include "inkscape.h"
#include "selection-chemistry.h"
#include "object/sp-guide.h"
#include "object/sp-namedview.h"

void
create_guides_around_page(SPDocument* document)
{
    // Create Guides Around the Page
    sp_guide_create_guides_around_page(document);
}

void
lock_all_guides(SPDocument *document)
{
    document->getNamedView()->toggleLockGuides();
}

void
show_all_guides(SPDocument *document)
{
    document->getNamedView()->toggleShowGuides();
}

void
delete_all_guides(SPDocument* document)
{
    // Delete All Guides
    sp_guide_delete_all_guides(document);
}

void
fit_canvas_drawing(SPDocument* document)
{
    // Fit Page to Drawing
    if (fit_canvas_to_drawing(document)) {
        Inkscape::DocumentUndo::done(document, _("Fit Page to Drawing"), "");
    }
}

void
set_display_unit(Glib::ustring abbr, SPDocument* document)
{
    // This does not modify the scale of the document, just the units
    Inkscape::XML::Node *repr = document->getNamedView()->getRepr();
    repr->setAttribute("inkscape:document-units", abbr);
    document->setModifiedSinceSave();
    Inkscape::DocumentUndo::done(document, _("Changed default display unit"), "");
}

void toggle_clip_to_page(SPDocument* document) {
    if (!document || !document->getNamedView()) return;

    auto clip = !document->getNamedView()->clip_to_page;
    document->getNamedView()->change_bool_setting(SPAttr::INKSCAPE_CLIP_TO_PAGE_RENDERING, clip);
    document->setModifiedSinceSave();
    Inkscape::DocumentUndo::done(document, _("Clip to page"), "");
}

void
show_grids(SPDocument *document)
{
    document->getNamedView()->toggleShowGrids();
}

std::vector<std::vector<Glib::ustring>> raw_data_edit_document = {
    // clang-format off
    {"doc.create-guides-around-page",            N_("Create Guides Around the Current Page"),    "Edit Document",     N_("Create four guides aligned with the page borders of the current page")},
    {"doc.lock-all-guides",                      N_("Lock All Guides"),                          "Edit Document",     N_("Toggle lock of all guides in the document")},
    {"doc.show-all-guides",                      N_("Show All Guides"),                          "Edit Document",     N_("Toggle visibility of all guides in the document")},
    {"doc.delete-all-guides",                    N_("Delete All Guides"),                        "Edit Document",     N_("Delete all the guides in the document")},
    {"doc.fit-canvas-to-drawing",                N_("Fit Page to Drawing"),                      "Edit Document",     N_("Fit the page to the drawing")},
    {"doc.clip-to-page"                      ,   N_("Toggle Clip to Page"),                      "Edit Document",     N_("Toggle between clipped to page and complete rendering")},
    {"doc.show-grids",                           N_("Show Grids"),                               "Edit Document",     N_("Toggle the visibility of grids")},
    // clang-format on
};

void
add_actions_edit_document(SPDocument* document)
{
    Glib::RefPtr<Gio::SimpleActionGroup> map = document->getActionGroup();

    // clang-format off
    map->add_action( "create-guides-around-page",           sigc::bind<SPDocument*>(sigc::ptr_fun(&create_guides_around_page),  document));
    map->add_action( "delete-all-guides",                   sigc::bind<SPDocument*>(sigc::ptr_fun(&delete_all_guides),  document));
    map->add_action( "fit-canvas-to-drawing",               sigc::bind<SPDocument*>(sigc::ptr_fun(&fit_canvas_drawing),  document));
    map->add_action_bool( "lock-all-guides",                sigc::bind<SPDocument*>(sigc::ptr_fun(&lock_all_guides),   document));
    map->add_action_bool( "show-all-guides",                sigc::bind<SPDocument*>(sigc::ptr_fun(&show_all_guides),   document));
    map->add_action_bool( "show-grids",                     sigc::bind<SPDocument*>(sigc::ptr_fun(&show_grids),   document));

    map->add_action_radio_string("set-display-unit",        sigc::bind<SPDocument*>(sigc::ptr_fun(&set_display_unit), document), "px");
    map->add_action("clip-to-page",                         [=](){ toggle_clip_to_page(document); });
    // clang-format on

    // Check if there is already an application instance (GUI or non-GUI).
    auto app = InkscapeApplication::instance();
    if (!app) {
        show_output("add_actions_edit_document: no app!");
        return;
    }
    app->get_action_extra_data().add_data(raw_data_edit_document);
}
