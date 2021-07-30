// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Widgets used in the stroke style dialog.
 */
/* Author:
 *   Lauris Kaplinski <lauris@ximian.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2010 Jon A. Cruz
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

// WHOA! talk about header bloat!

#ifndef SEEN_DIALOGS_STROKE_STYLE_H
#define SEEN_DIALOGS_STROKE_STYLE_H

#include <glibmm/i18n.h>
#include <gtkmm/grid.h>
#include <gtkmm/radiobutton.h>


#include "desktop-style.h"
#include "desktop.h"
#include "document-undo.h"
#include "fill-style.h" // to get sp_fill_style_widget_set_desktop
#include "gradient-chemistry.h"
#include "inkscape.h"
#include "path-prefix.h"
#include "preferences.h"
#include "selection.h"
#include "style.h"
#include "verbs.h"

#include "display/drawing.h"

#include "helper/stock-items.h"

#include "io/sys.h"

#include "svg/css-ostringstream.h"

#include "ui/cache/svg_preview_cache.h"
#include "ui/dialog-events.h"
#include "ui/icon-names.h"
#include "ui/widget/spinbutton.h"

#include "widgets/spw-utilities.h"

#include "xml/repr.h"

namespace Gtk {
class Widget;
class Container;
}

namespace Inkscape {
    namespace Util {
        class Unit;
    }
    namespace UI {
        namespace Widget {
            class DashSelector;
            class MarkerComboBox;
            class UnitMenu;
        }
    }
}

struct { gchar const *key; gint value; } const SPMarkerNames[] = {
    {"marker-all", SP_MARKER_LOC},
    {"marker-start", SP_MARKER_LOC_START},
    {"marker-mid", SP_MARKER_LOC_MID},
    {"marker-end", SP_MARKER_LOC_END},
    {"", SP_MARKER_LOC_QTY},
    {nullptr, -1}
};

SPObject *getMarkerObj(gchar const *n, SPDocument *doc);

namespace Inkscape {
namespace UI {
namespace Widget {
class StrokeStyleButton;

class StrokeStyle : public Gtk::Box
{
public:
    StrokeStyle();
    ~StrokeStyle() override;
    void setDesktop(SPDesktop *desktop);

private:
    /** List of valid types for the stroke-style radio-button widget */
    enum StrokeStyleButtonType {
        STROKE_STYLE_BUTTON_JOIN, ///< A button to set the line-join style
        STROKE_STYLE_BUTTON_CAP,  ///< A button to set the line-cap style
        STROKE_STYLE_BUTTON_ORDER ///< A button to set the paint-order style
    };
    
    /**
     * A custom radio-button for setting the stroke style.  It can be configured
     * to set either the join or cap style by setting the button_type field.
     */
    class StrokeStyleButton : public Gtk::RadioButton {
        public:
            StrokeStyleButton(Gtk::RadioButtonGroup &grp,
                              char const            *icon,
                              StrokeStyleButtonType  button_type,
                              gchar const           *stroke_style);

            /** Get the type (line/cap) of the stroke-style button */
            inline StrokeStyleButtonType get_button_type() {return button_type;}

            /** Get the stroke style attribute associated with the button */
            inline gchar const * get_stroke_style() {return stroke_style;}

        private:
            StrokeStyleButtonType button_type; ///< The type (line/cap) of the button
            gchar const *stroke_style;         ///< The stroke style associated with the button
    };

    void updateLine();
    void updateAllMarkers(std::vector<SPItem*> const &objects, bool skip_undo = false);
    void setDashSelectorFromStyle(Inkscape::UI::Widget::DashSelector *dsel, SPStyle *style);
    void setJoinType (unsigned const jointype);
    void setCapType (unsigned const captype);
    void setPaintOrder (gchar const *paint_order);
    void setJoinButtons(Gtk::ToggleButton *active);
    void setCapButtons(Gtk::ToggleButton *active);
    void setPaintOrderButtons(Gtk::ToggleButton *active);
    void scaleLine();
    void setScaledDash(SPCSSAttr *css, int ndash, double *dash, double offset, double scale);
    bool isHairlineSelected() const;

    StrokeStyleButton * makeRadioButton(Gtk::RadioButtonGroup &grp,
                                        char const            *icon,
                                        Gtk::Box              *hb,
                                        StrokeStyleButtonType  button_type,
                                        gchar const           *stroke_style);

    // Callback functions
    void selectionModifiedCB(guint flags);
    void selectionChangedCB();
    void widthChangedCB();
    void miterLimitChangedCB();
    void lineDashChangedCB();
    void unitChangedCB();
    bool shouldMarkersBeUpdated();
    static void markerSelectCB(MarkerComboBox *marker_combo, StrokeStyle *spw, SPMarkerLoc const which);
    static void buttonToggledCB(StrokeStyleButton *tb, StrokeStyle *spw);


    MarkerComboBox *startMarkerCombo;
    MarkerComboBox *midMarkerCombo;
    MarkerComboBox *endMarkerCombo;
    Gtk::Grid *table;
    Glib::RefPtr<Gtk::Adjustment> *widthAdj;
    Glib::RefPtr<Gtk::Adjustment> *miterLimitAdj;
    Inkscape::UI::Widget::SpinButton *miterLimitSpin;
    Inkscape::UI::Widget::SpinButton *widthSpin;
    Inkscape::UI::Widget::UnitMenu *unitSelector;
    //Gtk::ToggleButton *hairline;
    StrokeStyleButton *joinMiter;
    StrokeStyleButton *joinRound;
    StrokeStyleButton *joinBevel;
    StrokeStyleButton *capButt;
    StrokeStyleButton *capRound;
    StrokeStyleButton *capSquare;
    StrokeStyleButton *paintOrderFSM;
    StrokeStyleButton *paintOrderSFM;
    StrokeStyleButton *paintOrderFMS;
    StrokeStyleButton *paintOrderMFS;
    StrokeStyleButton *paintOrderSMF;
    StrokeStyleButton *paintOrderMSF;
    Inkscape::UI::Widget::DashSelector *dashSelector;

    gboolean update;
    SPDesktop *desktop;
    sigc::connection selectChangedConn;
    sigc::connection selectModifiedConn;
    sigc::connection startMarkerConn;
    sigc::connection midMarkerConn;
    sigc::connection endMarkerConn;
    sigc::connection unitChangedConn;
    
    Inkscape::Util::Unit const *_old_unit;

    void _handleDocumentReplaced(SPDesktop *, SPDocument *);
    sigc::connection _document_replaced_connection;
};

} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // SEEN_DIALOGS_STROKE_STYLE_H

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
