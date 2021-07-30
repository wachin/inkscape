// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2007 Authors:
 *   Christopher Brown <audiere@gmail.com>
 *   Ted Gould <ted@gould.cx>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "extension/effect.h"
#include "extension/system.h"

#include "modulate.h"
#include <Magick++.h>

namespace Inkscape {
namespace Extension {
namespace Internal {
namespace Bitmap {
	
void
Modulate::applyEffect(Magick::Image* image) {
	double hue = (_hue * 200 / 360.0) + 100;
	image->modulate(_brightness, _saturation, hue);
}

void
Modulate::refreshParameters(Inkscape::Extension::Effect* module) {
	_brightness = module->get_param_float("brightness");
	_saturation = module->get_param_float("saturation");
	_hue = module->get_param_float("hue");
}

#include "../clear-n_.h"

void
Modulate::init()
{
    // clang-format off
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>" N_("HSB Adjust") "</name>\n"
            "<id>org.inkscape.effect.bitmap.modulate</id>\n"
            "<param name=\"hue\" gui-text=\"" N_("Hue:") "\" type=\"float\" min=\"-360\" max=\"360\">0</param>\n"
            "<param name=\"saturation\" gui-text=\"" N_("Saturation:") "\" type=\"float\" min=\"0\" max=\"200\">100</param>\n"
            "<param name=\"brightness\" gui-text=\"" N_("Brightness:") "\" type=\"float\" min=\"0\" max=\"200\">100</param>\n"
            "<effect>\n"
                "<object-type>all</object-type>\n"
                "<effects-menu>\n"
                    "<submenu name=\"" N_("Raster") "\" />\n"
                "</effects-menu>\n"
                "<menu-tip>" N_("Adjust the amount of hue, saturation, and brightness in selected bitmap(s)") "</menu-tip>\n"
            "</effect>\n"
        "</inkscape-extension>\n", new Modulate());
    // clang-format on
}

}; /* namespace Bitmap */
}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */
