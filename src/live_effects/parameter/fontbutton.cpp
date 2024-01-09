// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "fontbutton.h"

#include <glibmm/i18n.h>
#include <gtkmm.h>

#include "live_effects/effect.h"
#include "svg/stringstream.h"
#include "svg/svg.h"
#include "ui/icon-names.h"
#include "ui/widget/font-button.h"
#include "ui/widget/registered-widget.h"


namespace Inkscape {

namespace LivePathEffect {

FontButtonParam::FontButtonParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const Glib::ustring default_value )
    : Parameter(label, tip, key, wr, effect),
      value(default_value),
      defvalue(default_value)
{
}

void
FontButtonParam::param_set_default()
{
    param_setValue(defvalue);
}

void 
FontButtonParam::param_update_default(const gchar * default_value)
{
    defvalue = Glib::ustring(default_value);
}

bool
FontButtonParam::param_readSVGValue(const gchar * strvalue)
{
    Inkscape::SVGOStringStream os;
    os << strvalue;
    param_setValue((Glib::ustring)os.str());
    return true;
}

Glib::ustring
FontButtonParam::param_getSVGValue() const
{
    return value.c_str();
}

Glib::ustring
FontButtonParam::param_getDefaultSVGValue() const
{
    return defvalue;
}



Gtk::Widget *
FontButtonParam::param_newWidget()
{
    Inkscape::UI::Widget::RegisteredFontButton * fontbuttonwdg = Gtk::manage(
        new Inkscape::UI::Widget::RegisteredFontButton( param_label,
                                                        param_tooltip,
                                                        param_key,
                                                        *param_wr,
                                                        param_effect->getRepr(),
                                                        param_effect->getSPDoc() ) );
    Glib::ustring fontspec = param_getSVGValue();
    fontbuttonwdg->setValue( fontspec);
    fontbuttonwdg->set_undo_parameters(_("Change font button parameter"), INKSCAPE_ICON("dialog-path-effects"));
    return dynamic_cast<Gtk::Widget *> (fontbuttonwdg);
}

void
FontButtonParam::param_setValue(const Glib::ustring newvalue)
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
