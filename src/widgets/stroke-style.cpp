// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Bryce Harrington <brycehar@bryceharrington.org>
 *   bulia byak <buliabyak@users.sf.net>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *   Josh Andler <scislac@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2001-2005 authors
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2004 John Cliff
 * Copyright (C) 2008 Maximilian Albert (gtkmm-ification)
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#define noSP_SS_VERBOSE

#include "desktop-widget.h"
#include "stroke-style.h"

#include "object/sp-marker.h"
#include "object/sp-namedview.h"
#include "object/sp-rect.h"
#include "object/sp-stop.h"
#include "object/sp-text.h"

#include "svg/svg-color.h"

#include "ui/icon-loader.h"
#include "ui/widget/dash-selector.h"
#include "ui/widget/unit-menu.h"

#include "widgets/style-utils.h"

using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;

/**
 * Creates a new widget for the line stroke paint.
 */
Gtk::Widget *sp_stroke_style_paint_widget_new()
{
    return Inkscape::Widgets::createStyleWidget( STROKE );
}

/**
 * Creates a new widget for the line stroke style.
 */
Gtk::Widget *sp_stroke_style_line_widget_new()
{
    return Inkscape::Widgets::createStrokeStyleWidget();
}

void sp_stroke_style_widget_set_desktop(Gtk::Widget *widget, SPDesktop *desktop)
{
    Inkscape::StrokeStyle *ss = dynamic_cast<Inkscape::StrokeStyle*>(widget);
    if (ss) {
        ss->setDesktop(desktop);
    }
}


/**
 * Extract the actual name of the link
 * e.g. get mTriangle from url(#mTriangle).
 * \return Buffer containing the actual name, allocated from GLib;
 * the caller should free the buffer when they no longer need it.
 */
SPObject* getMarkerObj(gchar const *n, SPDocument *doc)
{
    gchar const *p = n;
    while (*p != '\0' && *p != '#') {
        p++;
    }

    if (*p == '\0' || p[1] == '\0') {
        return nullptr;
    }

    p++;
    int c = 0;
    while (p[c] != '\0' && p[c] != ')') {
        c++;
    }

    if (p[c] == '\0') {
        return nullptr;
    }

    gchar* b = g_strdup(p);
    b[c] = '\0';

    // FIXME: get the document from the object and let the caller pass it in
    SPObject *marker = doc->getObjectById(b);

    g_free(b);
    return marker;
}

