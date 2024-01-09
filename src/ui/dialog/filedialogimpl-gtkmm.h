// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Implementation of the file dialog interfaces defined in filedialogimpl.h
 */
/* Authors:
 *   Bob Jamison
 *   Johan Engelen <johan@shouraizou.nl>
 *   Joel Holdsworth
 *   Bruno Dilly
 *   Other dudes from The Inkscape Organization
 *
 * Copyright (C) 2004-2008 Authors
 * Copyright (C) 2004-2007 The Inkscape Organization
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef __FILE_DIALOGIMPL_H__
#define __FILE_DIALOGIMPL_H__

//Gtk includes
#include <gtkmm/filechooserdialog.h>
#include <glib/gstdio.h>

#include "filedialog.h"
#include "svg-preview.h"

namespace Gtk {
class CheckButton;
class ComboBoxText;
class Expander;
}

namespace Inkscape {
  class URI;

namespace UI {

namespace View {
  class SVGViewWidget;
}

namespace Dialog {

/*#########################################################################
### Utility
#########################################################################*/

void
findEntryWidgets(Gtk::Container *parent,
                 std::vector<Gtk::Entry *> &result);

void
findExpanderWidgets(Gtk::Container *parent,
                    std::vector<Gtk::Expander *> &result);

/*#########################################################################
### F I L E     D I A L O G    B A S E    C L A S S
#########################################################################*/

/**
 * This class is the base implementation for the others.  This
 * reduces redundancies and bugs.
 */
class FileDialogBaseGtk : public Gtk::FileChooserDialog
{
public:

    FileDialogBaseGtk(Gtk::Window& parentWindow, const Glib::ustring &title,
    		Gtk::FileChooserAction dialogType, FileDialogType type, gchar const* preferenceBase) :
        Gtk::FileChooserDialog(parentWindow, title, dialogType),
        preferenceBase(preferenceBase ? preferenceBase : "unknown"),
        _dialogType(type)
    {
        internalSetup();
    }

    FileDialogBaseGtk(Gtk::Window& parentWindow, const char *title,
                   Gtk::FileChooserAction dialogType, FileDialogType type, gchar const* preferenceBase) :
        Gtk::FileChooserDialog(parentWindow, title, dialogType),
        preferenceBase(preferenceBase ? preferenceBase : "unknown"),
        _dialogType(type)
    {
        internalSetup();
    }

    ~FileDialogBaseGtk() override = default;

    /**
     * Add a Gtk filter to our specially controlled filter dropdown.
     */
    Glib::RefPtr<Gtk::FileFilter> addFilter(const Glib::ustring &name, Glib::ustring pattern = "",
                                            Inkscape::Extension::Extension *mod = nullptr);

    Glib::ustring extToPattern(const Glib::ustring &extension) const;

    virtual void filterChangedCallback() {}

protected:
    void cleanup( bool showConfirmed );

    Glib::ustring const preferenceBase;
    /**
     * What type of 'open' are we? (open, import, place, etc)
     */
    FileDialogType _dialogType;

    /**
     * Our svg preview widget
     */
    SVGPreview svgPreview;

    /**
     * Child widgets
     */
    Gtk::CheckButton previewCheckbox;
    Gtk::CheckButton svgexportCheckbox;

    /**
     * Aquired Widgets
     */
    Gtk::ComboBoxText *filterComboBox;

private:
    void internalSetup();

    /**
     * Callback for seeing if the preview needs to be drawn
     */
    void _updatePreviewCallback();

    /**
     * Callback to for SVG 2 to SVG 1.1 export.
     */
    void _svgexportEnabledCB();

    /**
     * Overriden filter store.
     */
    Glib::RefPtr<Gtk::ListStore> filterStore;
};




/*#########################################################################
### F I L E    O P E N
#########################################################################*/

/**
 * Our implementation class for the FileOpenDialog interface..
 */
class FileOpenDialogImplGtk : public FileOpenDialog, public FileDialogBaseGtk
{
public:

    FileOpenDialogImplGtk(Gtk::Window& parentWindow,
    		       const Glib::ustring &dir,
                       FileDialogType fileTypes,
                       const Glib::ustring &title);

    ~FileOpenDialogImplGtk() override = default;

    bool show() override;

    Glib::ustring getFilename();

    std::vector<Glib::ustring> getFilenames() override;

    Glib::ustring getCurrentDirectory() override;

    void addFilterMenu(const Glib::ustring &name, Glib::ustring pattern = "",
                       Inkscape::Extension::Extension *mod = nullptr) override
    {
        addFilter(name, pattern, mod);
    }

private:

    /**
     *  Create a filter menu for this type of dialog
     */
    void createFilterMenu();
};



//########################################################################
//# F I L E    S A V E
//########################################################################

/**
 * Our implementation of the FileSaveDialog interface.
 */
class FileSaveDialogImplGtk : public FileSaveDialog, public FileDialogBaseGtk
{

public:
    FileSaveDialogImplGtk(Gtk::Window &parentWindow,
                          const Glib::ustring &dir,
                          FileDialogType fileTypes,
                          const Glib::ustring &title,
                          const Glib::ustring &default_key,
                          const gchar* docTitle,
                          const Inkscape::Extension::FileSaveMethod save_method);

    ~FileSaveDialogImplGtk() override = default;

    bool show() override;

    Glib::ustring getCurrentDirectory() override;

    void setExtension(Inkscape::Extension::Extension *key) override;

    void addFilterMenu(const Glib::ustring &name, Glib::ustring pattern = "",
                       Inkscape::Extension::Extension *mod = nullptr) override
    {
        addFilter(name, pattern, mod);
    }

private:
    //void change_title(const Glib::ustring& title);
    void change_path(const Glib::ustring& path);
    void updateNameAndExtension();

    /**
     * The file save method (essentially whether the dialog was invoked by "Save as ..." or "Save a
     * copy ..."), which is used to determine file extensions and save paths.
     */
    Inkscape::Extension::FileSaveMethod save_method;

    /**
     * Fix to allow the user to type the file name
     */
    Gtk::Entry *fileNameEntry;
    Gtk::Box childBox;
    Gtk::Box checksBox;
    Gtk::CheckButton fileTypeCheckbox;

    /**
     * Callback for user input into fileNameEntry
     */
    void filterChangedCallback() override;

    /**
     *  Create a filter menu for this type of dialog
     */
    void createFilterMenu();

    /**
     * Callback for user input into fileNameEntry
     */
    void fileNameEntryChangedCallback();
    void fileNameChanged();
    bool fromCB;
};


} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif /*__FILE_DIALOGIMPL_H__*/

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
