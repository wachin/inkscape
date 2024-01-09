// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
/*
 *   bulia byak <buliabyak@users.sf.net>
 *   Tavmjong Bah <tavmjong@free.fr>  (Documentation)
 *
 * Functions to manipulate SPCSSAttr which is a class derived from Inkscape::XML::Node See
 * sp-css-attr.h and node.h
 *
 * SPCSSAttr is a special node type where the "attributes" are the properties in an element's style
 * attribute. For example, style="fill:blue;stroke:none" is stored in a List (Inkscape::Util:List)
 * where the key is the property (e.g. "fill" or "stroke") and the value is the property's value
 * (e.g. "blue" or "none"). An element's properties are manipulated by adding, removing, or
 * changing an item in the List. Utility functions are provided to go back and forth between the
 * two ways of representing properties (by a string or by a list).
 *
 * Use sp_repr_css_write_string to go from a property list to a style string.
 *
 */

#define SP_REPR_CSS_C

#include <cstring>
#include <string>
#include <sstream>

#include <glibmm/ustring.h>

#include "3rdparty/libcroco/src/cr-declaration.h"

#include "svg/css-ostringstream.h"

#include "xml/repr.h"
#include "xml/simple-document.h"
#include "xml/sp-css-attr.h"

using Inkscape::XML::SimpleNode;
using Inkscape::XML::Node;
using Inkscape::XML::NodeType;
using Inkscape::XML::Document;

struct SPCSSAttrImpl : public SimpleNode, public SPCSSAttr {
public:
    SPCSSAttrImpl(Document *doc)
    : SimpleNode(g_quark_from_static_string("css"), doc) {}
    SPCSSAttrImpl(SPCSSAttrImpl const &other, Document *doc)
    : SimpleNode(other, doc) {}

    NodeType type() const override { return Inkscape::XML::NodeType::ELEMENT_NODE; }

protected:
    SimpleNode *_duplicate(Document* doc) const override { return new SPCSSAttrImpl(*this, doc); }
};

static void sp_repr_css_add_components(SPCSSAttr *css, Node const *repr, gchar const *attr);

/**
 * Creates an empty SPCSSAttr (a class for manipulating CSS style properties).
 */
SPCSSAttr *sp_repr_css_attr_new()
{
    static Inkscape::XML::Document *attr_doc=nullptr;
    if (!attr_doc) {
        attr_doc = new Inkscape::XML::SimpleDocument();
    }
    return new SPCSSAttrImpl(attr_doc);
}

/**
 * Unreferences an SPCSSAttr (will be garbage collected if no references remain).
 */
void sp_repr_css_attr_unref(SPCSSAttr *css)
{
    g_assert(css != nullptr);
    Inkscape::GC::release((Node *) css);
}

/**
 * Creates a new SPCSSAttr with one attribute (i.e. style) copied from an existing repr (node). The
 * repr attribute data is in the form of a char const * string (e.g. fill:#00ff00;stroke:none). The
 * string is parsed by libcroco which returns a CRDeclaration list (a typical C linked list) of
 * properties and values. This list is then used to fill the attributes of the new SPCSSAttr.
 */
SPCSSAttr *sp_repr_css_attr(Node const *repr, gchar const *attr)
{
    g_assert(repr != nullptr);
    g_assert(attr != nullptr);

    SPCSSAttr *css = sp_repr_css_attr_new();
    sp_repr_css_add_components(css, repr, attr);
    return css;
}


/**
 * Adds an attribute to an existing SPCSAttr with the cascaded value including all parents.
 */ 
static void sp_repr_css_attr_inherited_recursive(SPCSSAttr *css, Node const *repr, gchar const *attr)
{
    const Node *parent = repr->parent();

    // read the ancestors from root down, using head recursion, so that children override parents
    if (parent) {
        sp_repr_css_attr_inherited_recursive(css, parent, attr);
    }
    sp_repr_css_add_components(css, repr, attr);
}

/**
 * Creates a new SPCSSAttr with one attribute whose value is determined by cascading.
 */
