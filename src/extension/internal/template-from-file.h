// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Collect templates as svg documents and express them as usable
 * templates to the user with an icon.
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef EXTENSION_INTERNAL_TEMPLATE_FROM_FILE_H
#define EXTENSION_INTERNAL_TEMPLATE_FROM_FILE_H

#include <glib.h>

#include "extension/extension.h"
#include "extension/implementation/implementation.h"
#include "extension/system.h"
#include "extension/template.h"
#include "xml/repr.h"

class SPDocument;

namespace Inkscape {
namespace Extension {
namespace Internal {

class TemplatePresetFile : public TemplatePreset
{
public:
    TemplatePresetFile(Template *mod, const std::string &filename);
private:
    void _load_data(const Inkscape::XML::Node *root);
};

class TemplateFromFile : public Inkscape::Extension::Implementation::Implementation
{
public:
    static void init();
    bool check(Inkscape::Extension::Extension *module) override { return true; };
    SPDocument *new_from_template(Inkscape::Extension::Template *tmod) override;

    void get_template_presets(const Template *tmod, TemplatePresets &presets) const override;
private:
};

} // namespace Internal
} // namespace Extension
} // namespace Inkscape
#endif /* EXTENSION_INTERNAL_TEMPLATE_FROM_FILE_H */
