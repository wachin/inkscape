// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Multiindex container for selection
 *
 * Authors:
 *   Adrian Boguszewski
 *   Marc Jeanmougin
 *
 * Copyright (C) 2016 Adrian Boguszewski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_PROTOTYPE_OBJECTSET_H
#define INKSCAPE_PROTOTYPE_OBJECTSET_H

#include <string>
#include <unordered_map>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/sub_range.hpp>
#include <boost/range/any_range.hpp>
#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>
#include <sigc++/connection.h>
#include <inkgc/gc-soft-ptr.h>
#include "sp-object.h"
#include "sp-item.h"
#include "sp-item-group.h"
#include "desktop.h"
#include "document.h"
#include "verbs.h"

enum BoolOpErrors {
    DONE,
    DONE_NO_PATH,
    DONE_NO_ACTION,
    ERR_TOO_LESS_PATHS_1,
    ERR_TOO_LESS_PATHS_2,
    ERR_NO_PATHS,
    ERR_Z_ORDER
};

// boolean operation
enum bool_op
{
  bool_op_union,		// A OR B
  bool_op_inters,		// A AND B
  bool_op_diff,			// A \ B
  bool_op_symdiff,  // A XOR B
  bool_op_cut,      // coupure (pleines)
  bool_op_slice     // coupure (contour)
};
typedef enum bool_op BooleanOp;

class SPBox3D;
class Persp3D;

namespace Inkscape {

namespace XML {
class Node;
}

struct hashed{};
struct random_access{};

struct is_item {
    bool operator()(SPObject* obj) {
        return SP_IS_ITEM(obj);
    }
};

struct is_group {
    bool operator()(SPObject* obj) {
        return SP_IS_GROUP(obj);
    }
};

struct object_to_item {
    typedef SPItem* result_type;
    SPItem* operator()(SPObject* obj) const {
        return SP_ITEM(obj);
    }
};

struct object_to_node {
    typedef XML::Node* result_type;
    XML::Node* operator()(SPObject* obj) const {
        return obj->getRepr();
    }
};

struct object_to_group {
    typedef SPGroup* result_type;
    SPGroup* operator()(SPObject* obj) const {
        return SP_GROUP(obj);
    }
};

typedef boost::multi_index_container<
        SPObject*,
        boost::multi_index::indexed_by<
                boost::multi_index::sequenced<>,
                boost::multi_index::random_access<
                        boost::multi_index::tag<random_access>>,
                boost::multi_index::hashed_unique<
                        boost::multi_index::tag<hashed>,
                        boost::multi_index::identity<SPObject*>>
        >> MultiIndexContainer;

typedef boost::any_range<
        SPObject*,
        boost::random_access_traversal_tag,
        SPObject* const&,
        std::ptrdiff_t> SPObjectRange;

class ObjectSet {
public:
    enum CompareSize {HORIZONTAL, VERTICAL, AREA};
    typedef decltype(MultiIndexContainer().get<random_access>() | boost::adaptors::filtered(is_item()) | boost::adaptors::transformed(object_to_item())) SPItemRange;
    typedef decltype(MultiIndexContainer().get<random_access>() | boost::adaptors::filtered(is_group()) | boost::adaptors::transformed(object_to_group())) SPGroupRange;
    typedef decltype(MultiIndexContainer().get<random_access>() | boost::adaptors::filtered(is_item()) | boost::adaptors::transformed(object_to_node())) XMLNodeRange;

    ObjectSet(SPDesktop* desktop): _desktop(desktop) {
        if (desktop)
            _document = desktop->getDocument(); 
    };
    ObjectSet(SPDocument* doc): _desktop(nullptr), _document(doc) {};
    ObjectSet(): _desktop(nullptr), _document(nullptr) {}; // Used in spray-tool.h.
    virtual ~ObjectSet();
    
    void setDocument(SPDocument* doc){
        _document = doc;
    }
    