SPCSSAttr *sp_repr_css_attr_inherited(Node const *repr, gchar const *attr)
{
    g_assert(repr != nullptr);
    g_assert(attr != nullptr);

    SPCSSAttr *css = sp_repr_css_attr_new();

    sp_repr_css_attr_inherited_recursive(css, repr, attr);

    return css;
}

/**
 * Adds components (style properties) to an existing SPCSAttr from the specified attribute's data
 * (nominally a style attribute).
 * 
 */
static void sp_repr_css_add_components(SPCSSAttr *css, Node const *repr, gchar const *attr)
{
    g_assert(css != nullptr);
    g_assert(repr != nullptr);
    g_assert(attr != nullptr);

    char const *data = repr->attribute(attr);
    sp_repr_css_attr_add_from_string(css, data);
}

/**
 * Returns a character string of the value of a given style property or a default value if the
 * attribute is not found.
 */
char const *sp_repr_css_property(SPCSSAttr *css, gchar const *name, gchar const *defval)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);

    char const *attr = ((Node *)css)->attribute(name);
    return ( attr == nullptr
             ? defval
             : attr );
}

/**
 * Returns a character string of the value of a given style property or a default value if the
 * attribute is not found.
 */
Glib::ustring sp_repr_css_property(SPCSSAttr *css, Glib::ustring const &name, Glib::ustring const &defval)
{
    g_assert(css != nullptr);

    Glib::ustring retval = defval;
    char const *attr = ((Node *)css)->attribute(name.c_str());
    if (attr) {
        retval = attr;
    }

    return retval;
}

/**
 * Returns true if a style property is present and its value is unset.
 */
bool sp_repr_css_property_is_unset(SPCSSAttr *css, gchar const *name)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);

    char const *attr = ((Node *)css)->attribute(name);
    return (attr && !strcmp(attr, "inkscape:unset"));
}


/**
 * Set a style property to a new value (e.g. fill to #ffff00).
 */
void sp_repr_css_set_property(SPCSSAttr *css, gchar const *name, gchar const *value)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);

    ((Node *) css)->setAttribute(name, value);
}

/**
 * Set a style property to "inkscape:unset".
 */
void sp_repr_css_unset_property(SPCSSAttr *css, gchar const *name)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);

    ((Node *) css)->setAttribute(name, "inkscape:unset");
}

/**
 * Return the value of a style property if property define, or a default value if not.
 */
double sp_repr_css_double_property(SPCSSAttr *css, gchar const *name, double defval)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);
    
    return css->getAttributeDouble(name, defval);
}

/**
 * Set a style property to a new float value (e.g. opacity to 0.5).
 */
void sp_repr_css_set_property_double(SPCSSAttr *css, gchar const *name, double value)
{
    g_assert(css != nullptr);
    g_assert(name != nullptr);
    
    ((Node *) css)->setAttributeCssDouble(name, value);
}

/**
 * Write a style attribute string from a list of properties stored in an SPCSAttr object.
 */
void sp_repr_css_write_string(SPCSSAttr *css, Glib::ustring &str)
{
    str.clear();
    for (const auto & iter : css->attributeList())
    {
        if (iter.value && !strcmp(iter.value, "inkscape:unset")) {
            continue;
        }

        if (!str.empty()) {
            str.push_back(';');
        }

        str.append(g_quark_to_string(iter.key));
        str.push_back(':');
        str.append(iter.value); // Any necessary quoting to be done by calling routine.
    }
}

/**
 * Sets an attribute (e.g. style) to a string created from a list of style properties.
 */
void sp_repr_css_set(Node *repr, SPCSSAttr *css, gchar const *attr)
{
    g_assert(repr != nullptr);
    g_assert(css != nullptr);
    g_assert(attr != nullptr);

    Glib::ustring value;
    sp_repr_css_write_string(css, value);

    /*
     * If the new value is different from the old value, this will sometimes send a signal via
     * CompositeNodeObserver::notiftyAttributeChanged() which results in calling
     * SPObject::repr_attr_changed and thus updates the object's SPStyle. This update
     * results in another call to repr->setAttribute().
     */
    repr->setAttributeOrRemoveIfEmpty(attr, value);
}

