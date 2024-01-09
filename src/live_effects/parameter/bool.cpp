// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "bool.h"

#include <glibmm/i18n.h>

#include "inkscape.h"

#include "live_effects/effect.h"
#include "svg/stringstream.h"
#include "svg/svg.h"
#include "ui/icon-names.h"
#include "ui/widget/registered-widget.h"
#include "util/numeric/converters.h"


namespace Inkscape {

namespace LivePathEffect {

BoolParam::BoolParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, bool default_value)
    : Parameter(label, tip, key, wr, effect), value(default_value), defvalue(default_value)
{
}

BoolParam::~BoolParam() = default;

void
BoolParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
BoolParam::param_update_default(bool const default_value)
{
    defvalue = default_value;
}

void 
BoolParam::param_update_default(const gchar * default_value)
{
    param_update_default(Inkscape::Util::read_bool(default_value, defvalue));
}

bool
BoolParam::param_readSVGValue(const gchar * strvalue)
{
    param_setValue(Inkscape::Util::read_bool(strvalue, defvalue));
    return true; // not correct: if value is unacceptable, should return false!
}

Glib::ustring
BoolParam::param_getSVGValue() const
{
    return value ? "true" : "false";
}

Glib::ustring
BoolParam::param_getDefaultSVGValue() const
{
    return defvalue ? "true" : "false";
}

Gtk::Widget *
BoolParam::param_newWidget()
{
    if(widget_is_visible){
        Inkscape::UI::Widget::RegisteredCheckButton * checkwdg = Gtk::manage(
            new Inkscape::UI::Widget::RegisteredCheckButton( param_label,
                                                             param_tooltip,
                                                             param_key,
                                                             *param_wr,
                                                             false,
                                                             param_effect->getRepr(),
                                                             param_effect->getSPDoc()) );

        checkwdg->setActive(value);
        checkwdg->setProgrammatically = false;
        checkwdg->set_undo_parameters(_("Change bool parameter"), INKSCAPE_ICON("dialog-path-effects"));
        return dynamic_cast<Gtk::Widget *> (checkwdg);
    } else {
        return nullptr;
    }
}

void
BoolParam::param_setValue(bool newvalue)
{
    if (value != newvalue) {
        param_effect->refresh_widgets = true;
    }
    value = newvalue;
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