    /**
     * Add an SPObject to the set of selected objects.
     *
     * @param obj the SPObject to add
     * @param nosignal true if no signals should be sent
     */
    bool add(SPObject* object, bool nosignal = false);

    /**
     * Add an XML node's SPObject to the set of selected objects.
     *
     * @param the xml node of the item to add
     */
    void add(XML::Node *repr);

    /**  Add items from an STL iterator range to the selection.
     *  \param from the begin iterator
     *  \param to the end iterator
     */
    template <typename InputIterator>
    void add(InputIterator from, InputIterator to) {
        for(auto it = from; it != to; ++it) {
            _add(*it);
        }
        _emitChanged();
    }

    /**
     * Removes an item from the set of selected objects.
     *
     * It is ok to call this method for an unselected item.
     *
     * @param item the item to unselect
     *
     * @return is success
     */
    bool remove(SPObject* object);

    /**
     * Returns true if the given object is selected.
     */
    bool includes(SPObject *object);

    /**
     * Set the selection to a single specific object.
     *
     * @param obj the object to select
     */
    void set(SPObject *object, bool persist_selection_context = false);
    void set(XML::Node *repr);
    /**
     * Unselects all selected objects.
     */
    void clear();

    /**
     * Returns size of the selection.
     */
    int size();

    /**
     * Returns true if no items are selected.
     */
    bool isEmpty();

    /**
     * Removes an item if selected, adds otherwise.
     *
     * @param item the item to unselect
     */
    void toggle(SPObject *obj);

    /**
     * Returns a single selected object.
     *
     * @return NULL unless exactly one object is selected
     */
    SPObject *single();

    /**
     * Returns a single selected item.
     *
     * @return NULL unless exactly one object is selected
     */
    SPItem *singleItem();

    /**
     * Returns the smallest item from this selection.
     */
    SPItem *smallestItem(CompareSize compare);

    /**
     * Returns the largest item from this selection.
     */
    SPItem *largestItem(CompareSize compare);

    /** Returns the list of selected objects. */
    SPObjectRange objects();

    /** Returns a range of selected SPItems. */
    SPItemRange items() {
        return SPItemRange(_container.get<random_access>()
           | boost::adaptors::filtered(is_item())
           | boost::adaptors::transformed(object_to_item()));
    };

    /** Returns a range of selected groups. */
    SPGroupRange groups() {
        return SPGroupRange (_container.get<random_access>()
            | boost::adaptors::filtered(is_group())
            | boost::adaptors::transformed(object_to_group()));
    }

    /** Returns a range of the xml nodes of all selected objects. */
    XMLNodeRange xmlNodes() {
        return XMLNodeRange(_container.get<random_access>()
                            | boost::adaptors::filtered(is_item())
                            | boost::adaptors::transformed(object_to_node()));
    }

    /**
     * Returns a single selected object's xml node.
     *
     * @return NULL unless exactly one object is selected
     */
    XML::Node *singleRepr();

    /**
     * The top-most item, or NULL if the selection is empty.
     */
    XML::Node *topRepr() const;

    /**
     * Selects exactly the specified objects.
     *
     * @param objs the objects to select
     */
    template <class T>
    typename boost::enable_if<boost::is_base_of<SPObject, T>, void>::type
    setList(const std::vector<T*> &objs) {
        _clear();
        addList(objs);
    }
    
    /**
     * Attempt to select all the items between two child items. Must have the same parent.
     *
     * @param obj_a - The first item, doesn't have to appear first in the list.
     * @param obj_b - The last item, doesn't have to appear last in the list (optional)
     *                If selection already contains one item, will select from-to that.
     *
     * @returns the number of items added.
     */
    int setBetween(SPObject *obj_a, SPObject *obj_b = nullptr);

    /**
     * Selects the objects with the same IDs as those in `list`.
     *
     * @todo How about adding `setIdList(std::vector<Glib::ustring> const &list)`
     * 
     * @param list the repr list to add
     */
    void setReprList(std::vector<XML::Node*> const &list);

