// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Inkscape guideline implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Peter Moulder <pmoulder@mail.csse.monash.edu.au>
 *   Johan Engelen
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *
 * Copyright (C) 2000-2002 authors
 * Copyright (C) 2004 Monash University
 * Copyright (C) 2007 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cstring>
#include <vector>
#include <glibmm/i18n.h>

#include "attributes.h"
#include "desktop.h"
#include "desktop-events.h"
#include "document-undo.h"
#include "helper-fns.h"
#include "inkscape.h"
#include "verbs.h"

#include "sp-guide.h"
#include "sp-namedview.h"
#include "sp-root.h"

#include "display/control/canvas-item-guideline.h"

#include "svg/stringstream.h"
#include "svg/svg-color.h"
#include "svg/svg.h"

#include "ui/widget/canvas.h" // Should really be here

#include "xml/repr.h"

using Inkscape::DocumentUndo;


SPGuide::SPGuide()
    : SPObject()
    , label(nullptr)
    , locked(false)
    , normal_to_line(Geom::Point(0.,1.))
    , point_on_line(Geom::Point(0.,0.))
    , color(0x0000ff7f)
    , hicolor(0xff00007f)
{}

void SPGuide::setColor(guint32 color)
{
    this->color = color;
    for (auto view : views) {
        view->set_stroke(color);
    }
}

void SPGuide::build(SPDocument *document, Inkscape::XML::Node *repr)
{
    SPObject::build(document, repr);

    this->readAttr(SPAttr::INKSCAPE_COLOR);
    this->readAttr(SPAttr::INKSCAPE_LABEL);
    this->readAttr(SPAttr::INKSCAPE_LOCKED);
    this->readAttr(SPAttr::ORIENTATION);
    this->readAttr(SPAttr::POSITION);

    /* Register */
    document->addResource("guide", this);
}

void SPGuide::release()
{
    for(auto view : views) {
        delete view;
    }
    this->views.clear();

    if (this->document) {
        // Unregister ourselves
        this->document->removeResource("guide", this);
    }

    SPObject::release();
}

void SPGuide::set(SPAttr key, const gchar *value) {
    switch (key) {
    case SPAttr::INKSCAPE_COLOR:
        if (value) {
            this->setColor(sp_svg_read_color(value, 0x0000ff00) | 0x7f);
        }
        break;
    case SPAttr::INKSCAPE_LABEL:
        // this->label already freed in sp_guideline_set_label (src/display/guideline.cpp)
        // see bug #1498444, bug #1469514
        if (value) {
            this->label = g_strdup(value);
        } else {
            this->label = nullptr;
        }

        this->set_label(this->label, false);
        break;
    case SPAttr::INKSCAPE_LOCKED:
        if (value) {
            this->set_locked(helperfns_read_bool(value, false), false);
        }
        break;
    case SPAttr::ORIENTATION:
    {
        if (value && !strcmp(value, "horizontal")) {
            /* Visual representation of a horizontal line, constrain vertically (y coordinate). */
            this->normal_to_line = Geom::Point(0., 1.);
        } else if (value && !strcmp(value, "vertical")) {
            this->normal_to_line = Geom::Point(1., 0.);
        } else if (value) {
            gchar ** strarray = g_strsplit(value, ",", 2);
            double newx, newy;
            unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
            success += sp_svg_number_read_d(strarray[1], &newy);
            g_strfreev (strarray);
            if (success == 2 && (fabs(newx) > 1e-6 || fabs(newy) > 1e-6)) {
                Geom::Point direction(newx, newy);

                // <sodipodi:guide> stores inverted y-axis coordinates
                if (document->is_yaxisdown()) {
                    direction[Geom::X] *= -1.0;
                }

                direction.normalize();
                this->normal_to_line = direction;
            } else {
                // default to vertical line for bad arguments
                this->normal_to_line = Geom::Point(1., 0.);
            }
        } else {
            // default to vertical line for bad arguments
            this->normal_to_line = Geom::Point(1., 0.);
        }
        this->set_normal(this->normal_to_line, false);
    }
    break;
    case SPAttr::POSITION:
    {
        if (value) {
            gchar ** strarray = g_strsplit(value, ",", 2);
            double newx, newy;
            unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
            success += sp_svg_number_read_d(strarray[1], &newy);
            g_strfreev (strarray);
            if (success == 2) {
                // If root viewBox set, interpret guides in terms of viewBox (90/96)
                SPRoot *root = document->getRoot();
                if( root->viewBox_set ) {
                    if(Geom::are_near((root->width.computed * root->viewBox.height()) / (root->viewBox.width() * root->height.computed), 1.0, Geom::EPSILON)) {
                        // for uniform scaling, try to reduce numerical error
                        double vbunit2px = (root->width.computed / root->viewBox.width() + root->height.computed / root->viewBox.height())/2.0;
                        newx = newx * vbunit2px;
                        newy = newy * vbunit2px;
                    } else {
                        newx = newx * root->width.computed  / root->viewBox.width();
                        newy = newy * root->height.computed / root->viewBox.height();
                    }
                }
                this->point_on_line = Geom::Point(newx, newy);
            } else if (success == 1) {
                // before 0.46 style guideline definition.
                const gchar *attr = this->getRepr()->attribute("orientation");
                if (attr && !strcmp(attr, "horizontal")) {
                    this->point_on_line = Geom::Point(0, newx);
                } else {
                    this->point_on_line = Geom::Point(newx, 0);
                }
            }

            // <sodipodi:guide> stores inverted y-axis coordinates
            if (document->is_yaxisdown()) {
                this->point_on_line[Geom::Y] = document->getHeight().value("px") - this->point_on_line[Geom::Y];
            }
        } else {
            // default to (0,0) for bad arguments
            this->point_on_line = Geom::Point(0,0);
        }
        // update position in non-committing way
        // fixme: perhaps we need to add an update method instead, and request_update here
        this->moveto(this->point_on_line, false);
    }
    break;
    default:
    	SPObject::set(key, value);
        break;
    }
}

