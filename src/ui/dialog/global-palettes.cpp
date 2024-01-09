// SPDX-License-Identifier: GPL-2.0-or-later
#include "global-palettes.h"

#include <iomanip>

// Using Glib::regex because
//  - std::regex is too slow in debug mode.
//  - boost::regex requires a library not present in the CI image.
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/regex.h>

#include "io/resource.h"
#include "io/sys.h"

Inkscape::UI::Dialog::PaletteFileData::PaletteFileData(Glib::ustring const &path)
{
    name = Glib::path_get_basename(path);
    columns = 1;
    user = Inkscape::IO::file_is_writable(path.c_str());

    auto f = std::unique_ptr<FILE, void(*)(FILE*)>(Inkscape::IO::fopen_utf8name(path.c_str(), "r"), [] (FILE *f) {if (f) std::fclose(f);});
    if (!f) throw std::runtime_error("Failed to open file");

    char buf[1024];
    if (!std::fgets(buf, sizeof(buf), f.get())) throw std::runtime_error("File is empty");
    if (std::strncmp("GIMP Palette", buf, 12) != 0) throw std::runtime_error("First line is wrong");

    static auto const regex_rgb   = Glib::Regex::create("\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(?:\\s(.*\\S)\\s*)?$", Glib::REGEX_OPTIMIZE | Glib::REGEX_ANCHORED);
    static auto const regex_name  = Glib::Regex::create("\\s*Name:\\s*(.*\\S)", Glib::REGEX_OPTIMIZE | Glib::REGEX_ANCHORED);
    static auto const regex_cols  = Glib::Regex::create("\\s*Columns:\\s*(.*\\S)", Glib::REGEX_OPTIMIZE | Glib::REGEX_ANCHORED);
    static auto const regex_blank = Glib::Regex::create("\\s*(?:$|#)", Glib::REGEX_OPTIMIZE | Glib::REGEX_ANCHORED);

    while (std::fgets(buf, sizeof(buf), f.get())) {
        auto line = Glib::ustring(buf); // Unnecessary copy required until using a glibmm with support for string views. TODO: Fix when possible.
        Glib::MatchInfo match;
        if (regex_rgb->match(line, match)) { // ::regex_match(line, match, boost::regex(), boost::regex_constants::match_continuous)) {
            // RGB color, followed by an optional name.
            Color color;
            for (int i = 0; i < 3; i++) {
                color.rgb[i] = std::clamp(std::stoi(match.fetch(i + 1)), 0, 255);
            }
            color.name = match.fetch(4);

            if (!color.name.empty()) {
                // Translate the name if present.
                color.name = g_dpgettext2(nullptr, "Palette", color.name.c_str());
            } else {
                // Otherwise, set the name to be the hex value.
                color.name = Glib::ustring::compose("#%1%2%3",
                                 Glib::ustring::format(std::hex, std::setw(2), std::setfill(L'0'), color.rgb[0]),
                                 Glib::ustring::format(std::hex, std::setw(2), std::setfill(L'0'), color.rgb[1]),
                                 Glib::ustring::format(std::hex, std::setw(2), std::setfill(L'0'), color.rgb[2])
                             ).uppercase();
            }

            colors.emplace_back(std::move(color));
        } else if (regex_name->match(line, match)) {
            // Header entry for name.
            name = match.fetch(1);
        } else if (regex_cols->match(line, match)) {
            // Header entry for columns.
            columns = std::clamp(std::stoi(match.fetch(1)), 1, 1000);
        } else if (regex_blank->match(line, match)) {
            // Comment or blank line.
        } else {
            // Unrecognised.
            throw std::runtime_error("Invalid line " + std::string(line));
        }
    }
}

Inkscape::UI::Dialog::GlobalPalettes::GlobalPalettes()
{
    // Load the palettes.
    for (auto &path : Inkscape::IO::Resource::get_filenames(Inkscape::IO::Resource::PALETTES, {".gpl"})) {
        try {
            palettes.emplace_back(path);
        } catch (std::runtime_error const &e) {
            g_warning("Error loading palette %s: %s", path.c_str(), e.what());
        } catch (std::logic_error const &e) {
            g_warning("Error loading palette %s: %s", path.c_str(), e.what());
        }
    }

    std::sort(palettes.begin(), palettes.end(), [] (decltype(palettes)::const_reference a, decltype(palettes)::const_reference b) {
        // Sort by user/system first...
        if (a.user > b.user) return true;
        if (b.user > a.user) return false;
        // ... then by name.
        return a.name.compare(b.name) < 0;
    });
}

Inkscape::UI::Dialog::GlobalPalettes const &Inkscape::UI::Dialog::GlobalPalettes::get()
{
    static GlobalPalettes instance;
    return instance;
}