namespace Inkscape {


/**
 * Construct a stroke-style radio button with a given icon
 *
 * \param[in] grp          The Gtk::RadioButtonGroup to which to add the new button
 * \param[in] icon         The icon to use for the button
 * \param[in] button_type  The type of stroke-style radio button (join/cap)
 * \param[in] stroke_style The style attribute to associate with the button
 */
StrokeStyle::StrokeStyleButton::StrokeStyleButton(Gtk::RadioButtonGroup &grp,
                                                  char const            *icon,
                                                  StrokeStyleButtonType  button_type,
                                                  gchar const           *stroke_style)
    : 
        Gtk::RadioButton(grp),
        button_type(button_type),
        stroke_style(stroke_style)
{
    show();
    set_mode(false);

    auto px = Gtk::manage(sp_get_icon_image(icon, Gtk::ICON_SIZE_LARGE_TOOLBAR));
    g_assert(px != nullptr);
    px->show();
    add(*px);
}

/**
 * Create the fill or stroke style widget, and hook up all the signals.
 */
Gtk::Widget *Inkscape::Widgets::createStrokeStyleWidget( )
{
    StrokeStyle *strokeStyle = new StrokeStyle();

    return strokeStyle;
}

StrokeStyle::StrokeStyle() :
    Gtk::Box(),
    miterLimitSpin(),
    widthSpin(),
    unitSelector(),
    joinMiter(),
    joinRound(),
    joinBevel(),
    capButt(),
    capRound(),
    capSquare(),
    dashSelector(),
    update(false),
    desktop(nullptr),
    selectChangedConn(),
    selectModifiedConn(),
    startMarkerConn(),
    midMarkerConn(),
    endMarkerConn(),
    _old_unit(nullptr)
{
    table = new Gtk::Grid();
    table->set_border_width(4);
    table->set_row_spacing(4);
    table->set_hexpand(false);
    table->set_halign(Gtk::ALIGN_CENTER);
    table->show();
    add(*table);

    Gtk::HBox *hb;
    gint i = 0;

    //spw_label(t, C_("Stroke width", "_Width:"), 0, i);

    hb = spw_hbox(table, 3, 1, i);

// TODO: when this is gtkmmified, use an Inkscape::UI::Widget::ScalarUnit instead of the separate
// spinbutton and unit selector for stroke width. In sp_stroke_style_line_update, use
// setHundredPercent to remember the averaged width corresponding to 100%. Then the
// stroke_width_set_unit will be removed (because ScalarUnit takes care of conversions itself), and
// with it, the two remaining calls of stroke_average_width, allowing us to get rid of that
// function in desktop-style.
    widthAdj = new Glib::RefPtr<Gtk::Adjustment>(Gtk::Adjustment::create(1.0, 0.0, 1000.0, 0.1, 10.0, 0.0));
    widthSpin = new Inkscape::UI::Widget::SpinButton(*widthAdj, 0.1, 3);
    widthSpin->set_tooltip_text(_("Stroke width"));
    widthSpin->show();
    spw_label(table, C_("Stroke width", "_Width:"), 0, i, widthSpin);

    sp_dialog_defocus_on_enter_cpp(widthSpin);

    hb->pack_start(*widthSpin, false, false, 0);
    unitSelector = new Inkscape::UI::Widget::UnitMenu();
    unitSelector->setUnitType(Inkscape::Util::UNIT_TYPE_LINEAR);
    Gtk::Widget *us = Gtk::manage(unitSelector);
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;

    unitSelector->addUnit(*unit_table.getUnit("%"));
    _old_unit = unitSelector->getUnit();
    if (desktop) {
        unitSelector->setUnit(desktop->getNamedView()->display_units->abbr);
        _old_unit = desktop->getNamedView()->display_units;
    }
    widthSpin->setUnitMenu(unitSelector);
    unitChangedConn = unitSelector->signal_changed().connect(sigc::mem_fun(*this, &StrokeStyle::unitChangedCB));
    
    us->show();

    hb->pack_start(*us, FALSE, FALSE, 0);
    (*widthAdj)->signal_value_changed().connect(sigc::mem_fun(*this, &StrokeStyle::widthChangedCB));
    i++;

    /* Dash */
    spw_label(table, _("Dashes:"), 0, i, nullptr); //no mnemonic for now
                                            //decide what to do:
                                            //   implement a set_mnemonic_source function in the
                                            //   Inkscape::UI::Widget::DashSelector class, so that we do not have to
                                            //   expose any of the underlying widgets?
    dashSelector = Gtk::manage(new Inkscape::UI::Widget::DashSelector);

    dashSelector->show();
    dashSelector->set_hexpand();
    dashSelector->set_halign(Gtk::ALIGN_FILL);
    dashSelector->set_valign(Gtk::ALIGN_CENTER);
    table->attach(*dashSelector, 1, i, 3, 1);
    dashSelector->changed_signal.connect(sigc::mem_fun(*this, &StrokeStyle::lineDashChangedCB));

    i++;

    /* Drop down marker selectors*/
    // TRANSLATORS: Path markers are an SVG feature that allows you to attach arbitrary shapes
    // (arrowheads, bullets, faces, whatever) to the start, end, or middle nodes of a path.

    spw_label(table, _("Markers:"), 0, i, nullptr);

    hb = spw_hbox(table, 1, 1, i);
    i++;

    startMarkerCombo = Gtk::manage(new MarkerComboBox("marker-start", SP_MARKER_LOC_START));
    startMarkerCombo->set_tooltip_text(_("Start Markers are drawn on the first node of a path or shape"));
    startMarkerConn = startMarkerCombo->signal_changed().connect(
            sigc::bind<MarkerComboBox *, StrokeStyle *, SPMarkerLoc>(
                sigc::ptr_fun(&StrokeStyle::markerSelectCB), startMarkerCombo, this, SP_MARKER_LOC_START));
    startMarkerCombo->show();

    hb->pack_start(*startMarkerCombo, true, true, 0);

    midMarkerCombo = Gtk::manage(new MarkerComboBox("marker-mid", SP_MARKER_LOC_MID));
    midMarkerCombo->set_tooltip_text(_("Mid Markers are drawn on every node of a path or shape except the first and last nodes"));
    midMarkerConn = midMarkerCombo->signal_changed().connect(
        sigc::bind<MarkerComboBox *, StrokeStyle *, SPMarkerLoc>(
            sigc::ptr_fun(&StrokeStyle::markerSelectCB), midMarkerCombo, this, SP_MARKER_LOC_MID));
    midMarkerCombo->show();

    hb->pack_start(*midMarkerCombo, true, true, 0);

    endMarkerCombo = Gtk::manage(new MarkerComboBox("marker-end", SP_MARKER_LOC_END));
    endMarkerCombo->set_tooltip_text(_("End Markers are drawn on the last node of a path or shape"));
    endMarkerConn = endMarkerCombo->signal_changed().connect(
        sigc::bind<MarkerComboBox *, StrokeStyle *, SPMarkerLoc>(
            sigc::ptr_fun(&StrokeStyle::markerSelectCB), endMarkerCombo, this, SP_MARKER_LOC_END));
    endMarkerCombo->show();

    hb->pack_start(*endMarkerCombo, true, true, 0);

    i++;

    /* Join type */
    // TRANSLATORS: The line join style specifies the shape to be used at the
    //  corners of paths. It can be "miter", "round" or "bevel".
    spw_label(table, _("Join:"), 0, i, nullptr);

    hb = spw_hbox(table, 3, 1, i);

    Gtk::RadioButtonGroup joinGrp;

    joinRound = makeRadioButton(joinGrp, INKSCAPE_ICON("stroke-join-round"),
                                hb, STROKE_STYLE_BUTTON_JOIN, "round");

    // TRANSLATORS: Round join: joining lines with a rounded corner.
    //  For an example, draw a triangle with a large stroke width and modify the
    //  "Join" option (in the Fill and Stroke dialog).
    joinRound->set_tooltip_text(_("Round join"));

    joinBevel = makeRadioButton(joinGrp, INKSCAPE_ICON("stroke-join-bevel"),
                                hb, STROKE_STYLE_BUTTON_JOIN, "bevel");

    // TRANSLATORS: Bevel join: joining lines with a blunted (flattened) corner.
    //  For an example, draw a triangle with a large stroke width and modify the
    //  "Join" option (in the Fill and Stroke dialog).
    joinBevel->set_tooltip_text(_("Bevel join"));

    joinMiter = makeRadioButton(joinGrp, INKSCAPE_ICON("stroke-join-miter"),
                                hb, STROKE_STYLE_BUTTON_JOIN, "miter");

    // TRANSLATORS: Miter join: joining lines with a sharp (pointed) corner.
    //  For an example, draw a triangle with a large stroke width and modify the
    //  "Join" option (in the Fill and Stroke dialog).
    joinMiter->set_tooltip_text(_("Miter join"));

    /* Miterlimit  */
    // TRANSLATORS: Miter limit: only for "miter join", this limits the length
    //  of the sharp "spike" when the lines connect at too sharp an angle.
    // When two line segments meet at a sharp angle, a miter join results in a
    //  spike that extends well beyond the connection point. The purpose of the
    //  miter limit is to cut off such spikes (i.e. convert them into bevels)
    //  when they become too long.
    //spw_label(t, _("Miter _limit:"), 0, i);
    miterLimitAdj = new Glib::RefPtr<Gtk::Adjustment>(Gtk::Adjustment::create(4.0, 0.0, 100.0, 0.1, 10.0, 0.0));
    miterLimitSpin = new Inkscape::UI::Widget::SpinButton(*miterLimitAdj, 0.1, 2);
    miterLimitSpin->set_tooltip_text(_("Maximum length of the miter (in units of stroke width)"));
    miterLimitSpin->show();
    sp_dialog_defocus_on_enter_cpp(miterLimitSpin);

    hb->pack_start(*miterLimitSpin, false, false, 0);
    (*miterLimitAdj)->signal_value_changed().connect(sigc::mem_fun(*this, &StrokeStyle::miterLimitChangedCB));
    i++;

    /* Cap type */
    // TRANSLATORS: cap type specifies the shape for the ends of lines
    //spw_label(t, _("_Cap:"), 0, i);
    spw_label(table, _("Cap:"), 0, i, nullptr);

    hb = spw_hbox(table, 3, 1, i);

    Gtk::RadioButtonGroup capGrp;

    capButt = makeRadioButton(capGrp, INKSCAPE_ICON("stroke-cap-butt"),
                                hb, STROKE_STYLE_BUTTON_CAP, "butt");

    // TRANSLATORS: Butt cap: the line shape does not extend beyond the end point
    //  of the line; the ends of the line are square
    capButt->set_tooltip_text(_("Butt cap"));

    capRound = makeRadioButton(capGrp, INKSCAPE_ICON("stroke-cap-round"),
                                hb, STROKE_STYLE_BUTTON_CAP, "round");

    // TRANSLATORS: Round cap: the line shape extends beyond the end point of the
    //  line; the ends of the line are rounded
    capRound->set_tooltip_text(_("Round cap"));

    capSquare = makeRadioButton(capGrp, INKSCAPE_ICON("stroke-cap-square"),
                                hb, STROKE_STYLE_BUTTON_CAP, "square");

    // TRANSLATORS: Square cap: the line shape extends beyond the end point of the
    //  line; the ends of the line are square
    capSquare->set_tooltip_text(_("Square cap"));

    i++;

    /* Paint order */
    // TRANSLATORS: Paint order determines the order the 'fill', 'stroke', and 'markers are painted.
    spw_label(table, _("Order:"), 0, i, nullptr);

    hb = spw_hbox(table, 4, 1, i);

    Gtk::RadioButtonGroup paintOrderGrp;

    paintOrderFSM = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-fsm"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "normal");
    paintOrderFSM->set_tooltip_text(_("Fill, Stroke, Markers")); 

    paintOrderSFM = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-sfm"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "stroke fill markers");
    paintOrderSFM->set_tooltip_text(_("Stroke, Fill, Markers")); 

    paintOrderFMS = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-fms"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "fill markers stroke");
    paintOrderFMS->set_tooltip_text(_("Fill, Markers, Stroke")); 

    i++;

    hb = spw_hbox(table, 4, 1, i);

    paintOrderMFS = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-mfs"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "markers fill stroke");
    paintOrderMFS->set_tooltip_text(_("Markers, Fill, Stroke")); 

    paintOrderSMF = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-smf"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "stroke markers fill");
    paintOrderSMF->set_tooltip_text(_("Stroke, Markers, Fill")); 

    paintOrderMSF = makeRadioButton(paintOrderGrp, INKSCAPE_ICON("paint-order-msf"),
                                    hb, STROKE_STYLE_BUTTON_ORDER, "markers stroke fill");
    paintOrderMSF->set_tooltip_text(_("Markers, Stroke, Fill")); 

    i++;
}

StrokeStyle::~StrokeStyle()
{
    selectModifiedConn.disconnect();
    selectChangedConn.disconnect();
}

void StrokeStyle::setDesktop(SPDesktop *desktop)
{
    if (this->desktop != desktop) {

        if (this->desktop) {
            selectModifiedConn.disconnect();
            selectChangedConn.disconnect();
            _document_replaced_connection.disconnect();
        }
        this->desktop = desktop;

        if (!desktop) {
            return;
        }

        if (desktop->selection) {
            selectChangedConn = desktop->selection->connectChanged(sigc::hide(sigc::mem_fun(*this, &StrokeStyle::selectionChangedCB)));
            selectModifiedConn = desktop->selection->connectModified(sigc::hide<0>(sigc::mem_fun(*this, &StrokeStyle::selectionModifiedCB)));
        }

        _document_replaced_connection =
            desktop->connectDocumentReplaced(sigc::mem_fun(this, &StrokeStyle::_handleDocumentReplaced));

        _handleDocumentReplaced(nullptr, desktop->getDocument());

        updateLine();
    }
}

void StrokeStyle::_handleDocumentReplaced(SPDesktop *, SPDocument *document)
{
    for (MarkerComboBox *combo : { startMarkerCombo, midMarkerCombo, endMarkerCombo }) {
        combo->setDocument(document);
    }
}


/**
 * Helper function for creating stroke-style radio buttons.
 *
 * \param[in] grp           The Gtk::RadioButtonGroup in which to add the button
 * \param[in] icon          The icon for the button
 * \param[in] hb            The Gtk::Box container in which to add the button
 * \param[in] button_type   The type (join/cap) for the button
 * \param[in] stroke_style  The style attribute to associate with the button
 *
 * \details After instantiating the button, it is added to a container box and
 *          a handler for the toggle event is connected.
 */
StrokeStyle::StrokeStyleButton *
StrokeStyle::makeRadioButton(Gtk::RadioButtonGroup &grp,
                             char const            *icon,
                             Gtk::HBox             *hb,
                             StrokeStyleButtonType  button_type,
                             gchar const           *stroke_style)
{
    g_assert(icon != nullptr);
    g_assert(hb  != nullptr);

    StrokeStyleButton *tb = new StrokeStyleButton(grp, icon, button_type, stroke_style);

    hb->pack_start(*tb, false, false, 0);
    set_data(icon, tb);

    tb->signal_toggled().connect(sigc::bind<StrokeStyleButton *, StrokeStyle *>(
                                     sigc::ptr_fun(&StrokeStyle::buttonToggledCB), tb, this));

    return tb;
}

bool StrokeStyle::shouldMarkersBeUpdated()
{
    return startMarkerCombo->update() || midMarkerCombo->update() ||
                          endMarkerCombo->update();
}

/**
 * Handles when user selects one of the markers from the marker combobox.
 * Gets the marker uri string and applies it to all selected
 * items in the current desktop.
 */
void StrokeStyle::markerSelectCB(MarkerComboBox *marker_combo, StrokeStyle *spw, SPMarkerLoc const /*which*/)
{
    if (spw->update || spw->shouldMarkersBeUpdated()) {
        return;
    }

    spw->update = true;

    SPDocument *document = spw->desktop->getDocument();
    if (!document) {
        return;
    }

    /* Get Marker */
    gchar const *marker = marker_combo->get_active_marker_uri();


    SPCSSAttr *css = sp_repr_css_attr_new();
    gchar const *combo_id = marker_combo->get_id();
    sp_repr_css_set_property(css, combo_id, marker);

    // Also update the marker combobox, so the document's markers
    // show up at the top of the combobox
//    sp_stroke_style_line_update( SP_WIDGET(spw), desktop ? desktop->getSelection() : NULL);
    //spw->updateMarkerHist(which);

    Inkscape::Selection *selection = spw->desktop->getSelection();
    auto itemlist= selection->items();
    for(auto i=itemlist.begin();i!=itemlist.end();++i){
        SPItem *item = *i;
        if (!SP_IS_SHAPE(item) || SP_IS_RECT(item)) { // can't set marker to rect, until it's converted to using <path>
            continue;
        }
        Inkscape::XML::Node *selrepr = item->getRepr();
        if (selrepr) {
            sp_repr_css_change_recursive(selrepr, css, "style");
            SPObject *markerObj = getMarkerObj(marker, document);
            spw->setMarkerColor(markerObj, marker_combo->get_loc(), item);
        }

        item->requestModified(SP_OBJECT_MODIFIED_FLAG);
        item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);

        DocumentUndo::done(document, SP_VERB_DIALOG_FILL_STROKE, _("Set markers"));
    }