    /**
     * Assign IDs to selected objects that don't have an ID attribute
     * Checks if the object's id attribute is NULL. If it is, assign it a unique ID
     */
    void enforceIds();

    /**
     * Adds the specified objects to selection, without deselecting first.
     *
     * @param objs the objects to select
     */
    template <class T>
    typename boost::enable_if<boost::is_base_of<SPObject, T>, void>::type
    addList(const std::vector<T*> &objs) {
        for (auto obj: objs) {
            if (!includes(obj)) {
                add(obj, true);
            }
        }
        _emitChanged();
    }

    /** Returns the bounding rectangle of the selection. */
    Geom::OptRect bounds(SPItem::BBoxType type) const;
    Geom::OptRect visualBounds() const;
    Geom::OptRect geometricBounds() const;

    /**
     * Returns either the visual or geometric bounding rectangle of the selection, based on the
     * preferences specified for the selector tool
     */
    Geom::OptRect preferredBounds() const;

    /* Returns the bounding rectangle of the selectionin document coordinates.*/
    Geom::OptRect documentBounds(SPItem::BBoxType type) const;

    /**
     * Returns the rotation/skew center of the selection.
     */
    std::optional<Geom::Point> center() const;

    /** Returns a list of all perspectives which have a 3D box in the current selection.
       (these may also be nested in groups) */
    std::list<Persp3D *> const perspList();

    /**
     * Returns a list of all 3D boxes in the current selection which are associated to @c
     * persp. If @c pers is @c NULL, return all selected boxes.
     */
    std::list<SPBox3D *> const box3DList(Persp3D *persp = nullptr);

    /**
     * Returns the desktop the selection is bound to
     *
     * @return the desktop the selection is bound to, or NULL if in console mode
     */
    SPDesktop *desktop() { return _desktop; }

    /**
     * Returns the document the selection is bound to
     *
     * @return the document the selection is bound to, or NULL if in console mode
     */
    SPDocument *document() { return _document; }

    //item groups operations
    //in selection-chemistry.cpp
    void deleteItems();
    void duplicate(bool suppressDone = false, bool duplicateLayer = false);
    void clone();

    /**
     * @brief Unlink all directly selected clones.
     * @param skip_undo If this is set to true the call to DocumentUndo::done is omitted.
     * @return True if anything was unlinked, otherwise false.
     */
    bool unlink(const bool skip_undo = false);
    /**
     * @brief Recursively unlink any clones present in the current selection,
     * including clones which are used to clip other objects, groups of clones etc.
     * @return true if anything was unlinked, otherwise false.
     */
    bool unlinkRecursive(const bool skip_undo = false, const bool force = false);
    void removeLPESRecursive(bool keep_paths);
    void relink();
    void cloneOriginal();
    void cloneOriginalPathLPE(bool allow_transforms = false);
    Inkscape::XML::Node* group();
    void popFromGroup();
    void ungroup(bool skip_undo = false);
    
    //z-order management
    //in selection-chemistry.cpp
    void stackUp(bool skip_undo = false);
    void raise(bool skip_undo = false);
    void raiseToTop(bool skip_undo = false);
    void stackDown(bool skip_undo = false);
    void lower(bool skip_undo = false);
    void lowerToBottom(bool skip_undo = false);
    void toNextLayer(bool skip_undo = false);
    void toPrevLayer(bool skip_undo = false);
    void toLayer(SPObject *layer, bool skip_undo = false);
    void toLayer(SPObject *layer, bool skip_undo, Inkscape::XML::Node *after);

    //clipboard management
    //in selection-chemistry.cpp
    void copy();
    void cut();
    void pasteStyle();
    void pasteSize(bool apply_x, bool apply_y);
    void pasteSizeSeparately(bool apply_x, bool apply_y);
    void pastePathEffect();
    