/* Only used internally and in sp-line.cpp */
SPGuide *SPGuide::createSPGuide(SPDocument *doc, Geom::Point const &pt1, Geom::Point const &pt2)
{
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();

    Inkscape::XML::Node *repr = xml_doc->createElement("sodipodi:guide");

    Geom::Point n = Geom::rot90(pt2 - pt1);

    // If root viewBox set, interpret guides in terms of viewBox (90/96)
    double newx = pt1.x();
    double newy = pt1.y();

    SPRoot *root = doc->getRoot();

    // <sodipodi:guide> stores inverted y-axis coordinates
    if (doc->is_yaxisdown()) {
        newy = doc->getHeight().value("px") - newy;
        n[Geom::X] *= -1.0;
    }

    if( root->viewBox_set ) {
        // check to see if scaling is uniform
        if(Geom::are_near((root->viewBox.width() * root->height.computed) / (root->width.computed * root->viewBox.height()), 1.0, Geom::EPSILON)) {
            double px2vbunit = (root->viewBox.width()/root->width.computed + root->viewBox.height()/root->height.computed)/2.0;
            newx = newx * px2vbunit;
            newy = newy * px2vbunit;
        } else {
            newx = newx * root->viewBox.width()  / root->width.computed;
            newy = newy * root->viewBox.height() / root->height.computed;
        }
    }

    repr->setAttributePoint("position", Geom::Point( newx, newy ));
    repr->setAttributePoint("orientation", n);

    SPNamedView *namedview = sp_document_namedview(doc, nullptr);
    if (namedview) {
        if (namedview->lockguides) {
            repr->setAttribute("inkscape:locked", "true");
        }
        namedview->appendChild(repr);
    }
    Inkscape::GC::release(repr);

    SPGuide *guide= SP_GUIDE(doc->getObjectByRepr(repr));
    return guide;
}

SPGuide *SPGuide::duplicate(){
    return SPGuide::createSPGuide(
        document,
        point_on_line,
        Geom::Point(
            point_on_line[Geom::X] + normal_to_line[Geom::Y],
            point_on_line[Geom::Y] - normal_to_line[Geom::X]
            )
        );
}

void sp_guide_pt_pairs_to_guides(SPDocument *doc, std::list<std::pair<Geom::Point, Geom::Point> > &pts)
{
    for (auto & pt : pts) {
        SPGuide::createSPGuide(doc, pt.first, pt.second);
    }
}

