// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Live Path Effect editing dialog - implementation.
 */
/* Authors:
 *   Johan Engelen <j.b.c.engelen@utwente.nl>
 *   Steren Giannini <steren.giannini@gmail.com>
 *   Bastien Bouclet <bgkweb@gmail.com>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "livepatheffect-editor.h"

#include <gtkmm/expander.h>

#include "document-undo.h"
#include "document.h"
#include "inkscape.h"
#include "livepatheffect-add.h"
#include "path-chemistry.h"
#include "selection-chemistry.h"
#include "verbs.h"

#include "helper/action.h"
#include "ui/icon-loader.h"

#include "live_effects/effect.h"
#include "live_effects/lpeobject-reference.h"
#include "live_effects/lpeobject.h"

#include "object/sp-item-group.h"
#include "object/sp-path.h"
#include "object/sp-use.h"
#include "object/sp-text.h"

#include "ui/icon-names.h"
#include "ui/tools/node-tool.h"
#include "ui/widget/imagetoggler.h"

namespace Inkscape {
namespace UI {
namespace Dialog {


/*####################
 * Callback functions
 */


void LivePathEffectEditor::selectionChanged(Inkscape::Selection * selection)
{
    selection_changed_lock = true;
    lpe_list_locked = false;
    onSelectionChanged(selection);
    _on_button_release(nullptr); //to force update widgets
    selection_changed_lock = false;
}

void LivePathEffectEditor::selectionModified(Inkscape::Selection * selection, guint flags)
{
    lpe_list_locked = false;
    onSelectionChanged(selection);
}

