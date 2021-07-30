// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Jon A. Cruz
 *
 * Copyright (C) 2010 Jon A. Cruz
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_DIALOGS_GLYPHS_H
#define SEEN_DIALOGS_GLYPHS_H

#include <gtkmm/treemodel.h>

#include "ui/dialog/dialog-base.h"

namespace Gtk {
class ComboBoxText;
class Entry;
class IconView;
class Label;
class ListStore;
}

namespace Inkscape {
namespace UI {

namespace Widget {
class FontSelector;
}

namespace Dialog {

class GlyphColumns;

/**
 * A panel that displays character glyphs.
 */
class GlyphsPanel : public DialogBase
{
public:
    GlyphsPanel();
    ~GlyphsPanel() override;

    static GlyphsPanel& getInstance();

    void selectionChanged(Selection *selection) override;
    void selectionModified(Selection *selection, guint flags) override;

protected:

private:
    GlyphsPanel(GlyphsPanel const &) = delete; // no copy
    GlyphsPanel &operator=(GlyphsPanel const &) = delete; // no assign

    static GlyphColumns *getColumns();

    void rebuild();

    void glyphActivated(Gtk::TreeModel::Path const & path);
    void glyphSelectionChanged();
    void readSelection( bool updateStyle, bool updateContent );
    void calcCanInsert();
    void insertText();

    Glib::RefPtr<Gtk::ListStore> store;
    Gtk::IconView *iconView;
    Glib::RefPtr<Gtk::Entry> entry;
    Glib::RefPtr<Gtk::Label> label;
    Glib::RefPtr<Gtk::Button> insertBtn;
    Gtk::ComboBoxText *scriptCombo;
    Gtk::ComboBoxText *rangeCombo;
    Inkscape::UI::Widget::FontSelector *fontSelector;

    std::vector<sigc::connection> instanceConns;
};


} // namespace Dialogs
} // namespace UI
} // namespace Inkscape

#endif // SEEN_DIALOGS_GLYPHS_H
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