    sp_repr_css_attr_unref(css);
    css = nullptr;

    spw->update = false;
};

void StrokeStyle::updateMarkerHist(SPMarkerLoc const which)
{
    switch (which) {
        case SP_MARKER_LOC_START:
            startMarkerConn.block();
            startMarkerCombo->set_active_history();
            startMarkerConn.unblock();
            break;

        case SP_MARKER_LOC_MID:
            midMarkerConn.block();
            midMarkerCombo->set_active_history();
            midMarkerConn.unblock();
            break;

        case SP_MARKER_LOC_END:
            endMarkerConn.block();
            endMarkerCombo->set_active_history();
            endMarkerConn.unblock();
            break;
        default:
            g_assert_not_reached();
    }
}

/**
 * Callback for when UnitMenu widget is modified.
 * Triggers update action.
 */
void StrokeStyle::unitChangedCB()
{
    Inkscape::Util::Unit const *new_unit = unitSelector->getUnit();
    if (new_unit->type == Inkscape::Util::UNIT_TYPE_DIMENSIONLESS) {
        widthSpin->set_value(100);
    }
    widthSpin->set_value(Inkscape::Util::Quantity::convert(widthSpin->get_value(), _old_unit, new_unit));
    _old_unit = new_unit;
}

/**
 * Callback for when stroke style widget is modified.
 * Triggers update action.
 */