/**
 * Loops through a List of style properties, printing key/value pairs.
 */
void sp_repr_css_print(SPCSSAttr *css)
{
    for ( const auto & attr: css->attributeList() )
    {
        gchar const * key = g_quark_to_string(attr.key);
        gchar const * val = attr.value;
        g_print("%s:\t%s\n",key,val);
    }
}

/**
 * Merges two SPCSSAttr's. Properties in src overwrite properties in dst if present in both.
 */
void sp_repr_css_merge(SPCSSAttr *dst, SPCSSAttr *src)
{
    g_assert(dst != nullptr);
    g_assert(src != nullptr);

    dst->mergeFrom(src, "");
}

/**
 * Merges style properties as parsed by libcroco into an existing SPCSSAttr.
 * libcroco converts all single quotes to double quotes, which needs to be
 * undone as we always use single quotes inside our 'style' strings since
 * double quotes are used outside: e.g.:
 *   style="font-family:'DejaVu Sans'"
 */
static void sp_repr_css_merge_from_decl(SPCSSAttr *css, CRDeclaration const *const decl)
{
    guchar *const str_value_unsigned = cr_term_to_string(decl->value);
    css->setAttribute(decl->property->stryng->str, reinterpret_cast<char const *>(str_value_unsigned));
    g_free(str_value_unsigned);
}

/**
 * Merges style properties as parsed by libcroco into an existing SPCSSAttr.
 *
 * \pre decl_list != NULL
 */
static void sp_repr_css_merge_from_decl_list(SPCSSAttr *css, CRDeclaration const *const decl_list)
{
    // read the decls from start to end, using tail recursion, so that latter declarations override
    // (Ref: http://www.w3.org/TR/REC-CSS2/cascade.html#cascading-order point 4.)
    // because sp_repr_css_merge_from_decl sets properties unconditionally
    sp_repr_css_merge_from_decl(css, decl_list);
    if (decl_list->next) {
        sp_repr_css_merge_from_decl_list(css, decl_list->next);
    }
}

/**
 * Use libcroco to parse a string for CSS properties and then merge
 * them into an existing SPCSSAttr.
 */
void sp_repr_css_attr_add_from_string(SPCSSAttr *css, gchar const *p)
{
    if (p != nullptr) {
        CRDeclaration *const decl_list
            = cr_declaration_parse_list_from_buf(reinterpret_cast<guchar const *>(p), CR_UTF_8);
        if (decl_list) {
            sp_repr_css_merge_from_decl_list(css, decl_list);
            cr_declaration_destroy(decl_list);
        }
    }
}

/**
 * Creates a new SPCSAttr with the values filled from a repr, merges in properties from the given
 * SPCSAttr, and then replaces that SPCSAttr with the new one. This is called, for example, for
 * each object in turn when a selection's style is updated via sp_desktop_set_style().
 */
void sp_repr_css_change(Node *repr, SPCSSAttr *css, gchar const *attr)
{
    g_assert(repr != nullptr);
    g_assert(css != nullptr);
    g_assert(attr != nullptr);

    SPCSSAttr *current = sp_repr_css_attr(repr, attr);
    sp_repr_css_merge(current, css);
    sp_repr_css_set(repr, current, attr);

    sp_repr_css_attr_unref(current);
}

void sp_repr_css_change_recursive(Node *repr, SPCSSAttr *css, gchar const *attr)
{
    g_assert(repr != nullptr);
    g_assert(css != nullptr);
    g_assert(attr != nullptr);

    sp_repr_css_change(repr, css, attr);

    for (Node *child = repr->firstChild(); child != nullptr; child = child->next()) {
        sp_repr_css_change_recursive(child, css, attr);
    }
}

/**
 * Return a new SPCSSAttr with all the properties found in the input SPCSSAttr unset.
 */
SPCSSAttr* sp_repr_css_attr_unset_all(SPCSSAttr *css)
{
    SPCSSAttr* css_unset = sp_repr_css_attr_new();
    for ( const auto & iter : css->attributeList() ) {
        sp_repr_css_set_property (css_unset, g_quark_to_string(iter.key), "inkscape:unset");
    }
    return css_unset;
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
