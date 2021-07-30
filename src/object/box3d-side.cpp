// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * 3D box face implementation
 *
 * Authors:
 *   Maximilian Albert <Anhalter42@gmx.de>
 *   Abhishek Sharma
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2007  Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "box3d-side.h"
#include "document.h"
#include "xml/document.h"
#include "xml/repr.h"
#include "display/curve.h"
#include "svg/svg.h"
#include "attributes.h"
#include "inkscape.h"
#include "object/persp3d.h"
#include "object/persp3d-reference.h"
#include "object/box3d.h"
#include "ui/tools/box3d-tool.h"
#include "desktop-style.h"

static void box3d_side_compute_corner_ids(Box3DSide *side, unsigned int corners[4]);

Box3DSide::Box3DSide() : SPPolygon() {
    this->dir1 = Box3D::NONE;
    this->dir2 = Box3D::NONE;
    this->front_or_rear = Box3D::FRONT;
}

Box3DSide::~Box3DSide() = default;

void Box3DSide::build(SPDocument * document, Inkscape::XML::Node * repr) {
    SPPolygon::build(document, repr);

    this->readAttr(SPAttr::INKSCAPE_BOX3D_SIDE_TYPE);
}


Inkscape::XML::Node* Box3DSide::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        // this is where we end up when saving as plain SVG (also in other circumstances?)
        // thus we don' set "sodipodi:type" so that the box is only saved as an ordinary svg:path
        repr = xml_doc->createElement("svg:path");
    }

    if (flags & SP_OBJECT_WRITE_EXT) {
        repr->setAttributeInt("inkscape:box3dsidetype", this->dir1 ^ this->dir2 ^ this->front_or_rear);
    }

    this->set_shape();

    /* Duplicate the path */
    SPCurve const *curve = this->_curve.get();

    //Nulls might be possible if this called iteratively
    if ( !curve ) {
        return nullptr;
    }

    repr->setAttribute("d", sp_svg_write_path(curve->get_pathvector()));

    SPPolygon::write(xml_doc, repr, flags);

    return repr;
}

void Box3DSide::set(SPAttr key, const gchar* value) {
    // TODO: In case the box was recreated (by undo, e.g.) we need to recreate the path
    //       (along with other info?) from the parent box.

    /* fixme: we should really collect updates */
    switch (key) {
        case SPAttr::INKSCAPE_BOX3D_SIDE_TYPE:
            if (value) {
                guint desc = atoi (value);

                if (!Box3D::is_face_id(desc)) {
                    g_print ("desc is not a face id: =%s=\n", value);
                }

                g_return_if_fail (Box3D::is_face_id (desc));

                Box3D::Axis plane = (Box3D::Axis) (desc & 0x7);
                plane = (Box3D::is_plane(plane) ? plane : Box3D::orth_plane_or_axis(plane));
                this->dir1 = Box3D::extract_first_axis_direction(plane);
                this->dir2 = Box3D::extract_second_axis_direction(plane);
                this->front_or_rear = (Box3D::FrontOrRear) (desc & 0x8);

                this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            }
            break;
    default:
        SPPolygon::set(key, value);
        break;
    }
}

