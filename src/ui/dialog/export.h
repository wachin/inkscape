// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Anshudhar Kumar Singh <anshudhar2001@gmail.com>
 *
 * Copyright (C) 1999-2007, 2021 Authors
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SP_EXPORT_H
#define SP_EXPORT_H

#include <gtkmm.h>

#include "ui/dialog/dialog-base.h"
#include "ui/widget/scrollprotected.h"

class SPPage;

namespace Inkscape {
    class Preferences;
    namespace Util {
        class Unit;
    }
    namespace Extension {
        class Output;
    }

namespace UI {
namespace Dialog {
    class SingleExport;
    class BatchExport;

enum notebook_page
{
    SINGLE_IMAGE = 0,
    BATCH_EXPORT
};

void set_export_bg_color(SPObject* object, guint32 color);
guint32 get_export_bg_color(SPObject* object, guint32 default_color);

class Export : public DialogBase
{
public:
    Export();
    ~Export() override = default;

private:
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::Box *container = nullptr;            // Main Container
    Gtk::Notebook *export_notebook = nullptr; // Notebook Container for single and batch export

    SingleExport *single_image = nullptr;
    BatchExport *batch_export = nullptr;

    Inkscape::Preferences *prefs = nullptr;

    // setup default values of widgets
    void setDefaultNotebookPage();
    std::map<notebook_page, int> pages;

    sigc::connection notebook_signal;

    // signals callback
    void onNotebookPageSwitch(Widget *page, guint page_number);
    void documentReplaced() override;
    void desktopReplaced() override;
    void selectionChanged(Inkscape::Selection *selection) override;
    void selectionModified(Inkscape::Selection *selection, guint flags) override;

public:
    static std::string absolutizePath(SPDocument *doc, const std::string &filename);
    static bool unConflictFilename(SPDocument *doc, Glib::ustring &filename, Glib::ustring const extension);
    static std::string filePathFromObject(SPDocument *doc, SPObject *obj, const Glib::ustring &file_entry_text);
    static std::string filePathFromId(SPDocument *doc, Glib::ustring id, const Glib::ustring &file_entry_text);
    static Glib::ustring defaultFilename(SPDocument *doc, Glib::ustring &filename_entry_text, Glib::ustring extension);

    static bool exportRaster(
        Geom::Rect const &area, unsigned long int const &width, unsigned long int const &height,
        float const &dpi, guint32 bg_color, Glib::ustring const &filename, bool overwrite,
        unsigned (*callback)(float, void *), void *data,
        Inkscape::Extension::Output *extension, std::vector<SPItem *> *items = nullptr);
  
    static bool exportVector(
        Inkscape::Extension::Output *extension, SPDocument *doc, Glib::ustring const &filename,
        bool overwrite, const std::vector<SPItem *> &items, SPPage *page);
    static bool exportVector(
        Inkscape::Extension::Output *extension, SPDocument *doc, Glib::ustring const &filename,
        bool overwrite, const std::vector<SPItem *> &items, const std::vector<SPPage *> &pages);

};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape
#endif

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