static void lpe_style_button(Gtk::Button& btn, char const* iconName)
{
    GtkWidget *child = sp_get_icon_image(iconName, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_show( child );
    btn.add(*Gtk::manage(Glib::wrap(child)));
    btn.set_relief(Gtk::RELIEF_NONE);
}


/*
 * LivePathEffectEditor
 *
 * TRANSLATORS: this dialog is accessible via menu Path - Path Effect Editor...
 *
 */

LivePathEffectEditor::LivePathEffectEditor()
    : DialogBase("/dialogs/livepatheffect", "LivePathEffect")
    , lpe_list_locked(false)
    , effectwidget(nullptr)
    , status_label("", Gtk::ALIGN_CENTER)
    , effectcontrol_frame("")
    , button_add()
    , button_remove()
    , button_up()
    , button_down()
    , current_lpeitem(nullptr)
    , current_lperef(nullptr)
    , effectcontrol_vbox(Gtk::ORIENTATION_VERTICAL)
    , effectlist_vbox(Gtk::ORIENTATION_VERTICAL)
    , effectapplication_hbox(Gtk::ORIENTATION_HORIZONTAL, 4)
{
    set_spacing(4);

    //Add the TreeView, inside a ScrolledWindow, with the button underneath:
    scrolled_window.add(effectlist_view);
    scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    scrolled_window.set_shadow_type(Gtk::SHADOW_IN);
    scrolled_window.set_size_request(210, 70);

    effectcontrol_vbox.set_spacing(4);

    effectlist_vbox.pack_start(scrolled_window, Gtk::PACK_EXPAND_WIDGET);
    effectlist_vbox.pack_end(toolbar_hbox, Gtk::PACK_SHRINK);
    effectcontrol_eventbox.add_events(Gdk::BUTTON_RELEASE_MASK);
    effectcontrol_eventbox.signal_button_release_event().connect(sigc::mem_fun(*this, &LivePathEffectEditor::_on_button_release) );
    effectcontrol_eventbox.add(effectcontrol_vbox);
    effectcontrol_frame.add(effectcontrol_eventbox);

    button_add.set_tooltip_text(_("Add path effect"));
    lpe_style_button(button_add, INKSCAPE_ICON("list-add"));
    button_add.set_relief(Gtk::RELIEF_NONE);

    button_remove.set_tooltip_text(_("Delete current path effect"));
    lpe_style_button(button_remove, INKSCAPE_ICON("list-remove"));
    button_remove.set_relief(Gtk::RELIEF_NONE);

    button_up.set_tooltip_text(_("Raise the current path effect"));
    lpe_style_button(button_up, INKSCAPE_ICON("go-up"));
    button_up.set_relief(Gtk::RELIEF_NONE);

    button_down.set_tooltip_text(_("Lower the current path effect"));
    lpe_style_button(button_down, INKSCAPE_ICON("go-down"));
    button_down.set_relief(Gtk::RELIEF_NONE);

    // Add toolbar items to toolbar
    toolbar_hbox.set_layout (Gtk::BUTTONBOX_END);
    toolbar_hbox.add( button_add );
    toolbar_hbox.set_child_secondary( button_add , true);
    toolbar_hbox.add( button_remove );
    toolbar_hbox.set_child_secondary( button_remove , true);
    toolbar_hbox.add( button_up );
    toolbar_hbox.add( button_down );

    //Create the Tree model:
    effectlist_store = Gtk::ListStore::create(columns);
    effectlist_view.set_model(effectlist_store);
    effectlist_view.set_headers_visible(false);

    // Handle tree selections
    effectlist_selection = effectlist_view.get_selection();
    effectlist_selection->signal_changed().connect( sigc::mem_fun(*this, &LivePathEffectEditor::on_effect_selection_changed) );

    //Add the visibility icon column:
    Inkscape::UI::Widget::ImageToggler *eyeRenderer = Gtk::manage( new Inkscape::UI::Widget::ImageToggler(
        INKSCAPE_ICON("object-visible"), INKSCAPE_ICON("object-hidden")) );
    int visibleColNum = effectlist_view.append_column("is_visible", *eyeRenderer) - 1;
    eyeRenderer->signal_toggled().connect( sigc::mem_fun(*this, &LivePathEffectEditor::on_visibility_toggled) );
    eyeRenderer->property_activatable() = true;
    Gtk::TreeViewColumn* col = effectlist_view.get_column(visibleColNum);
    if ( col ) {
        col->add_attribute( eyeRenderer->property_active(), columns.col_visible );
    }

    //Add the effect name column:
    effectlist_view.append_column("Effect", columns.col_name);

    pack_start(effectlist_vbox, true, true);
    pack_start(status_label, false, false);
    pack_start(effectcontrol_frame, false, false);

    effectcontrol_frame.hide();
    selection_changed_lock = false;
    // connect callback functions to buttons
    button_add.signal_clicked().connect(sigc::mem_fun(*this, &LivePathEffectEditor::onAdd));
    button_remove.signal_clicked().connect(sigc::mem_fun(*this, &LivePathEffectEditor::onRemove));
    button_up.signal_clicked().connect(sigc::mem_fun(*this, &LivePathEffectEditor::onUp));
    button_down.signal_clicked().connect(sigc::mem_fun(*this, &LivePathEffectEditor::onDown));

    show_all_children();
}

LivePathEffectEditor::~LivePathEffectEditor()
{
    if (effectwidget) {
        effectcontrol_vbox.remove(*effectwidget);
        delete effectwidget;
        effectwidget = nullptr;
    }
}

bool LivePathEffectEditor::_on_button_release(GdkEventButton* button_event) {
    Glib::RefPtr<Gtk::TreeSelection> sel = effectlist_view.get_selection();
    if (sel->count_selected_rows () == 0) {
        return true;
    }
    Gtk::TreeModel::iterator it = sel->get_selected();
    LivePathEffect::LPEObjectReference * lperef = (*it)[columns.lperef];
    if (lperef && current_lpeitem && current_lperef != lperef) {
        if (lperef->getObject()) {
            LivePathEffect::Effect * effect = lperef->lpeobject->get_lpe();
            if (effect) {
                effect->refresh_widgets = true;
                showParams(*effect);
            }
        }
    }
    return true;
}

void
LivePathEffectEditor::showParams(LivePathEffect::Effect& effect)
{
    if (effectwidget && !effect.refresh_widgets) {
        return;
    }
    if (effectwidget) {
        effectcontrol_vbox.remove(*effectwidget);
        delete effectwidget;
        effectwidget = nullptr;
    }
    effectwidget = effect.newWidget();
    effectcontrol_frame.set_label(effect.getName());
    effectcontrol_vbox.pack_start(*effectwidget, true, true);

    button_remove.show();
    status_label.hide();
    effectcontrol_frame.show();
    effectcontrol_vbox.show_all_children();
    // fixme: add resizing of dialog
    effect.refresh_widgets = false;
}

void
LivePathEffectEditor::selectInList(LivePathEffect::Effect* effect)
{
    Gtk::TreeNodeChildren chi = effectlist_view.get_model()->children();
    for (Gtk::TreeIter ci = chi.begin() ; ci != chi.end(); ci++) {
        if (ci->get_value(columns.lperef)->lpeobject->get_lpe() == effect && effectlist_view.get_selection()) {
            effectlist_view.get_selection()->select(ci);
            break;
        }
    }
}


void
LivePathEffectEditor::showText(Glib::ustring const &str)
{
    if (effectwidget) {
        effectcontrol_vbox.remove(*effectwidget);
        delete effectwidget;
        effectwidget = nullptr;
    }

    status_label.show();
    status_label.set_label(str);

    effectcontrol_frame.hide();

    // fixme: do resizing of dialog ?
}

void
LivePathEffectEditor::set_sensitize_all(bool sensitive)
{
    //combo_effecttype.set_sensitive(sensitive);
    button_add.set_sensitive(sensitive);
    button_remove.set_sensitive(sensitive);
    effectlist_view.set_sensitive(sensitive);
    button_up.set_sensitive(sensitive);
    button_down.set_sensitive(sensitive);
}

void
LivePathEffectEditor::onSelectionChanged(Inkscape::Selection *sel)
{
    if (lpe_list_locked) {
        // this was triggered by selecting a row in the list, so skip reloading
        lpe_list_locked = false;
        return;
    }
    current_lpeitem = nullptr;
    effectlist_store->clear();

    if ( sel && !sel->isEmpty() ) {
        SPItem *item = sel->singleItem();
        if ( item ) {
            SPLPEItem *lpeitem = dynamic_cast<SPLPEItem *>(item);
            if ( lpeitem ) {
                effect_list_reload(lpeitem);
                current_lpeitem = lpeitem;
                set_sensitize_all(true);
                if ( lpeitem->hasPathEffect() ) {
                    Inkscape::LivePathEffect::Effect *lpe = lpeitem->getCurrentLPE();
                    if (lpe) {
                        showParams(*lpe);
                        lpe_list_locked = true;
                        selectInList(lpe);
                    } else {
                        showText(_("Unknown effect is applied"));
                    }
                } else {
                    showText(_("Click button to add an effect"));
                    button_remove.set_sensitive(false);
                    button_up.set_sensitive(false);
                    button_down.set_sensitive(false);
                }
            } else {
                SPUse *use = dynamic_cast<SPUse *>(item);
                if ( use ) {
                    // test whether linked object is supported by the CLONE_ORIGINAL LPE
                    SPItem *orig = use->get_original();
                    if ( dynamic_cast<SPShape *>(orig) ||
                         dynamic_cast<SPGroup *>(orig) ||
                         dynamic_cast<SPText *>(orig) )
                    {
                        // Note that an SP_USE cannot have an LPE applied, so we only need to worry about the "add effect" case.
                        set_sensitize_all(true);
                        showText(_("Click add button to convert clone"));
                        button_remove.set_sensitive(false);
                        button_up.set_sensitive(false);
                        button_down.set_sensitive(false);
                    } else {
                        showText(_("Select a path or shape"));
                        set_sensitize_all(false);
                    }
                } else {
                    showText(_("Select a path or shape"));
                    set_sensitize_all(false);
                }
            }
        } else {
            showText(_("Only one item can be selected"));
            set_sensitize_all(false);
        }
    } else {
        showText(_("Select a path or shape"));
        set_sensitize_all(false);
    }
}

/*
 * First clears the effectlist_store, then appends all effects from the effectlist.
 */
void
LivePathEffectEditor::effect_list_reload(SPLPEItem *lpeitem)
{
    effectlist_store->clear();

    PathEffectList effectlist = lpeitem->getEffectList();
    PathEffectList::iterator it;
    for( it = effectlist.begin() ; it!=effectlist.end(); ++it)
    {
        if ( !(*it)->lpeobject ) {
            continue;
        }

        if ((*it)->lpeobject->get_lpe()) {
            Gtk::TreeModel::Row row = *(effectlist_store->append());
            row[columns.col_name] = (*it)->lpeobject->get_lpe()->getName();
            row[columns.lperef] = *it;
            row[columns.col_visible] = (*it)->lpeobject->get_lpe()->isVisible();
        } else {
            Gtk::TreeModel::Row row = *(effectlist_store->append());
            row[columns.col_name] = _("Unknown effect");
            row[columns.lperef] = *it;
            row[columns.col_visible] = false;
        }
    }
}

/*########################################################################
# BUTTON CLICK HANDLERS    (callbacks)
########################################################################*/

// TODO:  factor out the effect applying code which can be called from anywhere. (selection-chemistry.cpp also needs it)
void LivePathEffectEditor::onAdd()
{
    auto selection = getSelection();
    if (selection && !selection->isEmpty() ) {
        SPItem *item = selection->singleItem();
        if (item) {
            if ( dynamic_cast<SPLPEItem *>(item) ) {
                // show effectlist dialog
                using Inkscape::UI::Dialog::LivePathEffectAdd;
                LivePathEffectAdd::show(getDesktop());
                if ( !LivePathEffectAdd::isApplied()) {
                    return;
                }

                const LivePathEffect::EnumEffectData<LivePathEffect::EffectType> *data =
                    LivePathEffectAdd::getActiveData();
                if (!data) {
                    return;
                }
                item = selection->singleItem(); // get new item

                LivePathEffect::Effect::createAndApply(data->key.c_str(), getDocument(), item);
                DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                                   _("Create and apply path effect"));

                lpe_list_locked = false;
                onSelectionChanged(selection);
            } else {
                SPUse *use = dynamic_cast<SPUse *>(item);
                if ( use ) {
                    // item is a clone. do not show effectlist dialog.
                    // convert to path, apply CLONE_ORIGINAL LPE, link it to the cloned path

                    // test whether linked object is supported by the CLONE_ORIGINAL LPE
                    SPItem *orig = use->get_original();
                    if ( dynamic_cast<SPShape *>(orig) ||
                         dynamic_cast<SPGroup *>(orig) ||
                         dynamic_cast<SPText *>(orig) )
                    {
                        // select original
                        selection->set(orig);

                        // delete clone but remember its id and transform
                        gchar *id = g_strdup(item->getRepr()->attribute("id"));
                        gchar *transform = g_strdup(item->getRepr()->attribute("transform"));
                        item->deleteObject(false);
                        item = nullptr;

                        // run sp_selection_clone_original_path_lpe
                        selection->cloneOriginalPathLPE(true);

                        SPItem *new_item = selection->singleItem();
                        // Check that the cloning was successful. We don't want to change the ID of the original referenced path!
                        if (new_item && (new_item != orig)) {
                            new_item->setAttribute("id", id);
                            new_item->setAttribute("transform", transform);
                        }
                        g_free(id);
                        g_free(transform);

                        /// \todo Add the LPE stack of the original path?

                        DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                                           _("Create and apply Clone original path effect"));

                        lpe_list_locked = false;
                        onSelectionChanged(selection);
                    }
                }
            }
        }
    }
}

