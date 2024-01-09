// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Abhishek Sharma
 *   Sushant A.A. <sushant.co19@gmail.com>
 *
 * Copyright (C) 2002-2007 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */


#include "effect.h"

#include "execution-env.h"
#include "inkscape.h"
#include "timer.h"

#include "implementation/implementation.h"
#include "prefdialog/prefdialog.h"
#include "ui/view/view.h"
#include "inkscape-application.h"
#include "actions/actions-effect.h"

/* Inkscape::Extension::Effect */

namespace Inkscape {
namespace Extension {

Effect * Effect::_last_effect = nullptr;

/**
 * Adds effect to Gio::Actions
 *
 *  \c effect is Filter or Extension
 *  \c show_prefs is used to show preferences dialog
*/
void
action_effect (Effect* effect, bool show_prefs)
{
    auto doc = InkscapeApplication::instance()->get_active_view();
    if (effect->_workingDialog && show_prefs) {
        effect->prefs(doc);
    } else {
        effect->effect(doc);
    }
}

// Modifying string to get submenu id
std::string
action_menu_name (std::string menu)
{
    transform(menu.begin(), menu.end(), menu.begin(), ::tolower);
    for (auto &x:menu) {
        if (x==' ') {
            x = '-';
        }
    }
    return menu;
}

Effect::Effect (Inkscape::XML::Node *in_repr, Implementation::Implementation *in_imp, std::string *base_directory)
    : Extension(in_repr, in_imp, base_directory)
    , _menu_node(nullptr)
    , _prefDialog(nullptr)
{
    Inkscape::XML::Node * local_effects_menu = nullptr;

    // can't use document level because it is not defined
    static auto app = InkscapeApplication::instance();

    if (!app) {
        // This happens during tests.
        // std::cerr << "Effect::Effect:: no app!" << std::endl;
        return;
    }

    if (!Inkscape::Application::exists()) {
        return;
    }

    // This is a weird hack
    if (!strcmp(this->get_id(), "org.inkscape.filter.dropshadow"))
        return;

    bool hidden = false;

    no_doc = false;
    no_live_preview = false;

    // Setting initial value of description to name of action incase if there is no description
    Glib::ustring description  = get_name();

    if (repr != nullptr) {

        for (Inkscape::XML::Node *child = repr->firstChild(); child != nullptr; child = child->next()) {
            if (!strcmp(child->name(), INKSCAPE_EXTENSION_NS "effect")) {
                if (child->attribute("needs-document") && !strcmp(child->attribute("needs-document"), "false")) {
                    no_doc = true;
                }
                if (child->attribute("needs-live-preview") && !strcmp(child->attribute("needs-live-preview"), "false")) {
                    no_live_preview = true;
                }
                if (child->attribute("implements-custom-gui") && !strcmp(child->attribute("implements-custom-gui"), "true")) {
                    _workingDialog = false;
                    ignore_stderr = true;
                }
                for (Inkscape::XML::Node *effect_child = child->firstChild(); effect_child != nullptr; effect_child = effect_child->next()) {
                    if (!strcmp(effect_child->name(), INKSCAPE_EXTENSION_NS "effects-menu")) {
                        // printf("Found local effects menu in %s\n", this->get_name());
                        local_effects_menu = effect_child->firstChild();
                        if (effect_child->attribute("hidden") && !strcmp(effect_child->attribute("hidden"), "true")) {
                            hidden = true;
                        }
                    }
                    if (!strcmp(effect_child->name(), INKSCAPE_EXTENSION_NS "menu-tip") ||
                            !strcmp(effect_child->name(), INKSCAPE_EXTENSION_NS "_menu-tip")) {
                        // printf("Found local effects menu in %s\n", this->get_name());
                        description = effect_child->firstChild()->content();
                    }
                } // children of "effect"
                break; // there can only be one effect
            } // find "effect"
        } // children of "inkscape-extension"
    } // if we have an XML file

    std::string aid = std::string(get_id());
    _sanitizeId(aid);
    std::string action_id = "app." + aid;

    static auto gapp = InkscapeApplication::instance()->gtk_app();
    if (gapp) {
        // Might be in command line mode without GUI (testing).
        action = gapp->add_action( aid, sigc::bind<Effect*>(sigc::ptr_fun(&action_effect), this, true));
        action_noprefs = gapp->add_action( aid + ".noprefs", sigc::bind<Effect*>(sigc::ptr_fun(&action_effect), this, false));
    }

    if (!hidden) {
        // Submenu retrieval as a list of strings (to handle nested menus).
        std::list<Glib::ustring> sub_menu_list;
        get_menu(local_effects_menu, sub_menu_list);

        if (local_effects_menu && local_effects_menu->attribute("name") && !strcmp(local_effects_menu->attribute("name"), ("Filters"))) {

            std::vector<std::vector<Glib::ustring>>raw_data_filter =
                {{ action_id, get_name(), "Filters", description },
                 { action_id + ".noprefs", Glib::ustring(get_name()) + " " + _("(No preferences)"), "Filters (no prefs)", description }};
            app->get_action_extra_data().add_data(raw_data_filter);

        } else {

            std::vector<std::vector<Glib::ustring>>raw_data_effect =
                {{ action_id, get_name(), "Extensions", description },
                 { action_id + ".noprefs", Glib::ustring(get_name()) + " " + _("(No preferences)"), "Extensions (no prefs)", description }};
            app->get_action_extra_data().add_data(raw_data_effect);

            sub_menu_list.emplace_front("Effects");
        }
        
        // std::cout << " Effect: name:  " << get_name();
        // std::cout << "  id: " << aid.c_str();
        // std::cout << "  menu: ";
        // for (auto sub_menu : sub_menu_list) {
        //     std::cout << "|" << sub_menu.raw(); // Must use raw() as somebody has messed up encoding.
        // }
        // std::cout << "|" << std::endl;

        // Add submenu to effect data
        gchar *ellipsized_name = widget_visible_count() ? g_strdup_printf(_("%s..."), get_name()) : nullptr;
        Glib::ustring menu_name = ellipsized_name ? ellipsized_name : get_name();
        app->get_action_effect_data().add_data(aid, sub_menu_list, menu_name);
        g_free(ellipsized_name);
    }
}

/** Sanitizes the passed id in place. If an invalid character is found in the ID, a warning
 *  is printed to stderr. All invalid characters are replaced with an 'X'.
 */
void Effect::_sanitizeId(std::string &id)
{
    auto allowed = [] (char ch) {
        // Note: std::isalnum() is locale-dependent
        if ('A' <= ch && ch <= 'Z') return true;
        if ('a' <= ch && ch <= 'z') return true;
        if ('0' <= ch && ch <= '9') return true;
        if (ch == '.' || ch == '-') return true;
        return false;
    };

    // Silently replace any underscores with dashes.
    std::replace(id.begin(), id.end(), '_', '-');

    // Detect remaining invalid characters and print a warning if found
    bool errored = false;
    for (auto &ch : id) {
        if (!allowed(ch)) {
            if (!errored) {
                auto message = std::string{"Invalid extension action ID found: \""} + id + "\".";
                g_warn_message("Inkscape", __FILE__, __LINE__, "Effect::_sanitizeId()", message.c_str());
                errored = true;
            }
            ch = 'X';
        }
    }
}


void
Effect::get_menu (Inkscape::XML::Node * pattern, std::list<Glib::ustring>& sub_menu_list)
{
    if (!pattern) {
        return;
    }

    Glib::ustring merge_name;

    gchar const *menu_name = pattern->attribute("name");
    if (!menu_name) {
        menu_name = pattern->attribute("_name");
    }
    if (!menu_name) {
        return;
    }

    if (_translation_enabled) {
        merge_name = get_translation(menu_name);
    } else {
        merge_name = _(menu_name);
    }

    // Making sub menu string
    sub_menu_list.push_back(merge_name);

    get_menu(pattern->firstChild(), sub_menu_list);
}

void
Effect::deactivate()
{
    if (action)
        action->set_enabled(false);
    if (action_noprefs)
        action_noprefs->set_enabled(false);
    Extension::deactivate();
}

Effect::~Effect ()
{
    if (get_last_effect() == this)
        set_last_effect(nullptr);
    if (_menu_node) {
        if (_menu_node->parent()) {
            _menu_node->parent()->removeChild(_menu_node);
        }
        Inkscape::GC::release(_menu_node);
    }
    return;
}

bool
Effect::prefs (Inkscape::UI::View::View * doc)
{
    if (_prefDialog != nullptr) {
        _prefDialog->raise();
        return true;
    }

    if (!widget_visible_count()) {
        effect(doc);
        return true;
    }

    if (!loaded())
        set_state(Extension::STATE_LOADED);
    if (!loaded()) return false;

    Glib::ustring name = this->get_name();
    _prefDialog = new PrefDialog(name, nullptr, this);
    _prefDialog->show();

    return true;
}

/**
    \brief  The function that 'does' the effect itself
    \param  doc  The Inkscape::UI::View::View to do the effect on

    This function first insures that the extension is loaded, and if not,
    loads it.  It then calls the implementation to do the actual work.  It
    also resets the last effect pointer to be this effect.  Finally, it
    executes a \c SPDocumentUndo::done to commit the changes to the undo
    stack.
*/
void
Effect::effect (Inkscape::UI::View::View * doc)
{
    //printf("Execute effect\n");
    if (!loaded())
        set_state(Extension::STATE_LOADED);
    if (!loaded()) return;
    ExecutionEnv executionEnv(this, doc, nullptr, _workingDialog, true);
    execution_env = &executionEnv;
    timer->lock();
    executionEnv.run();
    if (executionEnv.wait()) {
        executionEnv.commit();
    } else {
        executionEnv.cancel();
    }
    timer->unlock();

    return;
}

/** \brief  Sets which effect was called last
    \param in_effect  The effect that has been called

    This function sets the static variable \c _last_effect

    If the \c in_effect variable is \c NULL then the last effect
    verb is made insensitive.
*/
void
Effect::set_last_effect (Effect * in_effect)
{
    _last_effect = in_effect;
    enable_effect_actions(InkscapeApplication::instance(), !!in_effect);
    return;
}

Inkscape::XML::Node *
Effect::find_menu (Inkscape::XML::Node * menustruct, const gchar *name)
{
    if (menustruct == nullptr) return nullptr;
    for (Inkscape::XML::Node * child = menustruct;
            child != nullptr;
            child = child->next()) {
        if (!strcmp(child->name(), name)) {
            return child;
        }
        Inkscape::XML::Node * firstchild = child->firstChild();
        if (firstchild != nullptr) {
            Inkscape::XML::Node *found = find_menu (firstchild, name);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}


Gtk::Box *
Effect::get_info_widget()
{
    return Extension::get_info_widget();
}

PrefDialog *
Effect::get_pref_dialog ()
{
    return _prefDialog;
}

void
Effect::set_pref_dialog (PrefDialog * prefdialog)
{
    _prefDialog = prefdialog;
    return;
}

} }  /* namespace Inkscape, Extension */

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
