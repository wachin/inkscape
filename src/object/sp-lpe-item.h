// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SP_LPE_ITEM_H_SEEN
#define SP_LPE_ITEM_H_SEEN

/** \file
 * Base class for live path effect items
 */
/*
 * Authors:
 *   Johan Engelen <j.b.c.engelen@ewi.utwente.nl>
 *   Bastien Bouclet <bgkweb@gmail.com>
 *
 * Copyright (C) 2008 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <list>
#include <string>
#include "sp-item.h"

class LivePathEffectObject;
class SPCurve;
class SPShape;
class SPDesktop;

namespace Inkscape{ 
    namespace Display {
        class TemporaryItem;
    }
    namespace LivePathEffect{
        class LPEObjectReference;
        class Effect;
    }
}

typedef std::list<Inkscape::LivePathEffect::LPEObjectReference *> PathEffectList;

class SPLPEItem : public SPItem {
public:
    SPLPEItem();
    ~SPLPEItem() override;

    int path_effects_enabled;

    PathEffectList* path_effect_list;
    std::list<sigc::connection> *lpe_modified_connection_list; // this list contains the connections for listening to lpeobject parameter changes

    Inkscape::LivePathEffect::LPEObjectReference* current_path_effect;
    std::vector<Inkscape::Display::TemporaryItem*> lpe_helperpaths;

    void replacePathEffects( std::vector<LivePathEffectObject const *> const &old_lpeobjs,
                             std::vector<LivePathEffectObject const *> const &new_lpeobjs );


    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
    void release() override;

    void set(SPAttr key, char const* value) override;

    void update(SPCtx* ctx, unsigned int flags) override;
    void modified(unsigned int flags) override;
    bool autoFlattenFix();
    void removeAllAutoFlatten();
    void cleanupAutoFlatten();
    void child_added(Inkscape::XML::Node* child, Inkscape::XML::Node* ref) override;
    void remove_child(Inkscape::XML::Node* child) override;

    Inkscape::XML::Node* write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, unsigned int flags) override;

    virtual void update_patheffect(bool write);
    bool optimizeTransforms();
    void notifyTransform(Geom::Affine const &postmul);
    bool performPathEffect(SPCurve *curve, SPShape *current, bool is_clip_or_mask = false);
    bool performOnePathEffect(SPCurve *curve, SPShape *current, Inkscape::LivePathEffect::Effect *lpe, bool is_clip_or_mask = false);
    bool pathEffectsEnabled() const;
    bool hasPathEffect() const;
    bool hasPathEffectOfType(int const type, bool is_ready = true) const;
    bool hasPathEffectOfTypeRecursive(int const type, bool is_ready = true) const;
    bool hasPathEffectRecursive() const;
    bool hasPathEffectOnClipOrMask(SPLPEItem * shape) const;
    bool hasPathEffectOnClipOrMaskRecursive(SPLPEItem * shape) const;
    size_t getLPEIndex(Inkscape::LivePathEffect::Effect* lpe) const;
    size_t countLPEOfType(int const type, bool inc_hidden = true, bool is_ready = true) const;
    size_t getLPEReferenceIndex(Inkscape::LivePathEffect::LPEObjectReference* lperef) const;
    Inkscape::LivePathEffect::Effect* getPathEffectOfType(int type);
    Inkscape::LivePathEffect::Effect const* getPathEffectOfType(int type) const;
    bool hasBrokenPathEffect() const;
    PathEffectList getEffectList();
    PathEffectList const getEffectList() const;

    void downCurrentPathEffect();
    void upCurrentPathEffect();
    Inkscape::LivePathEffect::LPEObjectReference* getCurrentLPEReference();
    Inkscape::LivePathEffect::Effect* getCurrentLPE();
    Inkscape::LivePathEffect::LPEObjectReference* getPrevLPEReference(Inkscape::LivePathEffect::LPEObjectReference* lperef);
    Inkscape::LivePathEffect::Effect* getPrevLPE(Inkscape::LivePathEffect::Effect* lpe);
    Inkscape::LivePathEffect::LPEObjectReference* getNextLPEReference(Inkscape::LivePathEffect::LPEObjectReference*);
    Inkscape::LivePathEffect::Effect* getNextLPE(Inkscape::LivePathEffect::Effect* lpe);
    bool setCurrentPathEffect(Inkscape::LivePathEffect::LPEObjectReference* lperef);
    void removeCurrentPathEffect(bool keep_paths);
    void removeAllPathEffects(bool keep_paths);
    void addPathEffect(std::string value, bool reset);
    void addPathEffect(LivePathEffectObject * new_lpeobj);
    void resetClipPathAndMaskLPE(bool fromrecurse = false);
    void applyToMask(SPItem* to, Inkscape::LivePathEffect::Effect *lpe = nullptr);
    void applyToClipPath(SPItem* to, Inkscape::LivePathEffect::Effect *lpe = nullptr);
    void applyToClipPathOrMask(SPItem * clip_mask, SPItem* to, Inkscape::LivePathEffect::Effect *lpe = nullptr);
    bool forkPathEffectsIfNecessary(unsigned int nr_of_allowed_users = 1, bool recursive = true);

    void editNextParamOncanvas(SPDesktop *dt);
};
void sp_lpe_item_update_patheffect (SPLPEItem *lpeitem, bool wholetree, bool write); // careful, class already has method with *very* similar name!
void sp_lpe_item_enable_path_effects(SPLPEItem *lpeitem, bool enable);
SPObject * sp_lpe_item_remove_autoflatten(SPItem *item, const gchar *id);

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_LPE_ITEM, SPLPEItem)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_LPE_ITEM, SPLPEItem)

#endif /* !SP_LPE_ITEM_H_SEEN */

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
