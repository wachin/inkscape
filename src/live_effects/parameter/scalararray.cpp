// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2008 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/effect.h"
#include "live_effects/parameter/scalararray.h"
#include "ui/icon-names.h"
#include "ui/knot/knot-holder-entity.h"

#include <glibmm/i18n.h>

namespace Inkscape {

namespace LivePathEffect {

ScalarArrayParam::ScalarArrayParam( const Glib::ustring& label,
                const Glib::ustring& tip,
                const Glib::ustring& key,
                Inkscape::UI::Widget::Registry* wr,
                Effect* effect,
                double default_value,
                bool visible, 
                size_t n)
    : ArrayParam<double>(label, tip, key, wr, effect, n)
    , defvalue(default_value)
    , min(-SCALARPARAM_G_MAXDOUBLE)
    , max(SCALARPARAM_G_MAXDOUBLE)
    , integer(false)
    , digits(2)
    , inc_step(0.1)
    , inc_page(1)
    , add_slider(false)
    , _set_undo(true)
{
    widget_is_visible = visible;
}

ScalarArrayParam::~ScalarArrayParam()
= default;

Gtk::Widget *ScalarArrayParam::param_newWidget()
{
    if (widget_is_visible) {
        Inkscape::UI::Widget::RegisteredScalar *rsu = Gtk::manage(new Inkscape::UI::Widget::RegisteredScalar(
            param_label, param_tooltip, param_key, *param_wr, param_effect->getRepr(), param_effect->getSPDoc()));
        rsu->setProgrammatically = true;
        rsu->setValue(_vector[_active_index]);
        rsu->setProgrammatically = true;
        rsu->setDigits(digits);
        rsu->setIncrements(inc_step, inc_page);
        rsu->setRange(min, max);
        if (add_slider) {
            rsu->addSlider();
        }
        if (_set_undo) {
            rsu->set_undo_parameters(_("Change scalar parameter"), INKSCAPE_ICON("dialog-path-effects"));
        }
        rsu->setProgrammatically = true;
        rsu->signal_value_changed().connect (sigc::bind(sigc::mem_fun (*this, &ScalarArrayParam::on_value_changed),rsu));
        return dynamic_cast<Gtk::Widget *>(rsu);
    } else {
        return nullptr;
    }
}

void
ScalarArrayParam::on_value_changed(Inkscape::UI::Widget::RegisteredScalar *rsu)
{
    rsu->setProgrammatically = true;
    double val = rsu->getValue() < 1e-6 && rsu->getValue() > -1e-6?0.0: rsu->getValue();
    _vector[_active_index] = val;
    param_set_and_write_new_value(_vector);
}

Glib::ustring
ScalarArrayParam::param_getDefaultSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << defvalue;
    return os.str();
}


void ScalarArrayParam::param_set_default() {
    for (auto &vec : _vector) {
        vec = defvalue;
    }
}

void ScalarArrayParam::param_update_default(gdouble default_value) { defvalue = default_value; }

void ScalarArrayParam::param_update_default(const gchar *default_value)
{
    double newval;
    unsigned int success = sp_svg_number_read_d(default_value, &newval);
    if (success == 1) {
        param_update_default(newval);
    }
}

void ScalarArrayParam::param_set_range(gdouble min, gdouble max)
{
    // if you look at client code, you'll see that many effects
    // has a tendency to set an upper range of Geom::infinity().
    // Once again, in gtk2, this is not a problem. But in gtk3,
    // widgets get allocated the amount of size they ask for,
    // leading to excessively long widgets.
    if (min >= -SCALARPARAM_G_MAXDOUBLE) {
        this->min = min;
    } else {
        this->min = -SCALARPARAM_G_MAXDOUBLE;
    }
    if (max <= SCALARPARAM_G_MAXDOUBLE) {
        this->max = max;
    } else {
        this->max = SCALARPARAM_G_MAXDOUBLE;
    }
    this->param_effect->refresh_widgets = true;
}

void ScalarArrayParam::param_make_integer(bool yes)
{
    integer = yes;
    digits = 0;
    inc_step = 1;
    inc_page = 10;
}

void ScalarArrayParam::param_set_undo(bool set_undo) { _set_undo = set_undo; }

void ScalarArrayParam::param_set_digits(unsigned digits) { this->digits = digits; }

void ScalarArrayParam::param_set_increments(double step, double page)
{
    inc_step = step;
    inc_page = page;
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
