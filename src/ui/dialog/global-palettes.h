// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Global color palette information.
 */
/* Authors: PBS <pbs3141@gmail.com>
 * Copyright (C) 2022 PBS
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_GLOBAL_PALETTES_H
#define INKSCAPE_UI_DIALOG_GLOBAL_PALETTES_H

#include <array>
#include <vector>
#include <glibmm/ustring.h>

namespace Inkscape {
namespace UI {
namespace Dialog {

/**
 * The data loaded from a palette file.
 */
struct PaletteFileData
{
    /// Name of the palette, either specified in the file or taken from the filename.
    Glib::ustring name;

    /// The preferred number of columns (unused).
    int columns;

    /// Whether this is a user or system palette.
    bool user;

    struct Color
    {
        /// RGB color.
        std::array<unsigned, 3> rgb;

        /// Name of the color, either specified in the file or generated from the rgb.
        Glib::ustring name;
    };

    /// The list of colors in the palette.
    std::vector<Color> colors;

    /// Load from the given file, throwing std::runtime_error on fail.
    PaletteFileData(Glib::ustring const &path);
};

/**
 * Singleton class that manages the static list of global palettes.
 */
class GlobalPalettes
{
    GlobalPalettes();
public:
    static GlobalPalettes const &get();
    std::vector<PaletteFileData> palettes;
};

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_DIALOG_GLOBAL_PALETTES_H
