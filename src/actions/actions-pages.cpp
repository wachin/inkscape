// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gio::Actions for pages, mostly for the toolbar.
 *
 * Copyright (C) 2021 Martin Owens
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include <iostream>

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-pages.h"
#include "actions-helper.h"
#include "inkscape-application.h"
#include "inkscape-window.h"
#include "document-undo.h"

#include "page-manager.h"
#include "object/sp-page.h"
#include "ui/icon-names.h"

void page_new(SPDocument *document)
{
    document->getPageManager().selectPage(document->getPageManager().newPage());
    Inkscape::DocumentUndo::done(document, "New Automatic Page", INKSCAPE_ICON("tool-pages"));
}

void page_new_and_center(SPDesktop *desktop)
{
    if (auto document = desktop->getDocument()) {
        page_new(document);
        document->getPageManager().centerToSelectedPage(desktop);
    }
}

void page_delete(SPDocument *document)
{
    // Delete page's content if move_objects is checked.
    document->getPageManager().deletePage(document->getPageManager().move_objects());
    Inkscape::DocumentUndo::done(document, "Delete Page", INKSCAPE_ICON("tool-pages"));
}

void page_delete_and_center(SPDesktop *desktop)
{
    if (auto document = desktop->getDocument()) {
        page_delete(document);
        document->getPageManager().centerToSelectedPage(desktop);
    }
}

void page_backward(SPDocument *document)
{
    auto &page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() - 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, "Shift Page Backwards", INKSCAPE_ICON("tool-pages"));
        }
    }
}

void page_forward(SPDocument *document)
{
    auto &page_manager = document->getPageManager();
    if (auto page = page_manager.getSelected()) {
        if (page->setPageIndex(page->getPageIndex() + 1, page_manager.move_objects())) {
            Inkscape::DocumentUndo::done(document, "Shift Page Forewards", INKSCAPE_ICON("tool-pages"));
        }
    }
}

void set_move_objects(SPDocument *doc)
{
    if (auto action = doc->getActionGroup()->lookup_action("page-move-objects")) {
        bool active = false;
        action->get_state(active);
        active = !active; // toggle
        action->change_state(active);

        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        prefs->setBool("/tools/pages/move_objects", active);
    } else {
        g_warning("Can't find page-move-objects action group!");
    }
}

std::vector<std::vector<Glib::ustring>> doc_page_actions =
{
    // clang-format off
    {"doc.page-new",               N_("New Page"),                "Page",     N_("Create a new page")                                  },
    {"doc.page-delete",            N_("Delete Page"),             "Page",     N_("Delete the selected page")                           },
    {"doc.page-move-objects",      N_("Move Objects with Page"),  "Page",     N_("Move overlapping objects as the page is moved")     },
    {"doc.page-move-backward",     N_("Move Before Previous"),    "Page",     N_("Move page backwards in the page order")              },
    {"doc.page-move-forward",      N_("Move After Next"),         "Page",     N_("Move page forwards in the page order")               },
    // clang-format on
};

void add_actions_pages(SPDocument* doc)
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    auto group = doc->getActionGroup();
    group->add_action("page-new", sigc::bind<SPDocument*>(sigc::ptr_fun(&page_new), doc));
    group->add_action("page-delete", sigc::bind<SPDocument*>(sigc::ptr_fun(&page_delete), doc));
    group->add_action("page-move-backward", sigc::bind<SPDocument*>(sigc::ptr_fun(&page_backward), doc));
    group->add_action("page-move-forward", sigc::bind<SPDocument*>(sigc::ptr_fun(&page_forward), doc));
    group->add_action_bool("page-move-objects", sigc::bind<SPDocument*>(sigc::ptr_fun(&set_move_objects), doc),
        prefs->getBool("/tools/pages/move_objects", true));

    // Note: This will only work for the first ux to load, possible problem.
    auto app = InkscapeApplication::instance();
    if (!app) {
        show_output("add_actions_pages: no app!");
        return;
    }
    app->get_action_extra_data().add_data(doc_page_actions);
}

std::vector<std::vector<Glib::ustring>> win_page_actions =
{
    // clang-format off
    {"win.page-new",    N_("New Page"),    "Page", N_("Create a new page and center view on it")},
    {"win.page-delete", N_("Delete Page"), "Page", N_("Delete the selected page and center view on next page")},
    // clang-format on
};

void add_actions_page_tools(InkscapeWindow* win)
{
    auto desktop = win->get_desktop();

    win->add_action("page-new", sigc::bind<SPDesktop*>(sigc::ptr_fun(&page_new_and_center), desktop));
    win->add_action("page-delete", sigc::bind<SPDesktop*>(sigc::ptr_fun(&page_delete_and_center), desktop));

    auto app = InkscapeApplication::instance();
    app->get_action_extra_data().add_data(win_page_actions);
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