void sp_guide_create_guides_around_page(SPDesktop *dt)
{
    SPDocument *doc=dt->getDocument();
    std::list<std::pair<Geom::Point, Geom::Point> > pts;

    Geom::Point A(0, 0);
    Geom::Point C(doc->getWidth().value("px"), doc->getHeight().value("px"));
    Geom::Point B(C[Geom::X], 0);
    Geom::Point D(0, C[Geom::Y]);

    pts.emplace_back(A, B);
    pts.emplace_back(B, C);
    pts.emplace_back(C, D);
    pts.emplace_back(D, A);

    sp_guide_pt_pairs_to_guides(doc, pts);

    DocumentUndo::done(doc, SP_VERB_NONE, _("Create Guides Around the Page"));
}

void sp_guide_delete_all_guides(SPDesktop *dt)
{
    SPDocument *doc=dt->getDocument();
    std::vector<SPObject *> current = doc->getResourceList("guide");
    while (!current.empty()){
        SPGuide* guide = SP_GUIDE(*(current.begin()));
        sp_guide_remove(guide);
        current = doc->getResourceList("guide");
    }

    DocumentUndo::done(doc, SP_VERB_NONE, _("Delete All Guides"));
}

// Actually, create a new guide.
void SPGuide::showSPGuide(Inkscape::CanvasItemGroup *group)
{
    Glib::ustring ulabel = (label?label:"");
    auto item = new Inkscape::CanvasItemGuideLine(group, ulabel, point_on_line, normal_to_line);
    item->set_stroke(color);
    item->set_locked(locked);

    item->connect_event(sigc::bind(sigc::ptr_fun(&sp_dt_guide_event), item, this));

    views.push_back(item);
}

void SPGuide::showSPGuide()
{
    for (auto view : views) {
        view->show();
    }
}

// Actually deleted guide from a particular canvas.
void SPGuide::hideSPGuide(Inkscape::UI::Widget::Canvas *canvas)
{
    g_assert(canvas != nullptr);
    for (auto it = views.begin(); it != views.end(); ++it) {
        if (canvas == (*it)->get_canvas()) { // A guide can be displayed on more than one desktop with the same document.
            delete (*it);
            views.erase(it);
            return;
        }
    }

    assert(false);
}

void SPGuide::hideSPGuide()
{
    for(auto view : views) {
        view->hide();
    }
}

void SPGuide::sensitize(Inkscape::UI::Widget::Canvas *canvas, bool sensitive)
{
    g_assert(canvas != nullptr);

    for (auto view : views) {
        if (canvas == view->get_canvas()) {
            view->set_sensitive(sensitive);
            return;
        }
    }

    assert(false);
}

/**
 * \arg commit False indicates temporary moveto in response to motion event while dragging,
 *      true indicates a "committing" version: in response to button release event after
 *      dragging a guideline, or clicking OK in guide editing dialog.
 */
void SPGuide::moveto(Geom::Point const point_on_line, bool const commit)
{
    if(this->locked) {
        return;
    }

    for(auto view : this->views) {
        view->set_origin(point_on_line);
    }

    /* Calling Inkscape::XML::Node::setAttributePoint must precede calling sp_item_notify_moveto in the commit
       case, so that the guide's new position is available for sp_item_rm_unsatisfied_cns. */
    if (commit) {
        // If root viewBox set, interpret guides in terms of viewBox (90/96)
        double newx = point_on_line.x();
        double newy = point_on_line.y();

        // <sodipodi:guide> stores inverted y-axis coordinates
        if (document->is_yaxisdown()) {
            newy = document->getHeight().value("px") - newy;
        }

        SPRoot *root = document->getRoot();
        if( root->viewBox_set ) {
            // check to see if scaling is uniform
            if(Geom::are_near((root->viewBox.width() * root->height.computed) / (root->width.computed * root->viewBox.height()), 1.0, Geom::EPSILON)) {
                double px2vbunit = (root->viewBox.width()/root->width.computed + root->viewBox.height()/root->height.computed)/2.0;
                newx = newx * px2vbunit;
                newy = newy * px2vbunit;
            } else {
                newx = newx * root->viewBox.width()  / root->width.computed;
                newy = newy * root->viewBox.height() / root->height.computed;
            }
        }

        //XML Tree being used here directly while it shouldn't be.
        getRepr()->setAttributePoint("position", Geom::Point(newx, newy) );
    }
}

