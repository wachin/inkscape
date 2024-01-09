// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief C facade to Inkscape::XML::Node
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2002 authors
 * Copyright (C) 2000-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SP_REPR_H
#define SEEN_SP_REPR_H

#include <vector>
#include <glibmm/quark.h>

#include "xml/node.h"
#include "xml/document.h"

#define SP_SODIPODI_NS_URI "http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
#define SP_BROKEN_SODIPODI_NS_URI "http://inkscape.sourceforge.net/DTD/sodipodi-0.dtd"
#define SP_INKSCAPE_NS_URI "http://www.inkscape.org/namespaces/inkscape"
#define SP_XLINK_NS_URI "http://www.w3.org/1999/xlink"
#define SP_SVG_NS_URI "http://www.w3.org/2000/svg"
#define SP_RDF_NS_URI "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define SP_CC_NS_URI "http://creativecommons.org/ns#"
#define SP_OLD_CC_NS_URI "http://web.resource.org/cc/"
#define SP_DC_NS_URI "http://purl.org/dc/elements/1.1/"

class SPCSSAttr;
class SVGLength;

namespace Inkscape {
namespace IO {
class Writer;
} // namespace IO
} // namespace Inkscape

namespace Geom {
class Point;
}

/* SPXMLNs */
char const *sp_xml_ns_uri_prefix(char const *uri, char const *suggested);
char const *sp_xml_ns_prefix_uri(char const *prefix);

Inkscape::XML::Document *sp_repr_document_new(char const *rootname);

/* IO */

Inkscape::XML::Document *sp_repr_read_file(char const *filename, char const *default_ns, bool xinclude = false);
Inkscape::XML::Document *sp_repr_read_mem(char const *buffer, int length, char const *default_ns);
void sp_repr_write_stream(Inkscape::XML::Node *repr, Inkscape::IO::Writer &out,
                          int indent_level,  bool add_whitespace, Glib::QueryQuark elide_prefix,
                          int inlineattrs, int indent,
                          char const *old_href_base = nullptr,
                          char const *new_href_base = nullptr);
Inkscape::XML::Document *sp_repr_read_buf (const Glib::ustring &buf, const char *default_ns);
Glib::ustring sp_repr_save_buf(Inkscape::XML::Document *doc);

// TODO convert to std::string
void sp_repr_save_stream(Inkscape::XML::Document *doc, FILE *to_file,
                         char const *default_ns = nullptr, bool compress = false,
                         char const *old_href_base = nullptr,
                         char const *new_href_base = nullptr);

bool sp_repr_save_file(Inkscape::XML::Document *doc, char const *filename, char const *default_ns=nullptr);
bool sp_repr_save_rebased_file(Inkscape::XML::Document *doc, char const *filename_utf8,
                               char const *default_ns,
                               char const *old_base, char const *new_base_filename);


/* CSS stuff */

SPCSSAttr *sp_repr_css_attr_new();
void sp_repr_css_attr_unref(SPCSSAttr *css);
SPCSSAttr *sp_repr_css_attr(Inkscape::XML::Node const *repr, char const *attr);
SPCSSAttr *sp_repr_css_attr_inherited(Inkscape::XML::Node const *repr, char const *attr);
SPCSSAttr *sp_repr_css_attr_unset_all(SPCSSAttr *css);

char const *sp_repr_css_property(SPCSSAttr *css, char const *name, char const *defval);
Glib::ustring sp_repr_css_property(SPCSSAttr *css, Glib::ustring const &name, Glib::ustring const &defval);
void sp_repr_css_set_property(SPCSSAttr *css, char const *name, char const *value);
void sp_repr_css_unset_property(SPCSSAttr *css, char const *name);
bool sp_repr_css_property_is_unset(SPCSSAttr *css, char const *name);
double sp_repr_css_double_property(SPCSSAttr *css, char const *name, double defval);
void sp_repr_css_set_property_double(SPCSSAttr *css, char const *name, double value);

void sp_repr_css_write_string(SPCSSAttr *css, Glib::ustring &str);
void sp_repr_css_set(Inkscape::XML::Node *repr, SPCSSAttr *css, char const *key);
void sp_repr_css_merge(SPCSSAttr *dst, SPCSSAttr *src);
void sp_repr_css_attr_add_from_string(SPCSSAttr *css, const char *data);
void sp_repr_css_change(Inkscape::XML::Node *repr, SPCSSAttr *css, char const *key);
void sp_repr_css_change_recursive(Inkscape::XML::Node *repr, SPCSSAttr *css, char const *key);
void sp_repr_css_print(SPCSSAttr *css);

