// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *   Abhishek Sharma
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "live_effects/parameter/item.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpe-clone-original.h"
#include <glibmm/i18n.h>

#include <gtkmm/button.h>
#include <gtkmm/label.h>

#include "bad-uri-exception.h"
#include "ui/widget/point.h"

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpe-clone-original.h"
#include "svg/svg.h"

#include "desktop.h"
#include "inkscape.h"
#include "message-stack.h"
#include "selection-chemistry.h"
#include "ui/icon-loader.h"
#include "xml/repr.h"
// clipboard support
#include "ui/clipboard.h"
// required for linking to other paths
#include "object/uri.h"

#include "ui/icon-names.h"

namespace Inkscape {

namespace LivePathEffect {

ItemParam::ItemParam( const Glib::ustring& label, const Glib::ustring& tip,
                      const Glib::ustring& key, Inkscape::UI::Widget::Registry* wr,
                      Effect* effect, const gchar * default_value)
    : Parameter(label, tip, key, wr, effect),
      changed(true),
      href(nullptr),
      ref( (SPObject*)effect->getLPEObj() )
{
    last_transform = Geom::identity();
    defvalue = g_strdup(default_value);
    ref_changed_connection = ref.changedSignal().connect(sigc::mem_fun(*this, &ItemParam::ref_changed));
}

ItemParam::~ItemParam()
{
    remove_link();
    g_free(defvalue);
}

void
ItemParam::param_set_default()
{
    param_readSVGValue(defvalue);
}

void
ItemParam::param_update_default(const gchar * default_value){
    defvalue = strdup(default_value);
}

void
ItemParam::param_set_and_write_default()
{
    param_write_to_repr(defvalue);
}

bool
ItemParam::param_readSVGValue(const gchar * strvalue)
{
    if (strvalue) {
        remove_link();
        if (strvalue[0] == '#') {
            if (href)
                g_free(href);
            href = g_strdup(strvalue);
            try {
                ref.attach(Inkscape::URI(href));
                //lp:1299948
                SPItem* i = ref.getObject();
                if (i) {
                    linked_modified_callback(i, SP_OBJECT_MODIFIED_FLAG);
                } // else: document still processing new events. Repr of the linked object not created yet.
            } catch (Inkscape::BadURIException &e) {
                g_warning("%s", e.what());
                ref.detach();
            }
        }
        emit_changed();
        return true;
    }

    return false;
}

Glib::ustring
ItemParam::param_getSVGValue() const
{
    if (href) {
        return href;
    }
    return "";
}

Glib::ustring
ItemParam::param_getDefaultSVGValue() const
{
    return defvalue;
}

Gtk::Widget *
ItemParam::param_newWidget()
{
    Gtk::HBox * _widget = Gtk::manage(new Gtk::HBox());
    Gtk::Image *pIcon = Gtk::manage(sp_get_icon_image("edit-clone", Gtk::ICON_SIZE_BUTTON));
    Gtk::Button * pButton = Gtk::manage(new Gtk::Button());
    Gtk::Label* pLabel = Gtk::manage(new Gtk::Label(param_label));
    static_cast<Gtk::HBox*>(_widget)->pack_start(*pLabel, true, true);
    pLabel->set_tooltip_text(param_tooltip);
    pButton->set_relief(Gtk::RELIEF_NONE);
    pIcon->show();
    pButton->add(*pIcon);
    pButton->show();
    pButton->signal_clicked().connect(sigc::mem_fun(*this, &ItemParam::on_link_button_click));
    static_cast<Gtk::HBox*>(_widget)->pack_start(*pButton, true, true);
    pButton->set_tooltip_text(_("Link to item on clipboard"));

    static_cast<Gtk::HBox*>(_widget)->show_all_children();

    return dynamic_cast<Gtk::Widget *> (_widget);
}

void
ItemParam::emit_changed()
{
    changed = true;
    signal_item_changed.emit();
}


void
ItemParam::addCanvasIndicators(SPLPEItem const*/*lpeitem*/, std::vector<Geom::PathVector> &hp_vec)
{
}


void
ItemParam::start_listening(SPObject * to)
{
    if ( to == nullptr ) {
        return;
    }
    linked_delete_connection = to->connectDelete(sigc::mem_fun(*this, &ItemParam::linked_delete));
    linked_modified_connection = to->connectModified(sigc::mem_fun(*this, &ItemParam::linked_modified));
    if (SP_IS_ITEM(to)) {
        linked_transformed_connection = SP_ITEM(to)->connectTransformed(sigc::mem_fun(*this, &ItemParam::linked_transformed));
    }
    linked_modified(to, SP_OBJECT_MODIFIED_FLAG); // simulate linked_modified signal, so that path data is updated
}

void
ItemParam::quit_listening()
{
    linked_modified_connection.disconnect();
    linked_delete_connection.disconnect();
    linked_transformed_connection.disconnect();
}

void
ItemParam::ref_changed(SPObject */*old_ref*/, SPObject *new_ref)
{
    quit_listening();
    if ( new_ref ) {
        start_listening(new_ref);
    }
}

void
ItemParam::remove_link()
{
    if (href) {
        ref.detach();
        g_free(href);
        href = nullptr;
    }
}

void
ItemParam::linked_delete(SPObject */*deleted*/)
{
    quit_listening();
    remove_link();
}

void ItemParam::linked_modified(SPObject *linked_obj, guint flags)
{
    linked_modified_callback(linked_obj, flags);
}

void ItemParam::linked_transformed(Geom::Affine const *rel_transf, SPItem *moved_item)
{
    linked_transformed_callback(rel_transf, moved_item);
}

void
ItemParam::linked_modified_callback(SPObject *linked_obj, guint /*flags*/)
{
    emit_changed();
    SP_OBJECT(param_effect->getLPEObj())->requestModified(SP_OBJECT_MODIFIED_FLAG);
    last_transform = Geom::identity();
}

void
ItemParam::linked_transformed_callback(Geom::Affine const *rel_transf, SPItem *moved_item)
{
    last_transform = *rel_transf;
    param_effect->getLPEObj()->requestModified(SP_OBJECT_MODIFIED_FLAG);
    if (dynamic_cast<Inkscape::LivePathEffect::LPECloneOriginal *>(param_effect->getLPEObj()->get_lpe())) {
        auto hreflist = param_effect->getLPEObj()->hrefList;
        SPDesktop * desktop = SP_ACTIVE_DESKTOP;
        if (desktop && hreflist.size()) {
            Inkscape::Selection *selection = desktop->getSelection();
            SPLPEItem *sp_lpe_item = dynamic_cast<SPLPEItem *>(*hreflist.begin());
            SPLPEItem *moved_lpeitem = dynamic_cast<SPLPEItem *>(moved_item);
            // here use moved item because sp_lpe_item never has optimized transforms because clone LPE
            if (sp_lpe_item && !selection->includes(sp_lpe_item) && moved_lpeitem && !last_transform.isTranslation()) {
                if (!moved_lpeitem->optimizeTransforms()) {
                    sp_lpe_item->transform *= last_transform.withoutTranslation();
                }
                sp_lpe_item->doWriteTransform(sp_lpe_item->transform);
            }
        }
    }
}


void
ItemParam::linkitem(Glib::ustring itemid)
{
    if (itemid.empty()) {
        return;
    }

    // add '#' at start to make it an uri.
    itemid.insert(itemid.begin(), '#');
    if ( href && strcmp(itemid.c_str(), href) == 0 ) {
        // no change, do nothing
        return;
    } else {
        // TODO:
        // check if id really exists in document, or only in clipboard document: if only in clipboard then invalid
        // check if linking to object to which LPE is applied (maybe delegated to PathReference

        param_write_to_repr(itemid.c_str());
        DocumentUndo::done(param_effect->getSPDoc(), SP_VERB_DIALOG_LIVE_PATH_EFFECT,
                           _("Link item parameter to path"));
    }
}

void
ItemParam::on_link_button_click()
{
    Inkscape::UI::ClipboardManager *cm = Inkscape::UI::ClipboardManager::get();
    const gchar * iid = cm->getFirstObjectID();
    if (!iid) {
        return;
    }
    
    Glib::ustring itemid(iid);
    linkitem(itemid);
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