/**
 * \arg commit False indicates temporary moveto in response to motion event while dragging,
 *      true indicates a "committing" version: in response to button release event after
 *      dragging a guideline, or clicking OK in guide editing dialog.
 */
void SPGuide::set_normal(Geom::Point const normal_to_line, bool const commit)
{
    if(this->locked) {
        return;
    }
    for(auto view : this->views) {
        view->set_normal(normal_to_line);
    }

    /* Calling sp_repr_set_svg_point must precede calling sp_item_notify_moveto in the commit
       case, so that the guide's new position is available for sp_item_rm_unsatisfied_cns. */
    if (commit) {
        //XML Tree being used directly while it shouldn't be
        auto normal = normal_to_line;

        // <sodipodi:guide> stores inverted y-axis coordinates
        if (document->is_yaxisdown()) {
            normal[Geom::X] *= -1.0;
        }

        getRepr()->setAttributePoint("orientation", normal);
    }
}

void SPGuide::set_color(const unsigned r, const unsigned g, const unsigned b, bool const commit)
{
    this->color = (r << 24) | (g << 16) | (b << 8) | 0x7f;

    if (! views.empty()) {
        views[0]->set_stroke(color);
    }

    if (commit) {
        std::ostringstream os;
        os << "rgb(" << r << "," << g << "," << b << ")";
        //XML Tree being used directly while it shouldn't be
        setAttribute("inkscape:color", os.str());
    }
}

void SPGuide::set_locked(const bool locked, bool const commit)
{
    this->locked = locked;
    if ( !views.empty() ) {
        views[0]->set_locked(locked);
    }

    if (commit) {
        setAttribute("inkscape:locked", locked ? "true" : "false");
    }
}

void SPGuide::set_label(const char* label, bool const commit)
{
    if (!views.empty()) {
        views[0]->set_label(label);
    }

    if (commit) {
        //XML Tree being used directly while it shouldn't be
        setAttribute("inkscape:label", label);
    }
}

/**
 * Returns a human-readable description of the guideline for use in dialog boxes and status bar.
 * If verbose is false, only positioning information is included (useful for dialogs).
 *
 * The caller is responsible for freeing the string.
 */
char* SPGuide::description(bool const verbose) const
{
    using Geom::X;
    using Geom::Y;

    char *descr = nullptr;
    if ( !this->document ) {
        // Guide has probably been deleted and no longer has an attached namedview.
        descr = g_strdup(_("Deleted"));
    } else {
        SPNamedView *namedview = sp_document_namedview(this->document, nullptr);

        Inkscape::Util::Quantity x_q = Inkscape::Util::Quantity(this->point_on_line[X], "px");
        Inkscape::Util::Quantity y_q = Inkscape::Util::Quantity(this->point_on_line[Y], "px");
        Glib::ustring position_string_x = x_q.string(namedview->display_units);
        Glib::ustring position_string_y = y_q.string(namedview->display_units);

        gchar *shortcuts = g_strdup_printf("; %s", _("<b>Shift+drag</b> to rotate, <b>Ctrl+drag</b> to move origin, <b>Del</b> to delete"));

        if ( are_near(this->normal_to_line, Geom::Point(1., 0.)) ||
             are_near(this->normal_to_line, -Geom::Point(1., 0.)) ) {
            descr = g_strdup_printf(_("vertical, at %s"), position_string_x.c_str());
        } else if ( are_near(this->normal_to_line, Geom::Point(0., 1.)) ||
                    are_near(this->normal_to_line, -Geom::Point(0., 1.)) ) {
            descr = g_strdup_printf(_("horizontal, at %s"), position_string_y.c_str());
        } else {
            double const radians = this->angle();
            double const degrees = Geom::deg_from_rad(radians);
            int const degrees_int = (int) round(degrees);
            descr = g_strdup_printf(_("at %d degrees, through (%s,%s)"), 
                                    degrees_int, position_string_x.c_str(), position_string_y.c_str());
        }
        
        if (verbose) {
            gchar *oldDescr = descr;
            descr = g_strconcat(oldDescr, shortcuts, nullptr);
            g_free(oldDescr);
        }

        g_free(shortcuts);
    }

    return descr;
}

void sp_guide_remove(SPGuide *guide)
{
    g_assert(SP_IS_GUIDE(guide));

    //XML Tree being used directly while it shouldn't be.
    sp_repr_unparent(guide->getRepr());
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
