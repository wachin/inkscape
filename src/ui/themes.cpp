// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Gtk <themes> helper code.
 */
/*
 * Authors:
 *   Jabiertxof
 *   Martin Owens
 *
 * Copyright (C) 2017-2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "themes.h"
#include "preferences.h"
#include "io/resource.h"
#include "svg/svg-color.h"
#include <cstring>
#include <gio/gio.h>
#include <glibmm.h>
#include <gtkmm.h>
#include <map>
#include <utility>
#include <vector>
#include <regex>

namespace Inkscape {
namespace UI {

ThemeContext::ThemeContext()
{
}

/**
 * Inkscape fill gtk, taken from glib/gtk code with our own checks.
 */
void 
ThemeContext::inkscape_fill_gtk(const gchar *path, gtkThemeList &themes)
{
    const gchar *dir_entry;
    GDir *dir = g_dir_open(path, 0, nullptr);
    if (!dir)
        return;
    while ((dir_entry = g_dir_read_name(dir))) {
        gchar *filename = g_build_filename(path, dir_entry, "gtk-3.0", "gtk.css", nullptr);
        bool has_prefer_dark = false;
  
        Glib::ustring theme = dir_entry;
        gchar *filenamedark = g_build_filename(path, dir_entry, "gtk-3.0", "gtk-dark.css", nullptr);
        if (g_file_test(filenamedark, G_FILE_TEST_IS_REGULAR))
            has_prefer_dark = true;
        if (themes.find(theme) != themes.end() && !has_prefer_dark) {
            continue;
        }
        if (g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
            themes[theme] = has_prefer_dark;
        }
        g_free(filename);
        g_free(filenamedark);
    }
  
    g_dir_close(dir);
}

/**
 * Get available themes based on locations of gtk directories.
 */