void
LivePathEffectEditor::onRemove()
{
    auto selection = getSelection();
    if (selection && !selection->isEmpty() ) {
        SPItem *item = selection->singleItem();
        SPLPEItem *lpeitem  = dynamic_cast<SPLPEItem *>(item);
        if (lpeitem) {
            sp_lpe_item_update_patheffect(lpeitem, false, false);
            lpeitem->removeCurrentPathEffect(false);
            current_lperef = nullptr;
            DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Remove path effect"));
            lpe_list_locked = false;
            onSelectionChanged(selection);
        }
    }

}

void LivePathEffectEditor::onUp()
{
    auto selection = getSelection();
    if (selection && !selection->isEmpty() ) {
        SPItem *item = selection->singleItem();
        SPLPEItem *lpeitem = dynamic_cast<SPLPEItem *>(item);
        if (lpeitem) {
            Inkscape::LivePathEffect::Effect *lpe = lpeitem->getCurrentLPE();
            lpeitem->upCurrentPathEffect();
            DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Move path effect up") );
            effect_list_reload(lpeitem);
            if (lpe) {
                showParams(*lpe);
                lpe_list_locked = true;
                selectInList(lpe);
            }
        }
    }
}

void LivePathEffectEditor::onDown()
{
    auto selection = getSelection();
    if (selection && !selection->isEmpty() ) {
        SPItem *item = selection->singleItem();
        SPLPEItem *lpeitem = dynamic_cast<SPLPEItem *>(item);
        if ( lpeitem ) {
            Inkscape::LivePathEffect::Effect *lpe = lpeitem->getCurrentLPE();
            lpeitem->downCurrentPathEffect();
            DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT, _("Move path effect down") );
            effect_list_reload(lpeitem);
            if (lpe) {
                showParams(*lpe);
                lpe_list_locked = true;
                selectInList(lpe);
            }
        }
    }
}