void
StrokeStyle::selectionModifiedCB(guint flags)
{
    // We care deeply about only updating when the style is updated
    // if we update on other flags, we slow inkscape down when dragging
    if (flags & (SP_OBJECT_STYLE_MODIFIED_FLAG)) {
        updateLine();
    }
}

/**
 * Callback for when stroke style widget is changed.
 * Triggers update action.
 */
void
StrokeStyle::selectionChangedCB()
{
    updateLine();
}

/*
 * Fork marker if necessary and set the referencing items url to the new marker
 * Return the new marker
 */
SPObject *
StrokeStyle::forkMarker(SPObject *marker, int loc, SPItem *item)
{
    if (!item || !marker) {
        return nullptr;
    }

    gchar const *marker_id = SPMarkerNames[loc].key;

    /*
     * Optimization when all the references to this marker are from this item
     * then we can reuse it and don't need to fork
     */
    Glib::ustring urlId = Glib::ustring::format("url(#", marker->getRepr()->attribute("id"), ")");
    unsigned int refs = 0;
    for (int i = SP_MARKER_LOC_START; i < SP_MARKER_LOC_QTY; i++) {
        if (item->style->marker_ptrs[i]->set &&
            !strcmp(urlId.c_str(), item->style->marker_ptrs[i]->value())) {
            refs++;
        }
    }
    if (marker->hrefcount <= refs) {
        return marker;
    }

    marker = sp_marker_fork_if_necessary(marker);

    // Update the items url to new marker
    Inkscape::XML::Node *mark_repr = marker->getRepr();
    SPCSSAttr *css_item = sp_repr_css_attr_new();
    sp_repr_css_set_property(css_item, marker_id, g_strconcat("url(#", mark_repr->attribute("id"), ")", NULL));
    sp_repr_css_change_recursive(item->getRepr(), css_item, "style");

    sp_repr_css_attr_unref(css_item);
    css_item = nullptr;

    return marker;
}

