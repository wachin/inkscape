// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Pixel based screen template sizes.
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef EXTENSION_INTERNAL_TEMPLATE_SCREEN_H
#define EXTENSION_INTERNAL_TEMPLATE_SCREEN_H

#include "extension/internal/template-base.h"

namespace Inkscape {
namespace Extension {
namespace Internal {

class TemplateScreen : TemplateBase
{
public:
    TemplateScreen(){};
    static void init();
};

} // namespace Internal
} // namespace Extension
} // namespace Inkscape
#endif /* EXTENSION_INTERNAL_TEMPLATE_SCREEN_H */