void LivePathEffectEditor::on_effect_selection_changed()
{
    Glib::RefPtr<Gtk::TreeSelection> sel = effectlist_view.get_selection();
    if (sel->count_selected_rows () == 0) {
        button_remove.set_sensitive(false);
        return;
    }
    button_remove.set_sensitive(true);
    Gtk::TreeModel::iterator it = sel->get_selected();
    LivePathEffect::LPEObjectReference * lperef = (*it)[columns.lperef];

    if (lperef && current_lpeitem && current_lperef != lperef) {
        // The last condition ignore Gtk::TreeModel may occasionally be changed emitted when nothing has happened
        if (lperef->getObject()) {
            lpe_list_locked = true; // prevent reload of the list which would lose selection
            current_lpeitem->setCurrentPathEffect(lperef);
            current_lperef = lperef;
            LivePathEffect::Effect * effect = lperef->lpeobject->get_lpe();
            if (effect) {
                effect->refresh_widgets = true;
                showParams(*effect);
                // To reload knots and helper paths
                auto selection = getSelection();
                if (selection && !selection->isEmpty() && !selection_changed_lock) {
                    SPLPEItem *lpeitem = dynamic_cast<SPLPEItem *>(selection->singleItem());
                    if (lpeitem) {
                        selection->set(lpeitem);
                        Inkscape::UI::Tools::sp_update_helperpath(getDesktop());
                    }
                }
            }
        }
    }
}