/**
 * Change the color of the marker to match the color of the item.
 * Marker stroke color is set to item stroke color.
 * Fill color :
 * 1. If the item has fill, use that for the marker fill,
 * 2. If the marker has same fill and stroke assume its solid, use item stroke for both fill and stroke the line stroke
 * 3. If the marker has fill color, use the marker fill color
 *
 */
void
StrokeStyle::setMarkerColor(SPObject *marker, int loc, SPItem *item)
{

    if (!item || !marker) {
        return;
    }

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gboolean colorStock = prefs->getBool("/options/markers/colorStockMarkers", true);
    gboolean colorCustom = prefs->getBool("/options/markers/colorCustomMarkers", false);
    const gchar *stock = marker->getRepr()->attribute("inkscape:isstock");
    gboolean isStock = (stock && !strcmp(stock,"true"));

    if (isStock ? !colorStock : !colorCustom) {
        return;
    }

    // Check if we need to fork this marker
    marker = forkMarker(marker, loc, item);

    Inkscape::XML::Node *repr = marker->getRepr()->firstChild();
    if (!repr) {
        return;
    };

    // Current line style
    SPCSSAttr *css_item = sp_css_attr_from_object(item, SP_STYLE_FLAG_ALWAYS);
    const char *lstroke = getItemColorForMarker(item, FOR_STROKE, loc);
    const char *lstroke_opacity = sp_repr_css_property(css_item, "stroke-opacity", "1");
    const char *lfill = getItemColorForMarker(item, FOR_FILL, loc);
    const char *lfill_opacity = sp_repr_css_property(css_item, "fill-opacity", "1");

    // Current marker style
    SPCSSAttr *css_marker = sp_css_attr_from_object(marker->firstChild(), SP_STYLE_FLAG_ALWAYS);
    const char *mfill = sp_repr_css_property(css_marker, "fill", "none");
    const char *mstroke = sp_repr_css_property(css_marker, "fill", "none");

    // Create new marker style with the lines stroke
    SPCSSAttr *css = sp_repr_css_attr_new();

    sp_repr_css_set_property(css, "stroke", lstroke);
    sp_repr_css_set_property(css, "stroke-opacity", lstroke_opacity);

    if (strcmp(lfill, "none") ) {
        // 1. If the line has fill, use that for the marker fill
        sp_repr_css_set_property(css, "fill", lfill);
        sp_repr_css_set_property(css, "fill-opacity", lfill_opacity);
    }
    else if (mfill && mstroke && !strcmp(mfill, mstroke) && mfill[0] == '#' && strcmp(mfill, "#ffffff")) {
        // 2. If the marker has same fill and stroke assume its solid. use line stroke for both fill and stroke the line stroke
        sp_repr_css_set_property(css, "fill", lstroke);
        sp_repr_css_set_property(css, "fill-opacity", lstroke_opacity);
    }
    else if (mfill && mfill[0] == '#' && strcmp(mfill, "#000000")) {
        // 3. If the marker has fill color, use the marker fill color
        sp_repr_css_set_property(css, "fill", mfill);
        //sp_repr_css_set_property(css, "fill-opacity", mfill_opacity);
    }

    sp_repr_css_change_recursive(marker->firstChild()->getRepr(), css, "style");

    // Tell the combos to update its image cache of this marker
    gchar const *mid = marker->getRepr()->attribute("id");
    startMarkerCombo->update_marker_image(mid);
    midMarkerCombo->update_marker_image(mid);
    endMarkerCombo->update_marker_image(mid);

    sp_repr_css_attr_unref(css);
    css = nullptr;


}

/*
 * Get the fill or stroke color of the item
 * If its a gradient, then return first or last stop color
 */
const char *
StrokeStyle::getItemColorForMarker(SPItem *item, Inkscape::PaintTarget fill_or_stroke, int loc)
{
    SPCSSAttr *css_item = sp_css_attr_from_object(item, SP_STYLE_FLAG_ALWAYS);
    const char *color;
    if (fill_or_stroke == FOR_FILL)
        color = sp_repr_css_property(css_item, "fill", "none");
    else
        color = sp_repr_css_property(css_item, "stroke", "none");

    if (!strncmp (color, "url(", 4)) {
        // If the item has a gradient use the first stop color for the marker

        SPGradient *grad = getGradient(item, fill_or_stroke);
        if (grad) {
            SPGradient *vector = grad->getVector(FALSE);
            SPStop *stop = vector->getFirstStop();
            if (loc == SP_MARKER_LOC_END) {
                stop = sp_last_stop(vector);
            }
            if (stop) {
                guint32 const c1 = stop->get_rgba32();
                gchar c[64];
                sp_svg_write_color(c, sizeof(c), c1);
                color = g_strdup(c);
                //lstroke_opacity = Glib::ustring::format(stop->opacity).c_str();
            }
        }
    }
    return color;
}
/**
 * Sets selector widgets' dash style from an SPStyle object.
 */