/* Utility finctions */
/// Remove \a repr from children of its parent node.
inline void sp_repr_unparent(Inkscape::XML::Node *repr) {
    if (repr) {
        Inkscape::XML::Node *parent=repr->parent();
        if (parent) {
            parent->removeChild(repr);
        }
    }
}

bool sp_repr_is_meta_element(const Inkscape::XML::Node *node);

//c++-style comparison : returns (bool)(a<b)
int sp_repr_compare_position(Inkscape::XML::Node const *first, Inkscape::XML::Node const *second);
bool sp_repr_compare_position_bool(Inkscape::XML::Node const *first, Inkscape::XML::Node const *second);

// Searching
/**
 * @brief Find an element node with the given name.
 *
 * This function searches the descendants of the specified node depth-first for
 * the first XML node with the specified name.
 *
 * @param repr The node to start from
 * @param name The name of the element node to find
 * @param maxdepth Maximum search depth, or -1 for an unlimited depth
 * @return  A pointer to the matching Inkscape::XML::Node
 * @relatesalso Inkscape::XML::Node
 */
Inkscape::XML::Node *sp_repr_lookup_name(Inkscape::XML::Node *repr,
                                         char const *name,
                                         int maxdepth = -1);

Inkscape::XML::Node const *sp_repr_lookup_name(Inkscape::XML::Node const *repr,
                                               char const *name,
                                               int maxdepth = -1);

Glib::ustring sp_repr_lookup_content(Inkscape::XML::Node const *repr, gchar const *name, Glib::ustring otherwise = {});

std::vector<Inkscape::XML::Node const *> sp_repr_lookup_name_many(Inkscape::XML::Node const *repr,
                                                                  char const *name,
                                                                  int maxdepth = -1);

// Find an element node using an unique attribute.
Inkscape::XML::Node *sp_repr_lookup_child(Inkscape::XML::Node *repr,
                                          char const *key,
                                          char const *value);

// Find an element node using an unique attribute recursively.
Inkscape::XML::Node *sp_repr_lookup_descendant(Inkscape::XML::Node *repr,
                                               char const *key,
                                               char const *value);

Inkscape::XML::Node const *sp_repr_lookup_descendant(Inkscape::XML::Node const *repr,
                                                     char const *key,
                                                     char const *value);

// Find element nodes using a property value.
std::vector<Inkscape::XML::Node *> sp_repr_lookup_property_many(Inkscape::XML::Node *repr,
                                                                Glib::ustring const &property,
                                                                Glib::ustring const &value,
                                                                int maxdepth = -1);

inline Inkscape::XML::Node *sp_repr_document_first_child(Inkscape::XML::Document const *doc) {
    return const_cast<Inkscape::XML::Node *>(doc->firstChild());
}

inline bool sp_repr_is_def(Inkscape::XML::Node const *node) {
    return node->parent() != nullptr &&
        node->parent()->name() != nullptr &&
        strcmp("svg:defs", node->parent()->name()) == 0;
}

inline bool sp_repr_is_layer(Inkscape::XML::Node const *node) {
    return node->attribute("inkscape:groupmode") != nullptr &&
        strcmp("layer", node->attribute("inkscape:groupmode")) == 0;
}

/**
 * @brief Visit all descendants recursively.
 *
 * Traverse all descendants of node and call visitor on it.
 * Stop descending when visitor returns false
 *
 * @param node The root node to start visiting
 * @param visitor The visitor lambda (Node *) -> bool
 *                If visitor returns false child nodes of current node are not visited.
 * @relatesalso Inkscape::XML::Node
 */
template <typename Visitor>
void sp_repr_visit_descendants(Inkscape::XML::Node *node, Visitor visitor) {
    if (!visitor(node)) {
        return;
    }
    for (Inkscape::XML::Node *child = node->firstChild();
            child != nullptr;
            child = child->next()) {
        sp_repr_visit_descendants(child, visitor);
    }
}

/**
 * @brief Visit descendants of 2 nodes in parallel.
 * The assumption is that one a and b trees are the same in terms of structure (like one is
 * a duplicate of the other).
 *
 * @param a first node tree root
 * @param b second node tree root
 * @param visitor The visitor lambda (Node *, Node *) -> bool
 *                If visitor returns false child nodes are not visited.
 * @relatesalso Inkscape::XML::Node
 */
template <typename Visitor>
void sp_repr_visit_descendants(Inkscape::XML::Node *a, Inkscape::XML::Node *b, Visitor visitor) {
    if (!visitor(a, b)) {
        return;
    }
    for (Inkscape::XML::Node *ac = a->firstChild(), *bc = b->firstChild();
            ac != nullptr && bc != nullptr;
            ac = ac->next(), bc = bc->next()) {
        sp_repr_visit_descendants(ac, bc, visitor);
    }
}

#endif // SEEN_SP_REPR_H
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