void LivePathEffectEditor::on_visibility_toggled( Glib::ustring const& str )
{

    Gtk::TreeModel::Children::iterator iter = effectlist_view.get_model()->get_iter(str);
    Gtk::TreeModel::Row row = *iter;

    LivePathEffect::LPEObjectReference * lpeobjref = row[columns.lperef];

    if ( lpeobjref && lpeobjref->lpeobject->get_lpe() ) {
        bool newValue = !row[columns.col_visible];
        row[columns.col_visible] = newValue;
        /* FIXME: this explicit writing to SVG is wrong. The lpe_item should have a method to disable/enable an effect within its stack.
         * So one can call:  lpe_item->setActive(lpeobjref->lpeobject); */
        lpeobjref->lpeobject->get_lpe()->getRepr()->setAttribute("is_visible", newValue ? "true" : "false");
        auto selection = getSelection();
        if (selection && !selection->isEmpty() ) {
            SPItem *item = selection->singleItem();
            SPLPEItem *lpeitem  = dynamic_cast<SPLPEItem *>(item);
            if ( lpeitem ) {
                lpeobjref->lpeobject->get_lpe()->doOnVisibilityToggled(lpeitem);
            }
        }
        DocumentUndo::done(getDocument(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                            newValue ? _("Activate path effect") : _("Deactivate path effect"));
    }
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape

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