void
StrokeStyle::setDashSelectorFromStyle(Inkscape::UI::Widget::DashSelector *dsel, SPStyle *style)
{
    if (!style->stroke_dasharray.values.empty()) {
        double d[64];
        size_t len = MIN(style->stroke_dasharray.values.size(), 64);
        /* Set dash */
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        gboolean scale = prefs->getBool("/options/dash/scale", true);
        double scaledash = 1.0;
        if (scale) {
            scaledash = style->stroke_width.computed;
        }
        for (unsigned i = 0; i < len; i++) {
            if (style->stroke_width.computed != 0)
                d[i] = style->stroke_dasharray.values[i].value / scaledash;
            else
                d[i] = style->stroke_dasharray.values[i].value; // is there a better thing to do for stroke_width==0?
        }
        dsel->set_dash(len, d,
                       style->stroke_width.computed != 0 ? style->stroke_dashoffset.value / scaledash
                                                         : style->stroke_dashoffset.value);
    } else {
        dsel->set_dash(0, nullptr, 0.0);
    }
}

/**
 * Sets the join type for a line, and updates the stroke style widget's buttons
 */
void
StrokeStyle::setJoinType (unsigned const jointype)
{
    Gtk::RadioButton *tb = nullptr;
    switch (jointype) {
        case SP_STROKE_LINEJOIN_MITER:
            tb = joinMiter;
            break;
        case SP_STROKE_LINEJOIN_ROUND:
            tb = joinRound;
            break;
        case SP_STROKE_LINEJOIN_BEVEL:
            tb = joinBevel;
            break;
        default:
            // Should not happen
            std::cerr << "StrokeStyle::setJoinType(): Invalid value: " << jointype << std::endl;
            tb = joinMiter;
            break;
    }
    setJoinButtons(tb);
}

/**
 * Sets the cap type for a line, and updates the stroke style widget's buttons
 */
void
StrokeStyle::setCapType (unsigned const captype)
{
    Gtk::RadioButton *tb = nullptr;
    switch (captype) {
        case SP_STROKE_LINECAP_BUTT:
            tb = capButt;
            break;
        case SP_STROKE_LINECAP_ROUND:
            tb = capRound;
            break;
        case SP_STROKE_LINECAP_SQUARE:
            tb = capSquare;
            break;
        default:
            // Should not happen
            std::cerr << "StrokeStyle::setCapType(): Invalid value: " << captype << std::endl;
            tb = capButt;
            break;
    }
    setCapButtons(tb);
}

/**
 * Sets the cap type for a line, and updates the stroke style widget's buttons
 */
void
StrokeStyle::setPaintOrder (gchar const *paint_order)
{
    Gtk::RadioButton *tb = paintOrderFSM;

    SPIPaintOrder temp;
    temp.read( paint_order );

    if (temp.layer[0] != SP_CSS_PAINT_ORDER_NORMAL) {

        if (temp.layer[0] == SP_CSS_PAINT_ORDER_FILL) {
            if (temp.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                tb = paintOrderFSM;
            } else {
                tb = paintOrderFMS;
            }
        } else if (temp.layer[0] == SP_CSS_PAINT_ORDER_STROKE) {
            if (temp.layer[1] == SP_CSS_PAINT_ORDER_FILL) {
                tb = paintOrderSFM;
            } else {
                tb = paintOrderSMF;
            }
        } else {
            if (temp.layer[1] == SP_CSS_PAINT_ORDER_STROKE) {
                tb = paintOrderMSF;
            } else {
                tb = paintOrderMFS;
            }
        }

    }
    setPaintOrderButtons(tb);
}

/**
 * Callback for when stroke style widget is updated, including markers, cap type,
 * join type, etc.
 */
void
StrokeStyle::updateLine()
{
    if (update) {
        return;
    }

    update = true;

    Inkscape::Selection *sel = desktop ? desktop->getSelection() : nullptr;

    FillOrStroke kind = GPOINTER_TO_INT(get_data("kind")) ? FILL : STROKE;

    // create temporary style
    SPStyle query(SP_ACTIVE_DOCUMENT);
    // query into it
    int result_sw = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_STROKEWIDTH);
    int result_ml = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_STROKEMITERLIMIT);
    int result_cap = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_STROKECAP);
    int result_join = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_STROKEJOIN);

    int result_order = sp_desktop_query_style (SP_ACTIVE_DESKTOP, &query, QUERY_STYLE_PROPERTY_PAINTORDER);

    SPIPaint &targPaint = *query.getFillOrStroke(kind == FILL);

    if (!sel || sel->isEmpty()) {
        // Nothing selected, grey-out all controls in the stroke-style dialog
        table->set_sensitive(false);

        update = false;

        return;
    } else {
        table->set_sensitive(true);

        if (result_sw == QUERY_STYLE_MULTIPLE_AVERAGED) {
            unitSelector->setUnit("%");
        } else {
            // same width, or only one object; no sense to keep percent, switch to absolute
            Inkscape::Util::Unit const *tempunit = unitSelector->getUnit();
            if (tempunit->type != Inkscape::Util::UNIT_TYPE_LINEAR) {
                unitSelector->setUnit(SP_ACTIVE_DESKTOP->getNamedView()->display_units->abbr);
            }
        }

        Inkscape::Util::Unit const *unit = unitSelector->getUnit();

        if (unit->type == Inkscape::Util::UNIT_TYPE_LINEAR) {
            double avgwidth = Inkscape::Util::Quantity::convert(query.stroke_width.computed, "px", unit);
            (*widthAdj)->set_value(avgwidth);
        } else {
            (*widthAdj)->set_value(100);
        }

        // if none of the selected objects has a stroke, than quite some controls should be disabled
        // The markers might still be shown though, so these will not be disabled
        bool enabled = (result_sw != QUERY_STYLE_NOTHING) && !targPaint.isNoneSet();
        /* No objects stroked, set insensitive */
        joinMiter->set_sensitive(enabled);
        joinRound->set_sensitive(enabled);
        joinBevel->set_sensitive(enabled);

        miterLimitSpin->set_sensitive(enabled);

        capButt->set_sensitive(enabled);
        capRound->set_sensitive(enabled);
        capSquare->set_sensitive(enabled);

        dashSelector->set_sensitive(enabled);
    }

    if (result_ml != QUERY_STYLE_NOTHING)
        (*miterLimitAdj)->set_value(query.stroke_miterlimit.value); // TODO: reflect averagedness?

    using Inkscape::is_query_style_updateable;
    if (! is_query_style_updateable(result_join)) {
        setJoinType(query.stroke_linejoin.value);
    } else {
        setJoinButtons(nullptr);
    }

    if (! is_query_style_updateable(result_cap)) {
        setCapType (query.stroke_linecap.value);
    } else {
        setCapButtons(nullptr);
    }

    if (! is_query_style_updateable(result_order)) {
        setPaintOrder (query.paint_order.value);
    } else {
        setPaintOrder (nullptr);
    }

    std::vector<SPItem*> const objects(sel->items().begin(), sel->items().end());
    if (objects.size()) {
        SPObject *const object = objects[0];
        SPStyle *const style = object->style;
        /* Markers */
        updateAllMarkers(objects, true); // FIXME: make this desktop query too

        /* Dash */
        setDashSelectorFromStyle(dashSelector, style); // FIXME: make this desktop query too
    }
    table->set_sensitive(true);

    update = false;
}