void Box3DSide::update(SPCtx* ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        flags &= ~SP_OBJECT_USER_MODIFIED_FLAG_B; // since we change the description, it's not a "just translation" anymore
    }

    if (flags & (SP_OBJECT_MODIFIED_FLAG |
                 SP_OBJECT_STYLE_MODIFIED_FLAG |
                 SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {

        this->set_shape();
    }

    SPPolygon::update(ctx, flags);
}

/* Create a new Box3DSide and append it to the parent box */
Box3DSide * Box3DSide::createBox3DSide(SPBox3D *box)
{
	Box3DSide *box3d_side = nullptr;
	Inkscape::XML::Document *xml_doc = box->document->getReprDoc();;
	Inkscape::XML::Node *repr_side = xml_doc->createElement("svg:path");
	repr_side->setAttribute("sodipodi:type", "inkscape:box3dside");
	box3d_side = static_cast<Box3DSide *>(box->appendChildRepr(repr_side));
	return box3d_side;
}

/*
 * Function which return the type attribute for Box3D. 
 * Acts as a replacement for directly accessing the XML Tree directly.
 */
int Box3DSide::getFaceId()
{
	    return this->getIntAttribute("inkscape:box3dsidetype", -1);
}

void
Box3DSide::position_set () {
	this->set_shape();

    // This call is responsible for live update of the sides during the initial drag
    this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
}

void Box3DSide::set_shape() {
    if (!this->document->getRoot()) {
        // avoid a warning caused by sp_document_height() (which is called from sp_item_i2d_affine() below)
        // when reading a file containing 3D boxes
        return;
    }

    SPObject *parent = this->parent;

    SPBox3D *box = dynamic_cast<SPBox3D *>(parent);
    if (!box) {
        g_warning("Parent of 3D box side is not a 3D box.\n");
        return;
    }

    Persp3D *persp = this->perspective();

    if (!persp) {
        return;
    }

    // TODO: Draw the correct quadrangle here
    //       To do this, determine the perspective of the box, the orientation of the side (e.g., XY-FRONT)
    //       compute the coordinates of the corners in P^3, project them onto the canvas, and draw the
    //       resulting path.

    unsigned int corners[4];
    box3d_side_compute_corner_ids(this, corners);

    auto c = std::make_unique<SPCurve>();

    if (!box->get_corner_screen(corners[0]).isFinite() ||
        !box->get_corner_screen(corners[1]).isFinite() ||
        !box->get_corner_screen(corners[2]).isFinite() ||
        !box->get_corner_screen(corners[3]).isFinite() )
    {
        g_warning ("Trying to draw a 3D box side with invalid coordinates.\n");
        return;
    }

    c->moveto(box->get_corner_screen(corners[0]));
    c->lineto(box->get_corner_screen(corners[1]));
    c->lineto(box->get_corner_screen(corners[2]));
    c->lineto(box->get_corner_screen(corners[3]));
    c->closepath();

    /* Reset the shape's curve to the "original_curve"
     * This is very important for LPEs to work properly! (the bbox might be recalculated depending on the curve in shape)*/

    SPCurve const *before = curveBeforeLPE();
    if (before && before->get_pathvector() != c->get_pathvector()) {
        setCurveBeforeLPE(std::move(c));
        sp_lpe_item_update_patheffect(this, true, false);
        return;
    }

    if (hasPathEffectOnClipOrMaskRecursive(this)) {
        setCurveBeforeLPE(std::move(c));
        return;
    }

    // This happends on undo, fix bug:#1791784
    setCurveInsync(std::move(c));
}

Glib::ustring Box3DSide::axes_string() const
{
    Glib::ustring result(Box3D::string_from_axes((Box3D::Axis) (this->dir1 ^ this->dir2)));

    switch ((Box3D::Axis) (this->dir1 ^ this->dir2)) {
        case Box3D::XY:
            result += ((this->front_or_rear == Box3D::FRONT) ? "front" : "rear");
            break;

        case Box3D::XZ:
            result += ((this->front_or_rear == Box3D::FRONT) ? "top" : "bottom");
            break;

        case Box3D::YZ:
            result += ((this->front_or_rear == Box3D::FRONT) ? "right" : "left");
            break;

        default:
            break;
    }

    return result;
}

static void
box3d_side_compute_corner_ids(Box3DSide *side, unsigned int corners[4]) {
    Box3D::Axis orth = Box3D::third_axis_direction (side->dir1, side->dir2);

    corners[0] = (side->front_or_rear ? orth : 0);
    corners[1] = corners[0] ^ side->dir1;
    corners[2] = corners[0] ^ side->dir1 ^ side->dir2;
    corners[3] = corners[0] ^ side->dir2;
}

Persp3D *
Box3DSide::perspective() const {
    SPBox3D *box = dynamic_cast<SPBox3D *>(this->parent);
    return box ? box->persp_ref->getObject() : nullptr;
}

Inkscape::XML::Node *Box3DSide::convert_to_path() const {
    // TODO: Copy over all important attributes (see sp_selected_item_to_curved_repr() for an example)
    SPDocument *doc = this->document;
    Inkscape::XML::Document *xml_doc = doc->getReprDoc();

    Inkscape::XML::Node *repr = xml_doc->createElement("svg:path");
    repr->setAttribute("d", this->getAttribute("d"));
    repr->setAttribute("style", this->getAttribute("style"));

    return repr;
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