    //path operations
    //in path-chemistry.cpp
    void combine(bool skip_undo = false);
    void breakApart(bool skip_undo = false);
    void toCurves(bool skip_undo = false);
    void toLPEItems();
    void pathReverse();

    // path operations
    // in path/path-object-set.cpp
    bool strokesToPaths(bool legacy = false, bool skip_undo = false);
    bool simplifyPaths(bool skip_undo = false);

    // Boolean operations
    // in splivarot.cpp
    bool pathUnion(const bool skip_undo = false);
    bool pathIntersect(const bool skip_undo = false);
    bool pathDiff(const bool skip_undo = false);
    bool pathSymDiff(const bool skip_undo = false);
    bool pathCut(const bool skip_undo = false);
    bool pathSlice(const bool skip_undo = false);

    //Other path operations
    //in selection-chemistry.cpp
    void toMarker(bool apply = true);
    void toGuides();
    void toSymbol();
    void unSymbol();
    void tile(bool apply = true); //"Object to Pattern"
    void untile();
    void createBitmapCopy();
    void setMask(bool apply_clip_path, bool apply_to_layer = false, bool skip_undo = false);
    void editMask(bool clip);
    void unsetMask(const bool apply_clip_path, const bool skip_undo = false);
    void setClipGroup();
    
    // moves
    // in selection-chemistry.cpp
    void removeLPE();
    void removeFilter();
    void applyAffine(Geom::Affine const &affine, bool set_i2d=true,bool compensate=true, bool adjust_transf_center=true);
    void removeTransform();
    void setScaleAbsolute(double, double, double, double);
    void setScaleRelative(const Geom::Point&, const Geom::Scale&);
    void rotateRelative(const Geom::Point&, double);
    void skewRelative(const Geom::Point&, double, double);
    void moveRelative(const Geom::Point &move, bool compensate = true);
    void moveRelative(double dx, double dy);
    void rotate90(bool ccw);
    void rotate(double);
    void rotateScreen(double);
    void scale(double);
    void scaleScreen(double);
    void scaleTimes(double);
    void move(double dx, double dy);
    void moveScreen(double dx, double dy);
    
    // various
    void getExportHints(Glib::ustring &filename, float *xdpi, float *ydpi);
    bool fitCanvas(bool with_margins, bool skip_undo = false);
    void swapFillStroke();
    void fillBetweenMany();

protected:
    virtual void _connectSignals(SPObject* object) {};
    virtual void _releaseSignals(SPObject* object) {};
    virtual void _emitChanged(bool persist_selection_context = false) {}
    void _add(SPObject* object);
    void _clear();
    void _remove(SPObject* object);
    bool _anyAncestorIsInSet(SPObject *object);
    void _removeDescendantsFromSet(SPObject *object);
    void _removeAncestorsFromSet(SPObject *object);
    SPItem *_sizeistItem(bool sml, CompareSize compare);
    SPObject *_getMutualAncestor(SPObject *object);
    virtual void _add3DBoxesRecursively(SPObject *obj);
    virtual void _remove3DBoxesRecursively(SPObject *obj);

    MultiIndexContainer _container;
    GC::soft_ptr<SPDesktop> _desktop;
    GC::soft_ptr<SPDocument> _document;
    std::list<SPBox3D *> _3dboxes;
    std::unordered_map<SPObject*, sigc::connection> _releaseConnections;

private:
    BoolOpErrors pathBoolOp(bool_op bop, const bool skip_undo, const bool checked = false, const unsigned int verb = SP_VERB_NONE, const Glib::ustring description = "");
    void _disconnect(SPObject* object);

};

typedef ObjectSet::SPItemRange SPItemRange;
typedef ObjectSet::SPGroupRange SPGroupRange;
typedef ObjectSet::XMLNodeRange XMLNodeRange;

} // namespace Inkscape

#endif //INKSCAPE_PROTOTYPE_OBJECTSET_H

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