/**
 * Sets a line's dash properties in a CSS style object.
 */
void
StrokeStyle::setScaledDash(SPCSSAttr *css,
                                int ndash, double *dash, double offset,
                                double scale)
{
    if (ndash > 0) {
        Inkscape::CSSOStringStream osarray;
        for (int i = 0; i < ndash; i++) {
            osarray << dash[i] * scale;
            if (i < (ndash - 1)) {
                osarray << ",";
            }
        }
        sp_repr_css_set_property(css, "stroke-dasharray", osarray.str().c_str());

        Inkscape::CSSOStringStream osoffset;
        osoffset << offset * scale;
        sp_repr_css_set_property(css, "stroke-dashoffset", osoffset.str().c_str());
    } else {
        sp_repr_css_set_property(css, "stroke-dasharray", "none");
        sp_repr_css_set_property(css, "stroke-dashoffset", nullptr);
    }
}

static inline double calcScaleLineWidth(const double width_typed, SPItem *const item, Inkscape::Util::Unit const *const unit)
{
    if (unit->type == Inkscape::Util::UNIT_TYPE_LINEAR) {
        return Inkscape::Util::Quantity::convert(width_typed, unit, "px");
    } else { // percentage
        const gdouble old_w = item->style->stroke_width.computed;
        return old_w * width_typed / 100;
    }
}

/**
 * Sets line properties like width, dashes, markers, etc. on all currently selected items.
 */
void
StrokeStyle::scaleLine()
{
    if (!desktop) {
        return;
    }

    if (update) {
        return;
    }

    update = true;
    
    SPDocument *document = desktop->getDocument();
    Inkscape::Selection *selection = desktop->getSelection();
    auto items= selection->items();

    /* TODO: Create some standardized method */
    SPCSSAttr *css = sp_repr_css_attr_new();

    if (!items.empty()) {
        double width_typed = (*widthAdj)->get_value();
        double const miterlimit = (*miterLimitAdj)->get_value();

        Inkscape::Util::Unit const *const unit = unitSelector->getUnit();

        double *dash, offset;
        int ndash;
        dashSelector->get_dash(&ndash, &dash, &offset);

        for(auto i=items.begin();i!=items.end();++i){
            /* Set stroke width */
            const double width = calcScaleLineWidth(width_typed, (*i), unit);

            {
                Inkscape::CSSOStringStream os_width;
                os_width << width;
                sp_repr_css_set_property(css, "stroke-width", os_width.str().c_str());
            }

            {
                Inkscape::CSSOStringStream os_ml;
                os_ml << miterlimit;
                sp_repr_css_set_property(css, "stroke-miterlimit", os_ml.str().c_str());
            }

            /* Set dash */
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            gboolean scale = prefs->getBool("/options/dash/scale", true);
            if (scale) {
                setScaledDash(css, ndash, dash, offset, width);
            }
            else {
                setScaledDash(css, ndash, dash, offset, document->getDocumentScale()[0]);
            }
            sp_desktop_apply_css_recursive ((*i), css, true);
        }

        g_free(dash);

        if (unit->type != Inkscape::Util::UNIT_TYPE_LINEAR) {
            // reset to 100 percent
            (*widthAdj)->set_value(100.0);
        }

    }

    // we have already changed the items, so set style without changing selection
    // FIXME: move the above stroke-setting stuff, including percentages, to desktop-style
    sp_desktop_set_style (desktop, css, false);

    sp_repr_css_attr_unref(css);
    css = nullptr;

    DocumentUndo::done(document, SP_VERB_DIALOG_FILL_STROKE,
                       _("Set stroke style"));

    update = false;
}

/**
 * Callback for when the stroke style's width changes.
 * Causes all line styles to be applied to all selected items.
 */
void
StrokeStyle::widthChangedCB()
{
    if (update) {
        return;
    }

    scaleLine();
}

/**
 * Callback for when the stroke style's miterlimit changes.
 * Causes all line styles to be applied to all selected items.
 */
void
StrokeStyle::miterLimitChangedCB()
{
    if (update) {
        return;
    }

    scaleLine();
}

/**
 * Callback for when the stroke style's dash changes.
 * Causes all line styles to be applied to all selected items.
 */

void
StrokeStyle::lineDashChangedCB()
{
    if (update) {
        return;
    }

    scaleLine();
}

