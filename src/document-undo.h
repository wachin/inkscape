// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_DOCUMENT_UNDO_H
#define SEEN_SP_DOCUMENT_UNDO_H

#include <glib.h>   // gboolean, gchar

namespace Glib {
    class ustring;
}

class SPDocument;

namespace Inkscape {

class DocumentUndo
{
public:

    /**
     * Set undo sensitivity.
     *
     * Don't use this to temporarily turn sensitivity off, use ScopedInsensitive instead.
    */
    static void setUndoSensitive(SPDocument *doc, bool sensitive);

    static bool getUndoSensitive(SPDocument const *document);

    static void clearUndo(SPDocument *document);

    static void clearRedo(SPDocument *document);

    /* undo_icon is only used in History dialog. */
    static void done(SPDocument *document, Glib::ustring const &event_description, Glib::ustring const &undo_icon);

    static void maybeDone(SPDocument *document, const gchar *keyconst, Glib::ustring const &event_description, Glib::ustring const &undo_icon);

private:
    static void finish_incomplete_transaction(SPDocument &document);

    static void perform_document_update(SPDocument &document);

public:
    static void resetKey(SPDocument *document);

    static void cancel(SPDocument *document);

    static gboolean undo(SPDocument *document);

    static gboolean redo(SPDocument *document);

    /**
     * RAII-style mechanism for creating a temporary undo-insensitive context.
     *
     * \verbatim
        {
            DocumentUndo::ScopedInsensitive tmp(document);
            ... do stuff ...
            // "tmp" goes out of scope here and automatically restores undo-sensitivity
        } \endverbatim
     */
    class ScopedInsensitive {
        SPDocument * m_doc;
        bool m_saved;

      public:
        ScopedInsensitive(SPDocument *doc)
            : m_doc(doc)
        {
            m_saved = getUndoSensitive(doc);
            setUndoSensitive(doc, false);
        }
        ~ScopedInsensitive() { setUndoSensitive(m_doc, m_saved); }
    };
};

} // namespace Inkscape

#endif // SEEN_SP_DOCUMENT_UNDO_H

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