std::map<Glib::ustring, bool> 
ThemeContext::get_available_themes()
{
    gtkThemeList themes;
    Glib::ustring theme = "";
    gchar *path;
    gchar **builtin_themes;
    guint i, j;
    const gchar *const *dirs;
  
    /* Builtin themes */
    builtin_themes = g_resources_enumerate_children("/org/gtk/libgtk/theme", G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
    for (i = 0; builtin_themes[i] != NULL; i++) {
        if (g_str_has_suffix(builtin_themes[i], "/")) {
            theme = builtin_themes[i];
            theme.resize(theme.size() - 1);
            Glib::ustring theme_path = "/org/gtk/libgtk/theme";
            theme_path += "/" + theme;
            gchar **builtin_themes_files =
                g_resources_enumerate_children(theme_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
            bool has_prefer_dark = false;
            if (builtin_themes_files != NULL) {
                for (j = 0; builtin_themes_files[j] != NULL; j++) {
                    Glib::ustring file = builtin_themes_files[j];
                    if (file == "gtk-dark.css") {
                        has_prefer_dark = true;
                    }
                }
            }
            g_strfreev(builtin_themes_files);
            themes[theme] = has_prefer_dark;
        }
    }

    g_strfreev(builtin_themes);

    path = g_build_filename(g_get_user_data_dir(), "themes", nullptr);
    inkscape_fill_gtk(path, themes);
    g_free(path);
  
    path = g_build_filename(g_get_home_dir(), ".themes", nullptr);
    inkscape_fill_gtk(path, themes);
    g_free(path);
  
    dirs = g_get_system_data_dirs();
    for (i = 0; dirs[i]; i++) {
        path = g_build_filename(dirs[i], "themes", nullptr);
        inkscape_fill_gtk(path, themes);
        g_free(path);
    }
    return themes;
}

Glib::ustring 
ThemeContext::get_symbolic_colors()
{
    Glib::ustring css_str;
    gchar colornamed[64];
    gchar colornamedsuccess[64];
    gchar colornamedwarning[64];
    gchar colornamederror[64];
    gchar colornamed_inverse[64];
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    Glib::ustring themeiconname = prefs->getString("/theme/iconTheme", prefs->getString("/theme/defaultIconTheme", ""));
    guint32 colorsetbase = 0x2E3436ff;
    guint32 colorsetbase_inverse;
    guint32 colorsetsuccess = 0x4AD589ff;
    guint32 colorsetwarning = 0xF57900ff;
    guint32 colorseterror = 0xCC0000ff;
    colorsetbase = prefs->getUInt("/theme/" + themeiconname + "/symbolicBaseColor", colorsetbase);
    colorsetsuccess = prefs->getUInt("/theme/" + themeiconname + "/symbolicSuccessColor", colorsetsuccess);
    colorsetwarning = prefs->getUInt("/theme/" + themeiconname + "/symbolicWarningColor", colorsetwarning);
    colorseterror = prefs->getUInt("/theme/" + themeiconname + "/symbolicErrorColor", colorseterror);
    sp_svg_write_color(colornamed, sizeof(colornamed), colorsetbase);
    sp_svg_write_color(colornamedsuccess, sizeof(colornamedsuccess), colorsetsuccess);
    sp_svg_write_color(colornamedwarning, sizeof(colornamedwarning), colorsetwarning);
    sp_svg_write_color(colornamederror, sizeof(colornamederror), colorseterror);
    colorsetbase_inverse = colorsetbase ^ 0xffffff00;
    sp_svg_write_color(colornamed_inverse, sizeof(colornamed_inverse), colorsetbase_inverse);
    css_str += "@define-color warning_color " + Glib::ustring(colornamedwarning) + ";\n";
    css_str += "@define-color error_color " + Glib::ustring(colornamederror) + ";\n";
    css_str += "@define-color success_color " + Glib::ustring(colornamedsuccess) + ";\n";
    /* ":not(.rawstyle) > image" works only on images in first level of widget container
    if in the future we use a complex widget with more levels and we dont want to tweak the color
    here, retaining default we can add more lines like ":not(.rawstyle) > > image" 
    if we not override the color we use defautt theme colors*/
    bool overridebasecolor = !prefs->getBool("/theme/symbolicDefaultBaseColors", true);
    if (overridebasecolor) {
        css_str += "#InkRuler,";
        css_str += ":not(.rawstyle) > image";
        css_str += "{color:";
        css_str += colornamed;
        css_str += ";}";
    }
    css_str += ".dark .forcebright :not(.rawstyle) > image,";
    css_str += ".dark .forcebright image:not(.rawstyle),";
    css_str += ".bright .forcedark :not(.rawstyle) > image,";
    css_str += ".bright .forcedark image:not(.rawstyle),";
    css_str += ".dark :not(.rawstyle) > image.forcebright,";
    css_str += ".dark image.forcebright:not(.rawstyle),";
    css_str += ".bright :not(.rawstyle) > image.forcedark,";
    css_str += ".bright image.forcedark:not(.rawstyle),";
    css_str += ".inverse :not(.rawstyle) > image,";
    css_str += ".inverse image:not(.rawstyle)";
    css_str += "{color:";
    if (overridebasecolor) {
        css_str += colornamed_inverse;
    } else {
        // we override base color in this special cases using inverse color
        css_str += "@theme_bg_color";
    }
    css_str += ";}";
    return css_str;
}

std::string sp_tweak_background_colors(std::string cssstring, double crossfade, double contrast, bool dark)
{
    static std::regex re_no_affect("(inherit|unset|initial|none|url)");
    static std::regex re_color("background-color( ){0,3}:(.*?);");
    static std::regex re_image("background-image( ){0,3}:(.*?\\)) *?;");
    std::string sub = "";
    std::smatch m;
    std::regex_search(cssstring, m, re_no_affect);
    if (m.size() == 0) {
        if (cssstring.find("background-color") != std::string::npos) {
            sub = "background-color:shade($2," + Glib::ustring::format(crossfade) + ");";
            cssstring = std::regex_replace(cssstring, re_color, sub);
        } else if (cssstring.find("background-image") != std::string::npos) {
            if (dark) {
                contrast = std::clamp((int)((contrast) * 27), 0, 100);
                sub = "background-image:cross-fade(" + Glib::ustring::format(contrast) + "% image(rgb(255,255,255)), image($2));";
            } else {
                contrast = std::clamp((int)((contrast) * 90), 0 , 100);
                sub = "background-image:cross-fade(" + Glib::ustring::format(contrast) + "% image(rgb(0,0,0)), image($2));";
            }
            cssstring = std::regex_replace(cssstring, re_image, sub);
        }
    } else {
        cssstring = "";
    }
    return cssstring;
}

static void
show_parsing_error(const Glib::RefPtr<const Gtk::CssSection>& section, const Glib::Error& error)
{
#ifndef NDEBUG
  g_warning("There is a warning parsing theme CSS:: %s", error.what().c_str());
#endif
}

// callback for a "narrow spinbutton" preference change
struct NarrowSpinbuttonObserver : Preferences::Observer {
    NarrowSpinbuttonObserver(const char* path, Glib::RefPtr<Gtk::CssProvider> provider):
        Preferences::Observer(path), _provider(std::move(provider)) {}

    void notify(Preferences::Entry const& new_val) override {
        auto screen = Gdk::Screen::get_default();
        if (new_val.getBool()) {
            Gtk::StyleContext::add_provider_for_screen(screen, _provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        }
        else {
            Gtk::StyleContext::remove_provider_for_screen(screen, _provider);
        }
    }

    Glib::RefPtr<Gtk::CssProvider> _provider;
};

/**
 * \brief Add our CSS style sheets
 * @param only_providers: Apply only the providers part, from inkscape preferences::theme change, no need to reaply
 */
void ThemeContext::add_gtk_css(bool only_providers, bool cached)
{
    using namespace Inkscape::IO::Resource;
    // Add style sheet (GTK3)
    auto const screen = Gdk::Screen::get_default();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gchar *gtkThemeName = nullptr;
    gchar *gtkIconThemeName = nullptr;
    Glib::ustring themeiconname;
    gboolean gtkApplicationPreferDarkTheme;
    GtkSettings *settings = gtk_settings_get_default();
    if (settings && !only_providers) {
        g_object_get(settings, "gtk-icon-theme-name", &gtkIconThemeName, nullptr);
        g_object_get(settings, "gtk-theme-name", &gtkThemeName, nullptr);
        g_object_get(settings, "gtk-application-prefer-dark-theme", &gtkApplicationPreferDarkTheme, nullptr);
        prefs->setBool("/theme/defaultPreferDarkTheme", gtkApplicationPreferDarkTheme);
        prefs->setString("/theme/defaultGtkTheme", Glib::ustring(gtkThemeName));
        prefs->setString("/theme/defaultIconTheme", Glib::ustring(gtkIconThemeName));
        Glib::ustring gtkthemename = prefs->getString("/theme/gtkTheme");
        if (gtkthemename != "") {
            g_object_set(settings, "gtk-theme-name", gtkthemename.c_str(), nullptr);
        }
        bool preferdarktheme = prefs->getBool("/theme/preferDarkTheme", false);
        g_object_set(settings, "gtk-application-prefer-dark-theme", preferdarktheme, nullptr);
        themeiconname = prefs->getString("/theme/iconTheme");
        // legacy cleanup
        if (themeiconname == prefs->getString("/theme/defaultIconTheme")) {
            prefs->setString("/theme/iconTheme", "");
        } else if (themeiconname != "") {
            g_object_set(settings, "gtk-icon-theme-name", themeiconname.c_str(), nullptr);
        }
    }

    g_free(gtkThemeName);
    g_free(gtkIconThemeName);

    int themecontrast = prefs->getInt("/theme/contrast", 10);
    if (!_contrastthemeprovider) {
        _contrastthemeprovider = Gtk::CssProvider::create();
        // We can uncomment this line to remove warnings and errors on the theme
        _contrastthemeprovider->signal_parsing_error().connect(sigc::ptr_fun(show_parsing_error));
    }
    static std::string cssstringcached = "";
    // we use contrast only if is setup (!= 10)
    if (themecontrast < 10) {
        Glib::ustring css_contrast = "";
        double contrast = (10 - themecontrast) / 30.0;
        double shade = 1 - contrast;
        const gchar *variant = nullptr;
        if (prefs->getBool("/theme/preferDarkTheme", false)) {
            variant = "dark";
        }
        bool dark = prefs->getBool("/theme/darkTheme", false);
        if (dark) {
            contrast *= 2.5;
            shade = 1 + contrast;
        }
        Glib::ustring current_theme = prefs->getString("/theme/gtkTheme", prefs->getString("/theme/defaultGtkTheme", ""));
        
        std::string cssstring = "";
        if (cached && !cssstringcached.empty()) {
            cssstring = cssstringcached;    
        } else {
            GtkCssProvider *current_themeprovider = gtk_css_provider_get_named(current_theme.c_str(), variant);
            cssstring = gtk_css_provider_to_string(current_themeprovider);
        }
        if (contrast) {
            std::string cssdefined = ""; 
            // we do this way to fix issue Inkscape#2345
            // windows seem crash if text length > 2000;
            
            std::istringstream f(cssstring);
            std::string line;    
            while (std::getline(f, line)) {
                // here we ignore most of class to parse because is in additive mode
                // so stiles not applied are set on previous context style
                if (line.find(";") != std::string::npos &&
                    line.find("background-image") == std::string::npos &&
                    line.find("background-color") == std::string::npos)
                {
                    continue;
                }
                cssdefined += sp_tweak_background_colors(line, shade, contrast, dark);
                cssdefined += "\n";
                if (!cached) {
                    cssstringcached += line;
                    cssstringcached += "\n";
                }
            }
            if (!cached) {
                // Split on curly brackets. Even tokens are selectors, odd are values.
                std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("[}{]", cssstringcached);
                cssstringcached = "";
                for (unsigned i = 0; i < tokens.size() - 1; i += 2) {
                    Glib::ustring selector = tokens[i];
                    Glib::ustring properties = "";
                    if ((i + 1) < tokens.size()) {
                        properties = tokens[i + 1];
                    }
                    if (properties.find(";") != Glib::ustring::npos) {
                        cssstringcached += selector;
                        cssstringcached += "{\n";
                        cssstringcached += properties;
                        cssstringcached += "}\n";
                    }
                }
            }
            cssstring = cssdefined;
        }
        if (!cssstring.empty()) {
            // Use c format allow parse with errors or warnings
            gtk_css_provider_load_from_data (_contrastthemeprovider->gobj(), cssstring.c_str(), -1, nullptr);
            Gtk::StyleContext::add_provider_for_screen(screen, _contrastthemeprovider, GTK_STYLE_PROVIDER_PRIORITY_SETTINGS);
        }
    } else {
        cssstringcached = "";
        if (_contrastthemeprovider) {
            Gtk::StyleContext::remove_provider_for_screen(screen, _contrastthemeprovider);
        }
    } 
    Glib::ustring style = get_filename(UIS, "style.css");
    if (!style.empty()) {
        if (_styleprovider) {
            Gtk::StyleContext::remove_provider_for_screen(screen, _styleprovider);
        }
        if (!_styleprovider) {
            _styleprovider = Gtk::CssProvider::create();
        }
        try {
            _styleprovider->load_from_path(style);
        } catch (const Gtk::CssProviderError &ex) {
            g_critical("CSSProviderError::load_from_path(): failed to load '%s'\n(%s)", style.c_str(),
                       ex.what().c_str());
        }
        Gtk::StyleContext::add_provider_for_screen(screen, _styleprovider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    Glib::ustring gtkthemename = prefs->getString("/theme/gtkTheme", prefs->getString("/theme/defaultGtkTheme", ""));
    gtkthemename += ".css";
    style = get_filename(UIS, gtkthemename.c_str(), false, true);
    if (!style.empty()) {
        if (_themeprovider) {
            Gtk::StyleContext::remove_provider_for_screen(screen, _themeprovider);
        }
        if (!_themeprovider) {
            _themeprovider = Gtk::CssProvider::create();
        }
        try {
            _themeprovider->load_from_path(style);
        } catch (const Gtk::CssProviderError &ex) {
            g_critical("CSSProviderError::load_from_path(): failed to load '%s'\n(%s)", style.c_str(),
                       ex.what().c_str());
        }
        Gtk::StyleContext::add_provider_for_screen(screen, _themeprovider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

    if (!_colorizeprovider) {
        _colorizeprovider = Gtk::CssProvider::create();
    }
    Glib::ustring css_str = "";
    if (prefs->getBool("/theme/symbolicIcons", false)) {
        css_str = get_symbolic_colors();
    }
    try {
        _colorizeprovider->load_from_data(css_str);
    } catch (const Gtk::CssProviderError &ex) {
        g_critical("CSSProviderError::load_from_data(): failed to load '%s'\n(%s)", css_str.c_str(), ex.what().c_str());
    }
    Gtk::StyleContext::add_provider_for_screen(screen, _colorizeprovider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // load small CSS snippet to style spinbuttons by removing excessive padding
    if (!_spinbuttonprovider) {
        _spinbuttonprovider = Gtk::CssProvider::create();
        Glib::ustring style = get_filename(UIS, "spinbutton.css");
        if (!style.empty()) {
            try {
                _spinbuttonprovider->load_from_path(style);
            } catch (const Gtk::CssProviderError &ex) {
                g_critical("CSSProviderError::load_from_path(): failed to load '%s'\n(%s)", style.c_str(), ex.what().c_str());
            }
        }
    }
    _spinbutton_observer = std::make_unique<NarrowSpinbuttonObserver>("/theme/narrowSpinButton", _spinbuttonprovider);
    // note: ideally we should remove the callback during destruction, but ThemeContext is never deleted
    prefs->addObserver(*_spinbutton_observer);
    // establish default value, so both this setting here and checkbox in preferences are in sync
    if (!prefs->getEntry(_spinbutton_observer->observed_path).isValid()) {
        prefs->setBool(_spinbutton_observer->observed_path, true);
    }
    _spinbutton_observer->notify(prefs->getEntry(_spinbutton_observer->observed_path));
}

/**
 * Check if current applied theme is dark or not by looking at style context.
 * This is important to check system default theme is dark or not
 * It only return True for dark and False for Bright. It does not apply any
 * property other than preferDarkTheme, so theme should be set before calling
 * this function as it may otherwise return outdated result.
 */
bool ThemeContext::isCurrentThemeDark(Gtk::Container *window)
{
    bool dark = false;
    if (window) {
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        Glib::ustring current_theme =
            prefs->getString("/theme/gtkTheme", prefs->getString("/theme/defaultGtkTheme", ""));
        auto settings = Gtk::Settings::get_default();
        if (settings) {
            settings->property_gtk_application_prefer_dark_theme() = prefs->getBool("/theme/preferDarkTheme", false);
        }
        dark = current_theme.find(":dark") != std::string::npos;
        // if theme is dark or we use contrast slider feature and have set preferDarkTheme we force the theme dark
        // and avoid color check, this fix a issue with low contrast themes bad switch of dark theme toggle
        dark = dark || (prefs->getInt("/theme/contrast", 10) != 10 && prefs->getBool("/theme/preferDarkTheme", false));
        if (!dark) {
            Glib::RefPtr<Gtk::StyleContext> stylecontext = window->get_style_context();
            Gdk::RGBA rgba;
            bool background_set = stylecontext->lookup_color("theme_bg_color", rgba);
            if (background_set && (0.299 * rgba.get_red() + 0.587 * rgba.get_green() + 0.114 * rgba.get_blue()) < 0.5) {
                dark = true;
            }
        }
    }
    return dark;
    
}


}
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
