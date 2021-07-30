// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Theodore Janeczko 2012 <flutterguy317@gmail.com>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/transformedpoint.h"

#include "desktop.h"
#include "verbs.h"

#include "live_effects/effect.h"
#include "svg/svg.h"
#include "svg/stringstream.h"
#include "ui/knot/knot-holder.h"
#include "ui/knot/knot-holder-entity.h"
#include "ui/widget/registered-widget.h"

#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

TransformedPointParam::TransformedPointParam( const Glib::ustring& label, const Glib::ustring& tip,
                        const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                        Effect* effect, Geom::Point default_vector,
                        bool dontTransform)
    : Parameter(label, tip, key, wr, effect),
      defvalue(default_vector),
      vector(default_vector),
      noTransform(dontTransform)
{
}

TransformedPointParam::~TransformedPointParam()
= default;

void
TransformedPointParam::param_set_default()
{
    setOrigin(Geom::Point(0.,0.));
    setVector(defvalue);
}

bool
TransformedPointParam::param_readSVGValue(const gchar * strvalue)
{
    gchar ** strarray = g_strsplit(strvalue, ",", 4);
    if (!strarray) {
        return false;
    }
    double val[4];
    unsigned int i = 0;
    while (i < 4 && strarray[i]) {
        if (sp_svg_number_read_d(strarray[i], &val[i]) != 0) {
            i++;
        } else {
            break;
        }
    }
    g_strfreev (strarray);
    if (i == 4) {
        setOrigin( Geom::Point(val[0], val[1]) );
        setVector( Geom::Point(val[2], val[3]) );
        return true;
    }
    return false;
}

Glib::ustring
TransformedPointParam::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << origin << " , " << vector;
    return os.str();
}

Glib::ustring
TransformedPointParam::param_getDefaultSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << defvalue;
    return os.str();
}

void
TransformedPointParam::param_update_default(Geom::Point default_point)
{
    defvalue = default_point;
}

void
TransformedPointParam::param_update_default(const gchar * default_point)
{
    gchar ** strarray = g_strsplit(default_point, ",", 2);
    double newx, newy;
    unsigned int success = sp_svg_number_read_d(strarray[0], &newx);
    success += sp_svg_number_read_d(strarray[1], &newy);
    g_strfreev (strarray);
    if (success == 2) {
        param_update_default( Geom::Point(newx, newy) );
    }
}

Gtk::Widget *
TransformedPointParam::param_newWidget()
{
    Inkscape::UI::Widget::RegisteredVector * pointwdg = Gtk::manage(
        new Inkscape::UI::Widget::RegisteredVector( param_label,
                                                    param_tooltip,
                                                    param_key,
                                                    *param_wr,
                                                    param_effect->getRepr(),
                                                    param_effect->getSPDoc() ) );
    pointwdg->setPolarCoords();
    pointwdg->setValue( vector, origin );
    pointwdg->clearProgrammatically();
    pointwdg->set_undo_parameters(SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Change vector parameter"));
    
    Gtk::Box * hbox = Gtk::manage( new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL) );
    hbox->pack_start(*pointwdg, true, true);
    hbox->show_all_children();

    return dynamic_cast<Gtk::Widget *> (hbox);
}

void
TransformedPointParam::set_and_write_new_values(Geom::Point const &new_origin, Geom::Point const &new_vector)
{
    setValues(new_origin, new_vector);
    param_write_to_repr(param_getSVGValue().c_str());
}

void
TransformedPointParam::param_transform_multiply(Geom::Affine const& postmul, bool /*set*/)
{
    if (!noTransform) {
        set_and_write_new_values( origin * postmul, vector * postmul.withoutTranslation() );
    }
}


void
TransformedPointParam::set_vector_oncanvas_looks(Inkscape::CanvasItemCtrlShape shape,
                                                 Inkscape::CanvasItemCtrlMode mode,
                                                 guint32 color)
{
    vec_knot_shape = shape;
    vec_knot_mode  = mode;
    vec_knot_color = color;
}

void
TransformedPointParam::set_oncanvas_color(guint32 color)
{
    vec_knot_color = color;
}

class TransformedPointParamKnotHolderEntity_Vector : public KnotHolderEntity {
public:
    TransformedPointParamKnotHolderEntity_Vector(TransformedPointParam *p) : param(p) { }
    ~TransformedPointParamKnotHolderEntity_Vector() override = default;

    void knot_set(Geom::Point const &p, Geom::Point const &/*origin*/, guint /*state*/) override {
        Geom::Point const s = p - param->origin;
        /// @todo implement angle snapping when holding CTRL
        param->setVector(s);
        param->set_and_write_new_values(param->origin, param->vector);
        sp_lpe_item_update_patheffect(SP_LPE_ITEM(item), false, false);
    };
    void knot_ungrabbed(Geom::Point const &p, Geom::Point const &origin, guint state) override
    {
        param->param_effect->refresh_widgets = true;
        param->write_to_SVG();
    };
    Geom::Point knot_get() const override{
        return param->origin + param->vector;
    };
    void knot_click(guint /*state*/) override{
        g_print ("This is the vector handle associated to parameter '%s'\n", param->param_key.c_str());
    };

private:
    TransformedPointParam *param;
};

void
TransformedPointParam::addKnotHolderEntities(KnotHolder *knotholder, SPDesktop *desktop, SPItem *item)
{
    TransformedPointParamKnotHolderEntity_Vector *vector_e = new TransformedPointParamKnotHolderEntity_Vector(this);
    vector_e->create(desktop, item, knotholder, Inkscape::CANVAS_ITEM_CTRL_TYPE_LPE, "LPE:Point", handleTip(),
                     vec_knot_color);
    knotholder->add(vector_e);
}

} /* namespace LivePathEffect */

} /* namespace Inkscape */

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