/**
 * This routine handles toggle events for buttons in the stroke style dialog.
 *
 * When activated, this routine gets the data for the various widgets, and then
 * calls the respective routines to update css properties, etc.
 *
 */
void StrokeStyle::buttonToggledCB(StrokeStyleButton *tb, StrokeStyle *spw)
{
    if (spw->update) {
        return;
    }

    if (tb->get_active()) {
        if (tb->get_button_type() == STROKE_STYLE_BUTTON_JOIN) {
            spw->miterLimitSpin->set_sensitive(!strcmp(tb->get_stroke_style(), "miter"));
        }

        /* TODO: Create some standardized method */
        SPCSSAttr *css = sp_repr_css_attr_new();

        switch (tb->get_button_type()) {
            case STROKE_STYLE_BUTTON_JOIN: 
                sp_repr_css_set_property(css, "stroke-linejoin", tb->get_stroke_style());
                sp_desktop_set_style (spw->desktop, css);
                spw->setJoinButtons(tb);
                break;
            case STROKE_STYLE_BUTTON_CAP:
                sp_repr_css_set_property(css, "stroke-linecap", tb->get_stroke_style());
                sp_desktop_set_style (spw->desktop, css);
                spw->setCapButtons(tb);
                break;
            case STROKE_STYLE_BUTTON_ORDER:
                sp_repr_css_set_property(css, "paint-order", tb->get_stroke_style());
                sp_desktop_set_style (spw->desktop, css);
                //spw->setPaintButtons(tb);
        }

        sp_repr_css_attr_unref(css);
        css = nullptr;

        DocumentUndo::done(spw->desktop->getDocument(), SP_VERB_DIALOG_FILL_STROKE, _("Set stroke style"));
    }
}

/**
 * Updates the join style toggle buttons
 */
void
StrokeStyle::setJoinButtons(Gtk::ToggleButton *active)
{
    joinMiter->set_active(active == joinMiter);
    miterLimitSpin->set_sensitive(active == joinMiter);
    joinRound->set_active(active == joinRound);
    joinBevel->set_active(active == joinBevel);
}

/**
 * Updates the cap style toggle buttons
 */
void
StrokeStyle::setCapButtons(Gtk::ToggleButton *active)
{
    capButt->set_active(active == capButt);
    capRound->set_active(active == capRound);
    capSquare->set_active(active == capSquare);
}


/**
 * Updates the paint order style toggle buttons
 */
void
StrokeStyle::setPaintOrderButtons(Gtk::ToggleButton *active)
{
    paintOrderFSM->set_active(active == paintOrderFSM);
    paintOrderSFM->set_active(active == paintOrderSFM);
    paintOrderFMS->set_active(active == paintOrderFMS);
    paintOrderMFS->set_active(active == paintOrderMFS);
    paintOrderSMF->set_active(active == paintOrderSMF);
    paintOrderMSF->set_active(active == paintOrderMSF);
}


/**
 * Recursively builds a simple list from an arbitrarily complex selection
 * of items and grouped items
 */
static void buildGroupedItemList(SPObject *element, std::vector<SPObject*> &simple_list)
{
    if (SP_IS_GROUP(element)) {
        for (SPObject *i = element->firstChild(); i; i = i->getNext()) {
            buildGroupedItemList(i, simple_list);
        }
    } else {
        simple_list.push_back(element);
    }
}


/**
 * Updates the marker combobox to highlight the appropriate marker and scroll to
 * that marker.
 */
void
StrokeStyle::updateAllMarkers(std::vector<SPItem*> const &objects, bool skip_undo)
{
    struct { MarkerComboBox *key; int loc; } const keyloc[] = {
            { startMarkerCombo, SP_MARKER_LOC_START },
            { midMarkerCombo, SP_MARKER_LOC_MID },
            { endMarkerCombo, SP_MARKER_LOC_END }
    };

    bool all_texts = true;

    auto simplified_list = std::vector<SPObject *>();
    for (SPItem *item : objects) {
        buildGroupedItemList(item, simplified_list);
    }

    for (SPObject *object : simplified_list) {
        if (!SP_IS_TEXT(object)) {
            all_texts = false;
            break;
        }
    }

    // We show markers of the last object in the list only
    // FIXME: use the first in the list that has the marker of each type, if any

    // -1 means prefs haven't been queried yet
    int update = -1;

    for (auto const &markertype : keyloc) {
        // For all three marker types,

        // find the corresponding combobox item
        MarkerComboBox *combo = markertype.key;

        // Quit if we're in update state
        if (combo->update()) {
            return;
        }

        // Per SVG spec, text objects cannot have markers; disable combobox if only texts are selected
        combo->set_sensitive(!all_texts);

        SPObject *marker = nullptr;

        if (!all_texts) {
            for (SPObject *object : simplified_list) {
                char const *value = object->style->marker_ptrs[markertype.loc]->value();

                // If the object has this type of markers,
                if (value == nullptr)
                    continue;

                // Extract the name of the marker that the object uses
                marker = getMarkerObj(value, object->document);

                // Set the marker color
                if (update < 0) {
                    // query prefs (only once)
                    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
                    update = prefs->getBool("/options/markers/colorUpdateMarkers", true) ? 1 : 0;
                }

                if (update > 0) {
                    setMarkerColor(marker, markertype.loc, SP_ITEM(object));

                    if (!skip_undo) {
                        SPDocument *document = desktop->getDocument();
                        DocumentUndo::maybeDone(document, "UaM", SP_VERB_DIALOG_FILL_STROKE,
                                       _("Set marker color"));
                    }
                }
            }
        }

        // Scroll the combobox to that marker
        combo->set_current(marker);
    }

}



} // namespace Inkscape


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
