// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SPDocument manipulation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   MenTaLguY <mental@rydia.net>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2004-2005 MenTaLguY
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2012 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/** \class SPDocument
 * SPDocument serves as the container of both model trees (agnostic XML
 * and typed object tree), and implements all of the document-level
 * functionality used by the program. Many document level operations, like
 * load, save, print, export and so on, use SPDocument as their basic datatype.
 *
 * SPDocument implements undo and redo stacks and an id-based object
 * dictionary.  Thanks to unique id attributes, the latter can be used to
 * map from the XML tree back to the object tree.
 *
 * SPDocument performs the basic operations needed for asynchronous
 * update notification (SPObject ::modified virtual method), and implements
 * the 'modified' signal, as well.
 */


#define noSP_DOCUMENT_DEBUG_IDLE
#define noSP_DOCUMENT_DEBUG_UNDO

#include <vector>
#include <string>
#include <cstring>

#include <boost/range/adaptor/reversed.hpp>

#include <2geom/transforms.h>

#include "desktop.h"
#include "document-undo.h"
#include "event-log.h"
#include "file.h"
#include "id-clash.h"
#include "inkscape.h"
#include "inkscape-window.h"
#include "profile-manager.h"
#include "rdf.h"

#include "live_effects/effect.h"

#include "actions/actions-edit-document.h"
#include "actions/actions-undo-document.h"
#include "actions/actions-pages.h"

#include "display/drawing.h"
#include "display/control/canvas-item-drawing.h"
#include "ui/widget/canvas.h"

#include "3rdparty/adaptagrams/libavoid/router.h"

#include "3rdparty/libcroco/src/cr-sel-eng.h"
#include "3rdparty/libcroco/src/cr-selector.h"

#include "io/dir-util.h"
#include "layer-manager.h"
#include "page-manager.h"
#include "live_effects/lpeobject.h"
#include "object/persp3d.h"
#include "object/sp-defs.h"
#include "object/sp-factory.h"
#include "object/sp-namedview.h"
#include "object/sp-root.h"
#include "object/sp-symbol.h"
#include "object/sp-page.h"

#include "widgets/desktop-widget.h"

#include "xml/croco-node-iface.h"
#include "xml/rebase-hrefs.h"
#include "xml/simple-document.h"

using Inkscape::DocumentUndo;
using Inkscape::Util::unit_table;

// Higher number means lower priority.
#define SP_DOCUMENT_UPDATE_PRIORITY (G_PRIORITY_HIGH_IDLE - 2)

// Should have a lower priority than SP_DOCUMENT_UPDATE_PRIORITY,
// since we want it to happen when there are no more updates.
#define SP_DOCUMENT_REROUTING_PRIORITY (G_PRIORITY_HIGH_IDLE - 1)

bool sp_no_convert_text_baseline_spacing = false;

//gboolean sp_document_resource_list_free(gpointer key, gpointer value, gpointer data);

static gint doc_count = 0;
static gint doc_mem_count = 0;

static unsigned long next_serial = 0;

SPDocument::SPDocument() :
    keepalive(false),
    virgin(true),
    rdoc(nullptr),
    rroot(nullptr),
    root(nullptr),
    style_cascade(cr_cascade_new(nullptr, nullptr, nullptr)),
    document_filename(nullptr),
    document_base(nullptr),
    document_name(nullptr),
    actionkey(),
    object_id_counter(1),
    _router(std::make_unique<Avoid::Router>(Avoid::PolyLineRouting|Avoid::OrthogonalRouting)),
    current_persp3d(nullptr),
    current_persp3d_impl(nullptr),
    _parent_document(nullptr),
    _node_cache_valid(false),
    _activexmltree(nullptr)
{
    // This is kept here so that members are not accessed before they are initialized

    _event_log = std::make_unique<Inkscape::EventLog>(this);
    _selection = std::make_unique<Inkscape::Selection>(this);

    _desktop_activated_connection = INKSCAPE.signal_activate_desktop.connect(
                sigc::hide(sigc::bind(
                sigc::ptr_fun(&DocumentUndo::resetKey), this)));

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    if (!prefs->getBool("/options/yaxisdown", true)) {
        _doc2dt[3] = -1;
    }

    // Penalise libavoid for choosing paths with needless extra segments.
    // This results in much better looking orthogonal connector paths.
    _router->setRoutingPenalty(Avoid::segmentPenalty);

    _serial = next_serial++;

    sensitive = false;
    partial = nullptr;
    history_size = 0;
    seeking = false;

    // Once things are set, hook in the manager
    _profileManager = std::make_unique<Inkscape::ProfileManager>(this);

    // For undo/redo
    undoStackObservers.add(*_event_log);

    // XXX only for testing!
    undoStackObservers.add(console_output_undo_observer);
    _node_cache = std::deque<SPItem*>();

    // Actions
    action_group = Gio::SimpleActionGroup::create();
    add_actions_edit_document(this);
    add_actions_pages(this);
    add_actions_undo_document(this);

    _page_manager = std::make_unique<Inkscape::PageManager>(this);
}

SPDocument::~SPDocument() {
    destroySignal.emit();

    // kill/unhook this first
    _profileManager.reset();
    _desktop_activated_connection.disconnect();

    if (partial) {
        sp_repr_free_log(partial);
        partial = nullptr;
    }

    DocumentUndo::clearRedo(this);
    DocumentUndo::clearUndo(this);

    if (root) {
        root->releaseReferences();
        sp_object_unref(root);
        root = nullptr;
    }

    if (rdoc) Inkscape::GC::release(rdoc);

    /* Free resources */
    resources.clear();

    // This also destroys all attached stylesheets
    cr_cascade_unref(style_cascade);
    style_cascade = nullptr;

    if (document_name) {
        g_free(document_name);
        document_name = nullptr;
    }
    if (document_base) {
        g_free(document_base);
        document_base = nullptr;
    }
    if (document_filename) {
        g_free(document_filename);
        document_filename = nullptr;
    }

    modified_connection.disconnect();
    rerouting_connection.disconnect();

    if (keepalive) {
        inkscape_unref(INKSCAPE);
        keepalive = false;
    }

    if (this->current_persp3d_impl)
        delete this->current_persp3d_impl;
    this->current_persp3d_impl = nullptr;

    // This is at the end of the destructor, because preceding code adds new orphans to the queue
    collectOrphans();
}

gint SPDocument::get_new_doc_number() {
    return ++doc_count;
}

Inkscape::XML::Node *SPDocument::getReprNamedView()
{
    return sp_repr_lookup_name (rroot, "sodipodi:namedview");
}

/**
 * Get the namedview for this document, creates it if it's not found.
 *
 * @returns SPNamedView object, existing or created.
 */
SPNamedView *SPDocument::getNamedView()
{
    auto xml = getReprNamedView();
    if (!xml) {
        xml = rdoc->createElement("sodipodi:namedview");
        rroot->addChildAtPos(xml, 0);
        Inkscape::GC::release(xml);
    }
    return cast<SPNamedView> (getObjectByRepr(xml));
}

SPDefs *SPDocument::getDefs()
{
    if (!root) {
        return nullptr;
    }
    return root->defs;
}

Persp3D *SPDocument::getCurrentPersp3D() {
    // Check if current_persp3d is still valid
    std::vector<Persp3D*> plist;
    getPerspectivesInDefs(plist);
    for (auto & i : plist) {
        if (current_persp3d == i)
            return current_persp3d;
    }

    // If not, return the first perspective in defs (which may be NULL of none exists)
    current_persp3d = Persp3D::document_first_persp (this);

    return current_persp3d;
}

void SPDocument::setCurrentPersp3D(Persp3D * const persp) {
    current_persp3d = persp;
    //current_persp3d_impl = persp->perspective_impl;
}

void SPDocument::getPerspectivesInDefs(std::vector<Persp3D*> &list) const
{
    for (auto &c : root->defs->children) {
        if (auto p = cast<Persp3D>(&c)) {
            list.emplace_back(p);
        }
    }
}

/**
void SPDocument::initialize_current_persp3d()
{
    this->current_persp3d = Persp3D::document_first_persp(this);
    if (!this->current_persp3d) {
        this->current_persp3d = Persp3D::create_xml_element(this);
    }
}
**/

/**
 * Enables or disables document pages, usually used in import code.
 */
void SPDocument::setPages(bool enabled)
{
    if (enabled) {
        _page_manager->enablePages();
    } else {
        _page_manager->disablePages();
    }
}

/**
 * Remove pages in bulk using the integer range format "1,2,3-4" etc.
 *
 * @param page_nums - A string containing a range of page numbers
 * @param invert - Keep the pages and remove the rest.
 */
void SPDocument::prunePages(const std::string &page_nums, bool invert)
{
    auto pages = _page_manager->getPages(page_nums, invert);
    for (auto page : pages) {
        if (page->getId()) {
            ensureUpToDate();
            _page_manager->deletePage(page, true);
        }
    }    
}

void SPDocument::queueForOrphanCollection(SPObject *object) {
    g_return_if_fail(object != nullptr);
    g_return_if_fail(object->document == this);

    sp_object_ref(object, nullptr);
    _collection_queue.push_back(object);
}

void SPDocument::collectOrphans() {
    while (!_collection_queue.empty()) {
        std::vector<SPObject *> objects(_collection_queue);
        _collection_queue.clear();
        for (auto object : objects) {
            object->collectOrphan();
            sp_object_unref(object, nullptr);
        }
    }
}

SPDocument *SPDocument::createDoc(Inkscape::XML::Document *rdoc,
                                  gchar const *filename,
                                  gchar const *document_base,
                                  gchar const *document_name,
                                  bool keepalive,
                                  SPDocument *parent)
{
    SPDocument *document = new SPDocument();

    Inkscape::XML::Node *rroot = rdoc->root();

    document->keepalive = keepalive;

    document->rdoc = rdoc;
    document->rroot = rroot;
    if (parent) {
        document->_parent_document = parent;
        parent->_child_documents.push_back(document);
    }

    if (document->document_filename){
        g_free(document->document_filename);
        document->document_filename = nullptr;
    }
    if (document->document_base){
        g_free(document->document_base);
        document->document_base = nullptr;
    }
    if (document->document_name){
        g_free(document->document_name);
        document->document_name = nullptr;
    }
#ifndef _WIN32
    document->document_filename = prepend_current_dir_if_relative(filename);
#else
    // FIXME: it may be that prepend_current_dir_if_relative works OK on windows too, test!
    document->document_filename = filename? g_strdup(filename) : NULL;
#endif

    // base is simply the part of the path before filename; e.g. when running "inkscape ../file.svg" the base is "../"
    // which is why we use g_get_current_dir() in calculating the abs path above
    //This is NULL for a new document
    if (document_base) {
        document->document_base = g_strdup(document_base);
    } else {
        document->document_base = nullptr;
    }
    document->document_name = g_strdup(document_name);

    // Create SPRoot element
    const std::string typeString = NodeTraits::get_type_string(*rroot);
    SPObject* rootObj = SPFactory::createObject(typeString);
    document->root = cast<SPRoot>(rootObj);

    if (document->root == nullptr) {
    	// Node is not a valid root element
    	delete rootObj;

    	// fixme: what to do here?
    	throw;
    }

    // Recursively build object tree
    document->root->invoke_build(document, rroot, false);

    /* Eliminate obsolete sodipodi:docbase, for privacy reasons */
    rroot->removeAttribute("sodipodi:docbase");

    /* Eliminate any claim to adhere to a profile, as we don't try to */
    rroot->removeAttribute("baseProfile");

    // loading or creating namedview.
    auto nv = document->getNamedView();

    // Set each of the defaults in new or existing namedview (allows for per-attr overriding)
    nv->setDefaultAttribute("pagecolor",                 "/template/base/pagecolor", "#ffffff");
    nv->setDefaultAttribute("bordercolor",               "/template/base/bordercolor", "");
    nv->setDefaultAttribute("borderopacity",             "/template/base/borderopacity", "");
    nv->setDefaultAttribute("inkscape:showpageshadow",   "/template/base/pageshadow", "2");
    nv->setDefaultAttribute("inkscape:pageopacity",      "/template/base/pageopacity", "0.0");
    nv->setDefaultAttribute("inkscape:pagecheckerboard", "/template/base/pagecheckerboard", "0");
    nv->setDefaultAttribute("inkscape:deskcolor",        "/template/base/deskcolor", "#d1d1d1");

    // If no units are set in the document, try and guess them from the width/height
    // XXX Replace these calls with nv->setDocumentUnit(document->root->width.getUnit());
    if (document->root->width.isAbsolute()) {
        nv->setDefaultAttribute("inkscape:document-units", "", document->root->width.getUnit());
    } else if (document->root->height.isAbsolute()) {
        nv->setDefaultAttribute("inkscape:document-units", "", document->root->height.getUnit());
    }

    // Defs
    if (!document->root->defs) {
        Inkscape::XML::Node *r = rdoc->createElement("svg:defs");
        rroot->addChild(r, nullptr);
        Inkscape::GC::release(r);
        g_assert(document->root->defs);
    }

    /* Default RDF */
    rdf_set_defaults( document );

    if (keepalive) {
        inkscape_ref(INKSCAPE);
    }

    // Check if the document already has a perspective (e.g., when opening an existing
    // document). If not, create a new one and set it as the current perspective.
    document->setCurrentPersp3D(Persp3D::document_first_persp(document));
    if (!document->getCurrentPersp3D()) {
        //document->setCurrentPersp3D(Persp3D::create_xml_element (document));
        Persp3DImpl *persp_impl = new Persp3DImpl();
        document->setCurrentPersp3DImpl(persp_impl);
    }

    DocumentUndo::setUndoSensitive(document, true);

    // ************* Fix Document **************
    // Move to separate function?

    /** Fix baseline spacing (pre-92 files) **/
    if ( (!sp_no_convert_text_baseline_spacing)
         && sp_version_inside_range( document->root->version.inkscape, 0, 1, 0, 92 ) ) {
        sp_file_convert_text_baseline_spacing(document);
    }

    /** Fix font names in legacy documents (pre-92 files) **/
    if ( sp_version_inside_range( document->root->version.inkscape, 0, 1, 0, 92 ) ) {
        sp_file_convert_font_name(document);
    }

    /** Fix first line spacing in legacy documents (pre-1.0 files) **/
    if (sp_version_inside_range(document->root->version.inkscape, 0, 1, 1, 0)) {
        sp_file_fix_empty_lines(document);
    }

    /** Fix OSB (pre-1.1 files) **/
    if (sp_version_inside_range(document->root->version.inkscape, 0, 1, 1, 1)) {
        sp_file_fix_osb(document->getRoot());
    }

    /** Fix feComposite (pre-1.2 files) **/
    if (sp_version_inside_range(document->root->version.inkscape, 0, 1, 1, 2)) {
        sp_file_fix_feComposite(document->getRoot());
    }

    /** Fix 1.3.1 issue deleting the d attributes on shapes (stars, etc) **/
    std::string version_string = sp_version_to_string(document->root->version.inkscape); // end of version is stored as a string so we can't escape a string comparison
    if (version_string.size() > 4 && version_string.substr(0, 5) == "1.3.1") {
        document->getRoot()->updateRepr(SP_OBJECT_CHILD_MODIFIED_FLAG);
    }


    /** Fix dpi (pre-92 files). With GUI fixed in Inkscape::Application::fix_document. **/
    if ( !(INKSCAPE.use_gui()) && sp_version_inside_range( document->root->version.inkscape, 0, 1, 0, 92 ) ) {
        sp_file_convert_dpi(document);
    }

    // Update document level action settings
    // -- none available so far --

    return document;
}

/**
 * Create a copy of the document, useful for modifying during save & export.
 */
std::unique_ptr<SPDocument> SPDocument::copy() const
{
    // New SimpleDocument object where we will put all the same data
    Inkscape::XML::Document *new_rdoc = new Inkscape::XML::SimpleDocument();

    // Duplicate the svg root node AND any PI and COMMENT nodes, this should be put
    // into xml/simple-document.h at some point to fix it's duplicate implementation.
    for (Inkscape::XML::Node *child = rdoc->firstChild(); child; child = child->next()) {
        if (child) {
            // Get a new xml repr for the svg root node
            Inkscape::XML::Node *new_child = child->duplicate(new_rdoc);

            // Add the duplicated svg node as the document's rdoc
            new_rdoc->appendChild(new_child);
            Inkscape::GC::release(new_child);
        }
    }

    auto doc = createDoc(new_rdoc, document_filename, document_base, document_name, keepalive, nullptr);
    doc->_original_document = this;

    return std::unique_ptr<SPDocument>(doc);
}

/*
    Rebase the document with a new XMLDoc.
    passing the same file is like revert but keep history
*/
void SPDocument::rebase(const gchar * file, bool keep_namedview)
{
    if (file == nullptr)
    {
        g_warning("Error on rebase_doc: no file.");
        return;
    }
    Inkscape::XML::Document *new_xmldoc = sp_repr_read_file(file, SP_SVG_NS_URI);

    if (new_xmldoc) {
        rebase(new_xmldoc, keep_namedview);
    } else {
        g_warning("Error on rebase_doc: The file could not be parsed.");
    }
}

/*
    Rebase the document with de a new XMLDoc.
    \brief  A function to replace all the elements in a document
            by those from a new XML::Document.
            document and repinserts them into an emptied old document.
    \param  new_xmldoc  The root node to inject into.

    This function first deletes all the root attributes in the old document followed
    by copying all the root attributes from the new document to the old document.    

    Then, it copies all the element in the new XML::Document into the root of document.
    keep a diferent approach for namedview to not erase it and merge new value
*/
void SPDocument::rebase(Inkscape::XML::Document * new_xmldoc, bool keep_namedview)
{
    if (new_xmldoc == nullptr)
    {
        g_warning("Error on rebase_doc: NULL pointer input.");
        return;
    }
    emitReconstructionStart();
    Inkscape::XML::Document * origin_xmldoc = getReprDoc();
    Inkscape::XML::Node *namedview = nullptr;
    for ( Inkscape::XML::Node *child = origin_xmldoc->root()->lastChild() ; child != nullptr ;)
    {
        Inkscape::XML::Node *prevchild = child->prev();
        if (!g_strcmp0(child->name(),"sodipodi:namedview") && keep_namedview) {
            namedview = child;
        } else {
            origin_xmldoc->root()->removeChild(child);
        }
        child = prevchild;
    }
    for ( Inkscape::XML::Node *child = new_xmldoc->root()->firstChild() ; child != nullptr ; child = child->next() )
    {
        if (!g_strcmp0(child->name(),"sodipodi:namedview") && keep_namedview) {
            namedview->mergeFrom(child, "id", true, true);
        } else {
            Inkscape::XML::Node *new_child = child->duplicate(origin_xmldoc);
            origin_xmldoc->root()->appendChild(new_child);
            Inkscape::GC::release(new_child);
        }
    }
    // Copy svg root attributes
    for (const auto & iter : new_xmldoc->root()->attributeList()) {
        origin_xmldoc->root()->setAttribute(g_quark_to_string(iter.key), iter.value);
    }
    emitReconstructionFinish();
    new_xmldoc->release();
}

/*
    Rebase the document from data in disk
*/
void SPDocument::rebase(bool keep_namedview)
{
    if (document_filename == nullptr)
    {
        g_warning("Error on rebase_doc: NULL file");
        return;
    }
    rebase(document_filename, keep_namedview);
}

/**
 * Fetches a document and attaches it to the current document as a child href
 */
SPDocument *SPDocument::createChildDoc(std::string const &filename)
{
    SPDocument *parent = this;
    SPDocument *document = nullptr;

    while(parent != nullptr && parent->getDocumentFilename() != nullptr && document == nullptr) {
        // Check myself and any parents in the chain
        if(filename == parent->getDocumentFilename()) {
            document = parent;
            break;
        }
        // Then check children of those.
        boost::ptr_list<SPDocument>::iterator iter;
        for (iter = parent->_child_documents.begin();
          iter != parent->_child_documents.end(); ++iter) {
            if(filename == iter->getDocumentFilename()) {
                document = &*iter;
                break;
            }
        }
        parent = parent->_parent_document;
    }

    // Load a fresh document from the svg source.
    if(!document) {
        std::string path;
        if (Glib::path_is_absolute(filename)) {
            path = filename;
        } else {
            path = this->getDocumentBase() + filename;
        }
        document = createNewDoc(path.c_str(), false, false, this);
    }
    return document;
}

void SPDocument::update_lpobjs() {
    Inkscape::DocumentUndo::ScopedInsensitive tmp(this);
    sp_lpe_item_update_patheffect(getRoot(), false, true, true);
}

/**
 * Fetches document from filename, or creates new, if NULL; public document
 * appears in document list.
 */
SPDocument *SPDocument::createNewDoc(gchar const *filename, bool keepalive, bool make_new, SPDocument *parent)
{
    Inkscape::XML::Document *rdoc = nullptr;
    gchar *document_base = nullptr;
    gchar *document_name = nullptr;

    if (filename) {
        Inkscape::XML::Node *rroot;
        /* Try to fetch repr from file */
        rdoc = sp_repr_read_file(filename, SP_SVG_NS_URI);
        /* If file cannot be loaded, return NULL without warning */
        if (rdoc == nullptr) return nullptr;
        rroot = rdoc->root();
        /* If xml file is not svg, return NULL without warning */
        /* fixme: destroy document */
        if (strcmp(rroot->name(), "svg:svg") != 0) return nullptr;

        // Opening a template that points to a sister file should still work
        // this also includes tutorials which point to png files.
        document_base = g_path_get_dirname(filename);

        if (make_new) {
            filename = nullptr;
            document_name = g_strdup_printf(_("New document %d"), ++doc_count);
        } else {
            document_name = g_path_get_basename(filename);
            if (strcmp(document_base, ".") == 0) {
                g_free(document_base);
                document_base = nullptr;
            }
        }
    } else {
        if (make_new) {
            document_name = g_strdup_printf(_("Memory document %d"), ++doc_mem_count);
        }

        rdoc = sp_repr_document_new("svg:svg");
    }

    //# These should be set by now
    g_assert(document_name);

    SPDocument *doc = createDoc(rdoc, filename, document_base, document_name, keepalive, parent);

    g_free(document_base);
    g_free(document_name);

    return doc;
}

SPDocument *SPDocument::createNewDocFromMem(gchar const *buffer, gint length, bool keepalive,
                                            Glib::ustring const &filename)
{
    SPDocument *doc = nullptr;

    Inkscape::XML::Document *rdoc = sp_repr_read_mem(buffer, length, SP_SVG_NS_URI);
    if ( rdoc ) {
        // Only continue to create a non-null doc if it could be loaded
        Inkscape::XML::Node *rroot = rdoc->root();
        if ( strcmp(rroot->name(), "svg:svg") != 0 ) {
            // If xml file is not svg, return NULL without warning
            // TODO fixme: destroy document
        } else {
            Glib::ustring document_base = g_path_get_dirname(filename.c_str());
            if (document_base == ".")
                document_base = "";

            Glib::ustring document_name = Glib::ustring::compose( _("Memory document %1"), ++doc_mem_count );
            doc = createDoc(rdoc, filename.c_str(), document_base.c_str(), document_name.c_str(), keepalive, nullptr);
        }
    }

    return doc;
}

std::unique_ptr<SPDocument> SPDocument::doRef()
{
    Inkscape::GC::anchor(this);
    return std::unique_ptr<SPDocument>(this);
}
std::unique_ptr<SPDocument const> SPDocument::doRef() const
{
    return const_cast<SPDocument*>(this)->doRef();
}

/// guaranteed not to return nullptr
Inkscape::Util::Unit const* SPDocument::getDisplayUnit()
{
    if (auto nv = getNamedView()) {
        return nv->getDisplayUnit();
    }
    return unit_table.getUnit("px");
}

/// Sets document scale (by changing viewBox)
void SPDocument::setDocumentScale(double scaleX, double scaleY) {
    if (scaleX <= 0 || scaleY <= 0) {
        g_warning("%s: Invalid scale, has to be positive: %f, %f", __func__, scaleX, scaleY);
        return;
    }

    // since scale is doc size / viewbox size, then it follows that viewbox size is doc size / scale
    root->viewBox = Geom::Rect::from_xywh(
        root->viewBox.left(),
        root->viewBox.top(),
        root->width.computed  / scaleX,
        root->height.computed / scaleY);
    root->viewBox_set = true;
    root->updateRepr();
}

/// Sets document scale (by changing viewBox, x and y scaling equal)
void SPDocument::setDocumentScale(double scale) {
    setDocumentScale(scale, scale);
}

/// Returns document scale as defined by width/height (in pixels) and viewBox (real world to
/// user-units).
Geom::Scale SPDocument::getDocumentScale() const
{
    Geom::Scale scale;
    if( root->viewBox_set ) {
        double scale_x = 1.0;
        double scale_y = 1.0;
        if( root->viewBox.width() > 0.0 ) {
            scale_x = root->width.computed / root->viewBox.width();
        }
        if( root->viewBox.height() > 0.0 ) {
            scale_y = root->height.computed / root->viewBox.height();
        }
        scale = Geom::Scale(scale_x, scale_y);
    }
    // std::cout << "SPDocument::getDocumentScale():\n" << scale << std::endl;
    return scale;
}

// Avoid calling root->updateRepr() twice by combining setting width and height.
// (As done on every delete as clipboard calls this via fitToRect())
void SPDocument::setWidthAndHeight(const Inkscape::Util::Quantity &width, const Inkscape::Util::Quantity &height, bool changeSize)
{
    Inkscape::Util::Unit const *old_width_units = unit_table.getUnit("px");
    if (root->width.unit)
        old_width_units = unit_table.getUnit(root->width.unit);
    gdouble old_width_converted;  // old width converted to new units
    if (root->width.unit == SVGLength::PERCENT)
        old_width_converted = Inkscape::Util::Quantity::convert(root->width.computed, "px", width.unit);
    else
        old_width_converted = Inkscape::Util::Quantity::convert(root->width.value, old_width_units, width.unit);

    root->width.computed = width.value("px");
    root->width.value = width.quantity;
    root->width.unit = (SVGLength::Unit) width.unit->svgUnit();

    Inkscape::Util::Unit const *old_height_units = unit_table.getUnit("px");
    if (root->height.unit)
        old_height_units = unit_table.getUnit(root->height.unit);
    gdouble old_height_converted;  // old height converted to new units
    if (root->height.unit == SVGLength::PERCENT)
        old_height_converted = Inkscape::Util::Quantity::convert(root->height.computed, "px", height.unit);
    else
        old_height_converted = Inkscape::Util::Quantity::convert(root->height.value, old_height_units, height.unit);

    root->height.computed = height.value("px");
    root->height.value = height.quantity;
    root->height.unit = (SVGLength::Unit) height.unit->svgUnit();

    // viewBox scaled by relative change in page size (maintains document scale).
    if (root->viewBox_set && changeSize) {
        root->viewBox.setMax(Geom::Point(
        root->viewBox.left() + (root->width.value /  old_width_converted ) * root->viewBox.width(),
        root->viewBox.top()  + (root->height.value / old_height_converted) * root->viewBox.height()));
    }
    root->updateRepr();
}

Inkscape::Util::Quantity SPDocument::getWidth() const
{
    g_return_val_if_fail(this->root != nullptr, Inkscape::Util::Quantity(0.0, unit_table.getUnit("")));

    gdouble result = root->width.value;
    SVGLength::Unit u = root->width.unit;
    if (root->width.unit == SVGLength::PERCENT && root->viewBox_set) {
        result = root->viewBox.width();
        u = SVGLength::PX;
    }
    if (u == SVGLength::NONE) {
        u = SVGLength::PX;
    }
    return Inkscape::Util::Quantity(result, unit_table.getUnit(u));
}

void SPDocument::setWidth(const Inkscape::Util::Quantity &width, bool changeSize)
{
    Inkscape::Util::Unit const *old_width_units = unit_table.getUnit("px");
    if (root->width.unit)
        old_width_units = unit_table.getUnit(root->width.unit);
    gdouble old_width_converted;  // old width converted to new units
    if (root->width.unit == SVGLength::PERCENT)
        old_width_converted = Inkscape::Util::Quantity::convert(root->width.computed, "px", width.unit);
    else
        old_width_converted = Inkscape::Util::Quantity::convert(root->width.value, old_width_units, width.unit);

    root->width.computed = width.value("px");
    root->width.value = width.quantity;
    root->width.unit = (SVGLength::Unit) width.unit->svgUnit();

    if (root->viewBox_set && changeSize)
        root->viewBox.setMax(Geom::Point(root->viewBox.left() + (root->width.value / old_width_converted) * root->viewBox.width(), root->viewBox.bottom()));

    root->updateRepr();
}


Inkscape::Util::Quantity SPDocument::getHeight() const
{
    g_return_val_if_fail(this->root != nullptr, Inkscape::Util::Quantity(0.0, unit_table.getUnit("")));

    gdouble result = root->height.value;
    SVGLength::Unit u = root->height.unit;
    if (root->height.unit == SVGLength::PERCENT && root->viewBox_set) {
        result = root->viewBox.height();
        u = SVGLength::PX;
    }
    if (u == SVGLength::NONE) {
        u = SVGLength::PX;
    }
    return Inkscape::Util::Quantity(result, unit_table.getUnit(u));
}

void SPDocument::setHeight(const Inkscape::Util::Quantity &height, bool changeSize)
{
    Inkscape::Util::Unit const *old_height_units = unit_table.getUnit("px");
    if (root->height.unit)
        old_height_units = unit_table.getUnit(root->height.unit);
    gdouble old_height_converted;  // old height converted to new units
    if (root->height.unit == SVGLength::PERCENT)
        old_height_converted = Inkscape::Util::Quantity::convert(root->height.computed, "px", height.unit);
    else
        old_height_converted = Inkscape::Util::Quantity::convert(root->height.value, old_height_units, height.unit);

    root->height.computed = height.value("px");
    root->height.value = height.quantity;
    root->height.unit = (SVGLength::Unit) height.unit->svgUnit();

    if (root->viewBox_set && changeSize)
        root->viewBox.setMax(Geom::Point(root->viewBox.right(), root->viewBox.top() + (root->height.value / old_height_converted) * root->viewBox.height()));

    root->updateRepr();
}

const Geom::Affine &SPDocument::doc2dt() const
{
    if (root && !is_yaxisdown()) {
        _doc2dt[5] = root->height.computed;
    }

    return _doc2dt;
}

Geom::Rect SPDocument::getViewBox() const
{
    Geom::Rect viewBox;
    if (root->viewBox_set) {
        viewBox = root->viewBox;
    } else {
        viewBox = *preferredBounds();
    }
    return viewBox;
}

/**
 * Set default viewbox calculated from document properties.
 */
void SPDocument::setViewBox()
{
    setViewBox(Geom::Rect(0,
                          0,
                          getWidth().value(getDisplayUnit()),
                          getHeight().value(getDisplayUnit())));
}

void SPDocument::setViewBox(const Geom::Rect &viewBox)
{
    root->viewBox_set = true;
    root->viewBox = viewBox;
    root->updateRepr();
}

Geom::Point SPDocument::getDimensions() const
{
    return Geom::Point(getWidth().value("px"), getHeight().value("px"));
}

Geom::OptRect SPDocument::preferredBounds() const
{
    return Geom::OptRect( Geom::Point(0, 0), getDimensions() );
}

/**
 * Returns the position of the selected page or the preferredBounds()
 */
Geom::OptRect SPDocument::pageBounds()
{
    if (auto page = _page_manager->getSelected()) {
        return page->getDesktopRect();
    }
    return preferredBounds();
}

/**
 * Given a Geom::Rect that may, for example, correspond to the bbox of an object,
 * this function fits the canvas to that rect by resizing the canvas
 * and translating the document root into position.
 * \param rect fit document size to this, in document coordinates
 * \param (unused)
 */
void SPDocument::fitToRect(Geom::Rect const &rect, bool)
{
    using namespace Inkscape::Util;
    Unit const *nv_units = unit_table.getUnit("px");

    if (root->height.unit && (root->height.unit != SVGLength::PERCENT)) {
        nv_units = unit_table.getUnit(root->height.unit);
    }

    // 1. Calculate geometric transformations that must be applied to the drawing,
    //    pages, grids and guidelines to compensate for the changed origin.
    bool y_down = is_yaxisdown();
    double const old_height = root->height.computed;
    double const tr_x = -rect[Geom::X].min();
    double const tr_y_items = -rect[Geom::Y].min() * yaxisdir();
    double const tr_y_gadgets = y_down ? -rect[Geom::Y].min() : rect[Geom::Y].max() - old_height;

    // Item translation (in desktop coordinates)
    auto const item_translation = Geom::Translate(tr_x, tr_y_items);
    // Translation of grids and guides (in document coordinates)
    auto const gadget_translation = Geom::Translate(tr_x, tr_y_gadgets);

    // 2. Translate the guides.
    auto *nv = getNamedView();
    if (nv) {
        // It's important to do it BEFORE the document is resized, in order to ensure
        // the correct undo sequence. During undo, the document height will be restored
        // first, so the guides can then correctly recalculate their own position.
        // See https://gitlab.com/inkscape/inkscape/-/issues/615
        nv->translateGuides(gadget_translation);
    }

    // 3. Resize the document. This changes the SVG origin relative to the drawing.
    setWidthAndHeight(Quantity(Quantity::convert(rect.width(),  "px", nv_units), nv_units),
                      Quantity(Quantity::convert(rect.height(), "px", nv_units), nv_units));

    // 4. Translate everything to cancel out the change in the origin position.
    root->translateChildItems(item_translation);
    if (nv) {
        nv->translateGrids(gadget_translation);
        _page_manager->movePages(item_translation);

        // FIXME: The scroll state isn't restored during undo.
        nv->scrollAllDesktops(-tr_x, -tr_y_gadgets * yaxisdir());
    }
}

void SPDocument::setDocumentBase( gchar const* document_base )
{
    if (this->document_base) {
        g_free(this->document_base);
        this->document_base = nullptr;
    }
    if (document_base) {
        this->document_base = g_strdup(document_base);
    }
}

void SPDocument::do_change_filename(gchar const *const filename, bool const rebase)
{
    gchar *new_document_base = nullptr;
    gchar *new_document_name = nullptr;
    gchar *new_document_filename = nullptr;
    if (filename) {

#ifndef _WIN32
        new_document_filename = prepend_current_dir_if_relative(filename);
#else
        // FIXME: it may be that prepend_current_dir_if_relative works OK on windows too, test!
        new_document_filename = g_strdup(filename);
#endif

        new_document_base = g_path_get_dirname(new_document_filename);
        new_document_name = g_path_get_basename(new_document_filename);
    } else {
        new_document_name = g_strdup_printf(_("Unnamed document %d"), ++doc_count);
        new_document_base = nullptr;
        new_document_filename = nullptr;
    }

    // Update saveable repr attributes.
    Inkscape::XML::Node *repr = getReprRoot();

    // Changing filename in the document repr must not be not undoable.
    {
        DocumentUndo::ScopedInsensitive _no_undo(this);

        if (rebase) {
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            bool use_sodipodi_absref = prefs->getBool("/options/svgoutput/usesodipodiabsref", false);
            Inkscape::XML::rebase_hrefs(this, new_document_base, use_sodipodi_absref);
        }

        if (strncmp(new_document_name, "ink_ext_XXXXXX", 14))	// do not use temporary filenames
            repr->setAttribute("sodipodi:docname", new_document_name);
    }


    g_free(this->document_name);
    g_free(this->document_base);
    g_free(this->document_filename);
    this->document_name = new_document_name;
    this->document_base = new_document_base;
    this->document_filename = new_document_filename;

    // In case of new document the filename is nullptr
    gchar *new_filename = this->document_filename ? this->document_filename : this->document_name;
    this->filename_set_signal.emit(new_filename);
}

/**
 * Sets base, name and filename members of \a document.  Doesn't update
 * any relative hrefs in the document: thus, this is primarily for
 * newly-created documents.
 *
 * \see SPDocument::changeFilenameAndHrefs
 */
void SPDocument::setDocumentFilename(gchar const *filename)
{
    do_change_filename(filename, false);
}

/**
 * Changes the base, name and filename members of \a document, and updates any
 * relative hrefs in the document to be relative to the new base.
 */
void SPDocument::changeFilenameAndHrefs(gchar const *filename)
{
    do_change_filename(filename, true);
}

void SPDocument::bindObjectToId(char const *id, SPObject *object)
{
    GQuark idq = g_quark_from_string(id);

    if (object) {
        if(object->getId()) {
            iddef.erase(object->getId());
        }
        auto ret = iddef.emplace(id, object);
        g_assert(ret.second);
    } else {
        auto it = iddef.find(id);
        g_assert(it != iddef.end());
        iddef.erase(it);
    }

    auto pos = id_changed_signals.find(idq);
    if (pos != id_changed_signals.end()) {
        if (!pos->second.empty()) {
            pos->second.emit(object);
        } else { // discard unused signal
            id_changed_signals.erase(pos);
        }
    }
}

SPObject *SPDocument::getObjectById(std::string const &id) const
{
    if (iddef.empty()) return nullptr;

    if (auto rv = iddef.find(id); rv != iddef.end()) {
        return rv->second;
    } else if (_parent_document) {
        return _parent_document->getObjectById(id);
    } else if (_ref_document) {
        return _ref_document->getObjectById(id);
    }

    return nullptr;
}

SPObject *SPDocument::getObjectById(char const *id) const
{
    if (!id || iddef.empty()) return nullptr;

    if (auto rv = iddef.find(id); rv != iddef.end()) {
        return rv->second;
    } else if (_parent_document) {
        return _parent_document->getObjectById(id);
    } else if (_ref_document) {
        return _ref_document->getObjectById(id);
    }

    return nullptr;
}

SPObject *SPDocument::getObjectByHref(std::string const &href) const
{
    if (iddef.empty()) return nullptr;
    auto id = href.substr(1);
    return getObjectById(id);
}

SPObject *SPDocument::getObjectByHref(char const *href) const
{
    if (!href || href[0] == '\0') return nullptr;
    auto id = href + 1;
    return getObjectById(id);
}

static void _getObjectsByClassRecursive(Glib::ustring const &klass, SPObject *parent, std::vector<SPObject*> &objects)
{
    if (!parent) return;

    if (auto const temp = parent->getAttribute("class")) {
        std::istringstream classes(temp);
        Glib::ustring token;
        while (classes >> token) {
            // we can have multiple class
            if (classes.str() == " ") {
                token = "";
                continue;
            }
            if (token == klass) {
                objects.emplace_back(parent);
                break;
            }
        }
    }

    // Check children
    for (auto &child : parent->children) {
        _getObjectsByClassRecursive(klass, &child, objects);
    }
}

std::vector<SPObject*> SPDocument::getObjectsByClass(Glib::ustring const &klass) const
{
    if (klass.empty()) return {};
    std::vector<SPObject*> objects;
    _getObjectsByClassRecursive(klass, root, objects);
    return objects;
}

static void _getObjectsByElementRecursive(Glib::ustring const &element,
                                          SPObject *parent,
                                          std::vector<SPObject*> &objects,
                                          bool custom)
{
    if (!parent) return;

    Glib::ustring prefixed = custom ? "inkscape:" : "svg:";
    prefixed += element;
    if (parent->getRepr()->name() == prefixed) {
        objects.emplace_back(parent);
    }

    // Check children
    for (auto &child : parent->children) {
        _getObjectsByElementRecursive(element, &child, objects, custom);
    }
}

std::vector<SPObject*> SPDocument::getObjectsByElement(Glib::ustring const &element, bool custom) const
{
    if (element.empty()) return {};
    std::vector<SPObject*> objects;
    _getObjectsByElementRecursive(element, root, objects, custom);
    return objects;
}

static void _getObjectsBySelectorRecursive(SPObject *parent,
                                           CRSelEng *sel_eng, CRSimpleSel *simple_sel,
                                           std::vector<SPObject*> &objects)
{
    if (parent) {
        gboolean result = false;
        cr_sel_eng_matches_node(sel_eng, simple_sel, parent->getRepr(), &result);
        if (result) {
            objects.push_back(parent);
        }

        // Check children
        for (auto &child : parent->children) {
            _getObjectsBySelectorRecursive(&child, sel_eng, simple_sel, objects);
        }
    }
}

std::vector<SPObject*> SPDocument::getObjectsBySelector(Glib::ustring const &selector) const
{
    if (selector.empty()) return {};

    static CRSelEng *sel_eng = nullptr;
    if (!sel_eng) {
        sel_eng = cr_sel_eng_new(&Inkscape::XML::croco_node_iface);
    }

    auto cr_selector = cr_selector_parse_from_buf(reinterpret_cast<guchar const*>(selector.c_str()), CR_UTF_8);

    std::vector<SPObject*> objects;
    for (auto cur = cr_selector; cur; cur = cur->next) {
        if (cur->simple_sel) {
            _getObjectsBySelectorRecursive(root, sel_eng, cur->simple_sel, objects);
        }
    }
    cr_selector_destroy(cr_selector);
    return objects;
}

// Note: Despite appearances, this implementation is allocation-free thanks to SSO.
std::string SPDocument::generate_unique_id(char const *prefix)
{
    auto result = std::string(prefix);
    auto const prefix_len = result.size();

    while (true) {
        result.replace(prefix_len, std::string::npos, std::to_string(object_id_counter));

        if (!getObjectById(result)) {
            break;
        }

        ++object_id_counter;
    }

    return result;
}

void SPDocument::bindObjectToRepr(Inkscape::XML::Node *repr, SPObject *object)
{
    if (object) {
        auto ret = reprdef.emplace(repr, object);
        g_assert(ret.second);
    } else {
        auto it = reprdef.find(repr);
        g_assert(it != reprdef.end());
        reprdef.erase(it);
    }
}

SPObject *SPDocument::getObjectByRepr(Inkscape::XML::Node *repr) const
{
    if (!repr) return nullptr;
    auto it = reprdef.find(repr);
    return it == reprdef.end() ? nullptr : it->second;
}

/** Returns preferred document languages (from most to least preferred)
 *
 * This currently includes (in order):
 * - language set in RDF metadata
 * - languages suitable for system locale (influenced by Inkscape GUI locale preference)
 */
std::vector<Glib::ustring> SPDocument::getLanguages() const
{
    std::vector<Glib::ustring> document_languages;

    // get language from RDF
    gchar const *rdf_language = rdf_get_work_entity(this, rdf_find_entity("language"));
    if (rdf_language) {
        gchar *rdf_language_stripped = g_strstrip(g_strdup(rdf_language));
        if (strcmp(rdf_language_stripped, "") != 0) {
            document_languages.emplace_back(rdf_language_stripped);
        }
        g_free(rdf_language_stripped);
    }

    // add languages from parent document
    if (_parent_document) {
        auto parent_languages = _parent_document->getLanguages();

        // return parent languages directly if we aren't contributing any
        if (document_languages.empty()) {
            return parent_languages;
        }

        // otherwise append parent's languages to what we already have
        std::move(parent_languages.begin(), parent_languages.end(),
                  std::back_insert_iterator(document_languages));

        // don't add languages from locale; parent already did that
        return document_languages;
    }

    // get language from system locale (will also match the interface language preference as we set LANG accordingly)
    // TODO: This includes locales with encodings like "de_DE.UTF-8" - is this useful or should we skip these?
    // TODO: This includes the default "C" locale - is this useful or should we skip it?
    const gchar * const * names = g_get_language_names();
    for (int i=0; names[i]; i++) {
        document_languages.emplace_back(names[i]);
    }

    return document_languages;
}

/* Object modification root handler */

void SPDocument::requestModified()
{
    if (modified_connection.empty()) {
        modified_connection =
            Glib::signal_idle().connect(sigc::mem_fun(*this, &SPDocument::idle_handler),
                                        SP_DOCUMENT_UPDATE_PRIORITY);
    }

    if (rerouting_connection.empty()) {
        rerouting_connection =
            Glib::signal_idle().connect(sigc::mem_fun(*this, &SPDocument::rerouting_handler),
                                        SP_DOCUMENT_REROUTING_PRIORITY);
    }
}

void SPDocument::setupViewport(SPItemCtx *ctx)
{
    ctx->flags = 0;
    ctx->i2doc = Geom::identity();
    // Set up viewport in case svg has it defined as percentages
    if (root->viewBox_set) { // if set, take from viewBox
        ctx->viewport = root->viewBox;
    } else { // as a last resort, set size to A4
        ctx->viewport = Geom::Rect::from_xywh(0, 0, Inkscape::Util::Quantity::convert(210, "mm", "px"), Inkscape::Util::Quantity::convert(297, "mm", "px"));
    }
    ctx->i2vp = Geom::identity();
}

/**
 * Tries to update the document state based on the modified and
 * "update required" flags, and return true if the document has
 * been brought fully up to date.
 */
bool
SPDocument::_updateDocument(int update_flags)
{
    /* Process updates */
    if (this->root->uflags || this->root->mflags) {
        if (this->root->uflags) {
            SPItemCtx ctx;
            setupViewport(&ctx);

            DocumentUndo::ScopedInsensitive _no_undo(this);

            this->root->updateDisplay((SPCtx *)&ctx, update_flags);
        }
        this->_emitModified();
    }

    return !(this->root->uflags || this->root->mflags);
}


/**
 * Repeatedly works on getting the document updated, since sometimes
 * it takes more than one pass to get the document updated.  But it
 * usually should not take more than a few loops, and certainly never
 * more than 32 iterations.  So we bail out if we hit 32 iterations,
 * since this typically indicates we're stuck in an update loop.
 */
gint SPDocument::ensureUpToDate()
{
    // Bring the document up-to-date, specifically via the following:
    //   1a) Process all document updates.
    //   1b) When completed, process connector routing changes.
    //   2a) Process any updates resulting from connector reroutings.
    int counter = 32;
    for (unsigned int pass = 1; pass <= 2; ++pass) {
        // Process document updates.
        while (!_updateDocument(0)) {
            if (counter == 0) {
                g_warning("More than 32 iteration while updating document '%s'", document_filename);
                break;
            }
            counter--;
        }
        if (counter == 0)
        {
            break;
        }

        // After updates on the first pass we get libavoid to process all the
        // changed objects and provide new routings.  This may cause some objects
            // to be modified, hence the second update pass.
        if (pass == 1) {
            _router->processTransaction();
        }
    }

    // Remove handlers
    modified_connection.disconnect();
    rerouting_connection.disconnect();

    return (counter > 0);
}

/**
 * An idle handler to update the document.  Returns true if
 * the document needs further updates.
 */
bool
SPDocument::idle_handler()
{
    bool status = !_updateDocument(0); // method TRUE if it does NOT need further modification, so invert
    if (!status) {
        modified_connection.disconnect();
    }
    return status;
}

/**
 * An idle handler to reroute connectors in the document.
 */
bool
SPDocument::rerouting_handler()
{
    // Process any queued movement actions and determine new routings for
    // object-avoiding connectors.  Callbacks will be used to update and
    // redraw affected connectors.
    _router->processTransaction();

    // We don't need to handle rerouting again until there are further
    // diagram updates.
    return false;
}

static bool is_within(Geom::Rect const &area, Geom::Rect const &box)
{
    return area.contains(box);
}

static bool overlaps(Geom::Rect const &area, Geom::Rect const &box)
{
    return area.intersects(box);
}

/**
 * Return a vector list of items in a given area.
 *
 * @param s The returned list
 * @param group The starting group
 * @param dkey The display control group to traverse
 * @param area Area in document coordinates
 * @param test A function called for each item's bbox
 * @param take_hidden (false) picks hidden items
 * @param take_insensitive (false) picks insensitive items
 * @param take_groups (true) doesn't tranverse into groups
 * @param enter_groups (false) traverse into regular groups
 * @param enter_layers (true) traverse into layer groups
 */
static std::vector<SPItem*> &find_items_in_area(std::vector<SPItem*> &s,
                                                SPGroup *group, unsigned int dkey,
                                                Geom::Rect const &area,
                                                bool (*test)(Geom::Rect const &, Geom::Rect const &),
                                                bool take_hidden = false,
                                                bool take_insensitive = false,
                                                bool take_groups = true,
                                                bool enter_groups = false,
                                                bool enter_layers = true)
{
    g_return_val_if_fail(group, s);

    for (auto& o: group->children) {
        if (auto item = cast<SPItem>(&o)) {
            if (!take_insensitive && item->isLocked()) {
                continue;
            }

            if (!take_hidden && item->isHidden()) {
                continue;
            }

            if (auto childgroup = cast<SPGroup>(item)) {
                bool is_layer = childgroup->effectiveLayerMode(dkey) == SPGroup::LAYER;
                if ((enter_layers && is_layer) || (enter_groups)) {
                    s = find_items_in_area(s, childgroup, dkey, area, test, take_hidden, take_insensitive, take_groups, enter_groups, enter_layers);
                }
                if (!take_groups || (enter_layers && is_layer)) {
                    continue;
                }
            }
            Geom::OptRect box = item->documentVisualBounds();
            if (box && test(area, *box)) {
                s.push_back(item);
            }
        }
    }
    return s;
}

SPItem *SPDocument::getItemFromListAtPointBottom(unsigned dkey, SPGroup *group, std::vector<SPItem*> const &list, Geom::Point const &p, bool take_insensitive)
{
    if (!group) {
        return nullptr;
    }

    double const delta = Inkscape::Preferences::get()->getDouble("/options/cursortolerance/value", 1.0);
    std::optional<bool> outline;

    for (auto &c: group->children) {
        if (auto item = cast<SPItem>(&c)) {
            if (auto di = item->get_arenaitem(dkey)) {
                if (!outline) {
                    if (auto cid = di->drawing().getCanvasItemDrawing()) {
                        auto canvas = cid->get_canvas();
                        outline = canvas->canvas_point_in_outline_zone(p - canvas->get_pos());
                    }
                }
                if (di->pick(p, delta, Inkscape::DrawingItem::PICK_STICKY | outline.value_or(false) * Inkscape::DrawingItem::PICK_OUTLINE) && (take_insensitive || item->isVisibleAndUnlocked(dkey))) {
                    if (std::find(list.begin(), list.end(), item) != list.end()) {
                        return item;
                    }
                }
            }

            if (auto group = cast<SPGroup>(item)) {
                if (auto ret = getItemFromListAtPointBottom(dkey, group, list, p, take_insensitive)) {
                    return ret;
                }
            }
        }
    }

    return nullptr;
}

/**
Turn the SVG DOM into a flat list of nodes that can be searched from top-down.
The list can be persisted, which improves "find at multiple points" speed.
*/
// TODO: study add `gboolean with_groups = false` as parameter.
void SPDocument::build_flat_item_list(unsigned int dkey, SPGroup *group, gboolean into_groups) const
{
    for (auto& o: group->children) {
        if (!is<SPItem>(&o)) {
            continue;
        }

        if (is<SPGroup>(&o) && (cast<SPGroup>(&o)->effectiveLayerMode(dkey) == SPGroup::LAYER || into_groups)) {
            build_flat_item_list(dkey, cast<SPGroup>(&o), into_groups);
        } else {
            auto child = cast<SPItem>(&o);
            if (child->isVisibleAndUnlocked(dkey)) {
                _node_cache.push_front(child);
            }
        }
    }
}

/**
Returns the items from the descendants of group (recursively) which are at the
point p, or NULL if none. Honors into_groups on whether to recurse into non-layer
groups or not. Honors take_insensitive on whether to return insensitive items.
If upto != NULL, then if item upto is encountered (at any level), stops searching
upwards in z-order and returns what it has found so far (i.e. the found items are
guaranteed to be lower than upto). Requires a list of nodes built by build_flat_item_list.
If items_count > 0, it'll return the topmost (in z-order) items_count items.
 */
static std::vector<SPItem*> find_items_at_point(std::deque<SPItem*> const &nodes, unsigned dkey,
                                                Geom::Point const &p, int items_count = 0, SPItem *upto = nullptr)
{
    double const delta = Inkscape::Preferences::get()->getDouble("/options/cursortolerance/value", 1.0);
    std::optional<bool> outline;

    std::vector<SPItem*> result;

    bool seen_upto = !upto;
    for (auto node : nodes) {
        if (!seen_upto) {
            if (node == upto) {
                seen_upto = true;
            }
            continue;
        }
        if (auto di = node->get_arenaitem(dkey)) {
            if (!outline) {
                if (auto cid = di->drawing().getCanvasItemDrawing()) {
                    auto canvas = cid->get_canvas();
                    outline = canvas->canvas_point_in_outline_zone(p - canvas->get_pos());
                }
            }
            if (di->pick(p, delta, Inkscape::DrawingItem::PICK_STICKY | outline.value_or(false) * Inkscape::DrawingItem::PICK_OUTLINE)) {
                result.emplace_back(node);
                if (--items_count == 0) {
                    break;
                }
            }
        }
    }

    return result;
}

static SPItem *find_item_at_point(std::deque<SPItem*> const &nodes, unsigned dkey, Geom::Point const &p, SPItem *upto = nullptr)
{
    auto items = find_items_at_point(nodes, dkey, p, 1, upto);
    if (items.empty()) {
        return nullptr;
    }
    return items.back();
}

/**
 * Returns the topmost non-layer group from the descendants of group which is at point p,
 * or null if none. Recurses into layers but not into groups.
 */
static SPItem *find_group_at_point(unsigned dkey, SPGroup *group, Geom::Point const &p)
{
    double const delta = Inkscape::Preferences::get()->getDouble("/options/cursortolerance/value", 1.0);
    std::optional<bool> outline;

    for (auto &c : boost::adaptors::reverse(group->children)) {
        if (auto group = cast<SPGroup>(&c)) {
            if (group->effectiveLayerMode(dkey) == SPGroup::LAYER) {
                if (auto ret = find_group_at_point(dkey, group, p)) {
                    return ret;
                }
            } else if (auto di = group->get_arenaitem(dkey)) {
                if (!outline) {
                    if (auto cid = di->drawing().getCanvasItemDrawing()) {
                        auto canvas = cid->get_canvas();
                        outline = canvas->canvas_point_in_outline_zone(p - canvas->get_pos());
                    }
                }
                if (di->pick(p, delta, Inkscape::DrawingItem::PICK_STICKY | outline.value_or(false) * Inkscape::DrawingItem::PICK_OUTLINE)) {
                    return group;
                }
            }
        }
    }

    return nullptr;
}

/**
 * Return list of items, contained in box
 *
 * @param box area to find items, in document coordinates
 */

std::vector<SPItem*> SPDocument::getItemsInBox(unsigned int dkey, Geom::Rect const &box, bool take_hidden, bool take_insensitive, bool take_groups, bool enter_groups, bool enter_layers) const
{
    std::vector<SPItem*> x;
    return find_items_in_area(x, this->root, dkey, box, is_within, take_hidden, take_insensitive, take_groups, enter_groups, enter_layers);
}

/**
 * Get items whose bounding box overlaps with given area.
 * @param dkey desktop view in use
 * @param box area to find items, in document coordinates
 * @param take_hidden get hidden items
 * @param take_insensitive get insensitive items
 * @param take_groups get also the groups
 * @param enter_groups get items inside groups
 * @return Return list of items, that the parts of the item contained in box
 */

std::vector<SPItem*> SPDocument::getItemsPartiallyInBox(unsigned int dkey, Geom::Rect const &box, bool take_hidden, bool take_insensitive, bool take_groups, bool enter_groups, bool enter_layers) const
{
    std::vector<SPItem*> x;
    return find_items_in_area(x, this->root, dkey, box, overlaps, take_hidden, take_insensitive, take_groups, enter_groups, enter_layers);
}

std::vector<SPItem*> SPDocument::getItemsAtPoints(unsigned const key, std::vector<Geom::Point> points, bool all_layers, bool topmost_only, size_t limit) const
{
    std::vector<SPItem*> result;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // When picking along the path, we don't want small objects close together
    // (such as hatching strokes) to obscure each other by their deltas,
    // so we temporarily set delta to a small value
    gdouble saved_delta = prefs->getDouble("/options/cursortolerance/value", 1.0);
    prefs->setDouble("/options/cursortolerance/value", 0.25);

    // Cache a flattened SVG DOM to speed up selection.
    if(!_node_cache_valid){
        _node_cache.clear();
        build_flat_item_list(key, this->root, true);
        _node_cache_valid=true;
    }
    SPObject *current_layer = nullptr;
    SPDesktop *desktop = SP_ACTIVE_DESKTOP;
    if(desktop){
        current_layer = desktop->layerManager().currentLayer();
    }
    size_t item_counter = 0;
    for(int i = points.size()-1;i>=0; i--) {
        std::vector<SPItem*> items = find_items_at_point(_node_cache, key, points[i], topmost_only);
        for (SPItem *item : items) {
            if (item && result.end()==find(result.begin(), result.end(), item))
                if(all_layers || (desktop && desktop->layerManager().layerForObject(item) == current_layer)){
                    result.push_back(item);
                    item_counter++;
                    //limit 0 = no limit
                    if(item_counter == limit){
                        prefs->setDouble("/options/cursortolerance/value", saved_delta);
                        return result;
                    }
                }
        }
    }

    // and now we restore it back
    prefs->setDouble("/options/cursortolerance/value", saved_delta);

    return result;
}

SPItem *SPDocument::getItemAtPoint( unsigned const key, Geom::Point const &p,
                                    bool const into_groups, SPItem *upto) const
{
    // Build a flattened SVG DOM for find_item_at_point.
    std::deque<SPItem*> bak(_node_cache);
    if(!into_groups){
        _node_cache.clear();
        build_flat_item_list(key, this->root, into_groups);
    }
    if(!_node_cache_valid && into_groups){
        _node_cache.clear();
        build_flat_item_list(key, this->root, true);
        _node_cache_valid=true;
    }

    SPItem *res = find_item_at_point(_node_cache, key, p, upto);
    if(!into_groups)
        _node_cache = bak;
    return res;
}

SPItem *SPDocument::getGroupAtPoint(unsigned int key, Geom::Point const &p) const
{
    return find_group_at_point(key, this->root, p);
}

// Resource management

bool SPDocument::addResource(gchar const *key, SPObject *object)
{
    g_return_val_if_fail(key != nullptr, false);
    g_return_val_if_fail(*key != '\0', false);
    g_return_val_if_fail(object != nullptr, false);

    bool result = false;

    if ( !object->cloned ) {
        std::vector<SPObject *> rlist = resources[key];
        g_return_val_if_fail(std::find(rlist.begin(),rlist.end(),object) == rlist.end(), false);
        resources[key].insert(resources[key].begin(),object);

        GQuark q = g_quark_from_string(key);

        /*in general, do not send signal if the object has no id (yet),
        it means the object is not completely built.
        (happens when pasting swatches across documents, cf bug 1495106)
        [this check should be more generally presend on emit() calls since
        the backtrace is unusable with crashed from this cause]
        */
        if (object->getId() || is<SPGroup>(object) || is<SPPage>(object)) {
            resources_changed_signals[q].emit();
        } else {
            pending_resource_changes.emplace(q);
        }

        result = true;
    }

    return result;
}

bool SPDocument::removeResource(gchar const *key, SPObject *object)
{
    g_return_val_if_fail(key != nullptr, false);
    g_return_val_if_fail(*key != '\0', false);
    g_return_val_if_fail(object != nullptr, false);

    bool result = false;

    if ( !object->cloned ) {
        std::vector<SPObject *> rlist = resources[key];
        g_return_val_if_fail(!rlist.empty(), false);
        std::vector<SPObject*>::iterator it = std::find(resources[key].begin(),resources[key].end(),object);
        g_return_val_if_fail(it != rlist.end(), false);
        resources[key].erase(it);

        GQuark q = g_quark_from_string(key);
        resources_changed_signals[q].emit();

        result = true;
    }

    return result;
}

std::vector<SPObject *> const SPDocument::getResourceList(gchar const *key)
{
    std::vector<SPObject *> emptyset;
    g_return_val_if_fail(key != nullptr, emptyset);
    g_return_val_if_fail(*key != '\0', emptyset);

    return resources[key];
}

void SPDocument::process_pending_resource_changes()
{
    while (!pending_resource_changes.empty()) {
        auto q = pending_resource_changes.front();
        pending_resource_changes.pop();
        resources_changed_signals[q].emit();
    }
}

/* Helpers */

static unsigned int count_objects_recursive(SPObject *obj, unsigned int count)
{
    count++; // obj itself

    for (auto& i: obj->children) {
        count = count_objects_recursive(&i, count);
    }

    return count;
}

/**
 * Count the number of objects in a given document recursively using the count_objects_recursive helper function
 *
 * @param[in] document Pointer to the document for counting objects
 * @return Number of objects in the document
 */
static unsigned int objects_in_document(SPDocument *document)
{
    return count_objects_recursive(document->getRoot(), 0);
}

/**
 * Remove unused definitions etc. recursively from an object and its siblings
 *
 * @param[inout] obj Object which shall be "cleaned"
 */
static void vacuum_document_recursive(SPObject *obj)
{
    if (is<SPDefs>(obj)) {
        for (auto& def: obj->children) {
            // fixme: some inkscape-internal nodes in the future might not be collectable
            def.requestOrphanCollection();
        }
    } else {
        for (auto& i: obj->children) {
            vacuum_document_recursive(&i);
        }
    }
}

/**
 * Remove unused definitions etc. recursively from an entire document.
 *
 * @return Number of removed objects
 */
unsigned int SPDocument::vacuumDocument()
{
    unsigned int start = objects_in_document(this);
    unsigned int end;
    unsigned int newend = start;

    unsigned int iterations = 0;

    do {
        end = newend;

        vacuum_document_recursive(root);
        this->collectOrphans();
        iterations++;

        newend = objects_in_document(this);

    } while (iterations < 100 && newend < end);
    // We stop if vacuum_document_recursive doesn't remove any more objects or after 100 iterations, whichever occurs first.

    return start - newend;
}

/**
 * Indicate to the user if the document has been modified since the last save by displaying a "*" in front of the name of the file in the window title.
 *
 * @param[in] modified True if the document has been modified.
 */
void SPDocument::setModifiedSinceSave(bool modified) {
    this->modified_since_save = modified;
    this->modified_since_autosave = modified;
    if (SP_ACTIVE_DESKTOP) {
        if (InkscapeWindow *window = SP_ACTIVE_DESKTOP->getInkscapeWindow()) {
            // During load, SP_ACTIVE_DESKTOP may be != nullptr, but parent might still be nullptr.
            // Moreover, the desktop widget may still not be fully constructed, in which case
            // window->get_desktop_widget() will return nullptr.
            if (auto *dtw = window->get_desktop_widget()) {
                dtw->updateTitle(getDocumentName());
            }
        }
    }
}


/**
 * Paste SVG defs from the document retrieved from the clipboard or imported document into the active document.
 * @param clipdoc The document to paste.
 * @pre @c clipdoc != NULL and pasting into the active document is possible.
 */
void SPDocument::importDefs(SPDocument *source)
{
    Inkscape::XML::Node *root = source->getReprRoot();
    Inkscape::XML::Node *target_defs = this->getDefs()->getRepr();
    std::vector<Inkscape::XML::Node const *> defsNodes = sp_repr_lookup_name_many(root, "svg:defs");

    prevent_id_clashes(source, this);

    for (auto & defsNode : defsNodes) {
       _importDefsNode(source, const_cast<Inkscape::XML::Node *>(defsNode), target_defs);
    }
}

void SPDocument::_importDefsNode(SPDocument *source, Inkscape::XML::Node *defs, Inkscape::XML::Node *target_defs)
{
    int stagger=0;

    /*  Note, "clipboard" throughout the comments means "the document that is either the clipboard
        or an imported document", as importDefs is called in both contexts.

        The order of the records in the clipboard is unpredictable and there may be both
        forward and backwards references to other records within it.  There may be definitions in
        the clipboard that duplicate definitions in the present document OR that duplicate other
        definitions in the clipboard.  (Inkscape will not have created these, but they may be read
        in from other SVG sources.)

        There are 3 passes to clean this up:

        In the first find and mark definitions in the clipboard that are duplicates of those in the
        present document.  Change the ID to "RESERVED_FOR_INKSCAPE_DUPLICATE_DEF_XXXXXXXXX".
        (Inkscape will not reuse an ID, and the XXXXXXXXX keeps it from automatically creating new ones.)
        References in the clipboard to the old clipboard name are converted to the name used
        in the current document.

        In the second find and mark definitions in the clipboard that are duplicates of earlier
        definitions in the clipbard.  Unfortunately this is O(n^2) and could be very slow for a large
        SVG with thousands of definitions.  As before, references are adjusted to reflect the name
        going forward.

        In the final cycle copy over those records not marked with that ID.

        If an SVG file uses the special ID it will cause problems!

        If this function is called because of the paste of a true clipboard the caller will have passed in a
        COPY of the clipboard items.  That is good, because this routine modifies that document.  If the calling
        behavior ever changes, so that the same document is passed in on multiple pastes, this routine will break
        as in the following example:
        1.  Paste clipboard containing B same as A into document containing A.  Result, B is dropped
        and all references to it will point to A.
        2.  Paste same clipboard into a new document.  It will not contain A, so there will be unsatisfied
        references in that window.
    */

    std::string DuplicateDefString = "RESERVED_FOR_INKSCAPE_DUPLICATE_DEF";

    /* First pass: remove duplicates in clipboard of definitions in document */
    for (Inkscape::XML::Node *def = defs->firstChild() ; def ; def = def->next()) {
        if(def->type() != Inkscape::XML::NodeType::ELEMENT_NODE)continue;
        /* If this  clipboard has been pasted into one document, and is now being pasted into another,
        or pasted again into the same, it will already have been processed.  If we detect that then
        skip the rest of this pass. */
        Glib::ustring defid = def->attribute("id");
        if( defid.find( DuplicateDefString ) != Glib::ustring::npos )break;

        SPObject *src = source->getObjectByRepr(def);

        // Prevent duplicates of solid swatches by checking if equivalent swatch already exists
        auto s_gr = cast<SPGradient>(src);
        auto s_lpeobj = cast<LivePathEffectObject>(src);
        if (src && (s_gr || s_lpeobj)) {
            for (auto& trg: getDefs()->children) {
                auto t_gr = cast<SPGradient>(&trg);
                if (src != &trg && s_gr && t_gr) {
                    if (s_gr->isEquivalent(t_gr)) {
                        // Change object references to the existing equivalent gradient
                        Glib::ustring newid = trg.getId();
                        if (newid != defid) { // id could be the same if it is a second paste into the same document
                            change_def_references(src, &trg);
                        }
                        gchar *longid = g_strdup_printf("%s_%9.9d", DuplicateDefString.c_str(), stagger++);
                        def->setAttribute("id", longid);
                        g_free(longid);
                        // do NOT break here, there could be more than 1 duplicate!
                    }
                }
                auto t_lpeobj = cast<LivePathEffectObject>(&trg);
                if (src != &trg && s_lpeobj && t_lpeobj) {
                    if (t_lpeobj->is_similar(s_lpeobj)) {
                        // Change object references to the existing equivalent gradient
                        Glib::ustring newid = trg.getId();
                        if (newid != defid) { // id could be the same if it is a second paste into the same document
                            change_def_references(src, &trg);
                        }
                        gchar *longid = g_strdup_printf("%s_%9.9d", DuplicateDefString.c_str(), stagger++);
                        def->setAttribute("id", longid);
                        g_free(longid);
                        // do NOT break here, there could be more than 1 duplicate!
                    }
                }
            }
        }
    }

    /* Second pass: remove duplicates in clipboard of earlier definitions in clipboard */
    for (Inkscape::XML::Node *def = defs->firstChild() ; def ; def = def->next()) {
        if(def->type() != Inkscape::XML::NodeType::ELEMENT_NODE)continue;
        Glib::ustring defid = def->attribute("id");
        if( defid.find( DuplicateDefString ) != Glib::ustring::npos )continue; // this one already handled
        SPObject *src = source->getObjectByRepr(def);
        auto s_lpeobj = cast<LivePathEffectObject>(src);
        auto s_gr = cast<SPGradient>(src);
        if (src && (s_gr || s_lpeobj)) {
            for (Inkscape::XML::Node *laterDef = def->next() ; laterDef ; laterDef = laterDef->next()) {
                SPObject *trg = source->getObjectByRepr(laterDef);
                auto t_gr = cast<SPGradient>(trg);
                if (trg && (src != trg) && s_gr && t_gr) {
                    Glib::ustring newid = trg->getId();
                    if (newid.find(DuplicateDefString) != Glib::ustring::npos)
                        continue; // this one already handled
                    if (t_gr && s_gr->isEquivalent(t_gr)) {
                        // Change object references to the existing equivalent gradient
                        // two id's in the clipboard should never be the same, so always change references
                        change_def_references(trg, src);
                        gchar *longid = g_strdup_printf("%s_%9.9d", DuplicateDefString.c_str(), stagger++);
                        laterDef->setAttribute("id", longid);
                        g_free(longid);
                        // do NOT break here, there could be more than 1 duplicate!
                    }
                }
                auto t_lpeobj = cast<LivePathEffectObject>(trg);
                if (trg && (src != trg) && s_lpeobj && t_lpeobj) {
                    Glib::ustring newid = trg->getId();
                    if (newid.find(DuplicateDefString) != Glib::ustring::npos)
                        continue; // this one already handled
                    if (t_lpeobj->is_similar(s_lpeobj)) {
                        // Change object references to the existing equivalent gradient
                        // two id's in the clipboard should never be the same, so always change references
                        change_def_references(trg, src);
                        gchar *longid = g_strdup_printf("%s_%9.9d", DuplicateDefString.c_str(), stagger++);
                        laterDef->setAttribute("id", longid);
                        g_free(longid);
                        // do NOT break here, there could be more than 1 duplicate!
                    }
                }
            }
        }
    }

    /* Final pass: copy over those parts which are not duplicates  */
    for (Inkscape::XML::Node *def = defs->firstChild() ; def ; def = def->next()) {
        if(def->type() != Inkscape::XML::NodeType::ELEMENT_NODE)continue;

        /* Ignore duplicate defs marked in the first pass */
        Glib::ustring defid = def->attribute("id");
        if( defid.find( DuplicateDefString ) != Glib::ustring::npos )continue;

        bool duplicate = false;
        SPObject *src = source->getObjectByRepr(def);

        // Prevent duplication of symbols... could be more clever.
        // The tag "_inkscape_duplicate" is added to "id" by ClipboardManagerImpl::copySymbol().
        // We assume that symbols are in defs section (not required by SVG spec).
        if (src && is<SPSymbol>(src)) {

            Glib::ustring id = src->getRepr()->attribute("id");
            size_t pos = id.find( "_inkscape_duplicate" );
            if( pos != Glib::ustring::npos ) {

                // This is our symbol, now get rid of tag
                id.erase( pos );

                // Check that it really is a duplicate
                for (auto& trg: getDefs()->children) {
                    if (is<SPSymbol>(&trg) && src != &trg) {
                        std::string id2 = trg.getRepr()->attribute("id");

                        if( !id.compare( id2 ) ) {
                            duplicate = true;
                            break;
                        }
                    }
                }
                if ( !duplicate ) {
                    src->setAttribute("id", id);
                }
            }
        }

        if (!duplicate) {
            Inkscape::XML::Node * dup = def->duplicate(this->getReprDoc());
            target_defs->appendChild(dup);
            Inkscape::GC::release(dup);
        }
    }
}

// Signals ------------------------------

void
SPDocument::addUndoObserver(Inkscape::UndoStackObserver& observer)
{
    this->undoStackObservers.add(observer);
}

void
SPDocument::removeUndoObserver(Inkscape::UndoStackObserver& observer)
{
    this->undoStackObservers.remove(observer);
}

sigc::connection SPDocument::connectDestroy(sigc::signal<void ()>::slot_type slot)
{
    return destroySignal.connect(slot);
}

sigc::connection SPDocument::connectModified(SPDocument::ModifiedSignal::slot_type slot)
{
    return modified_signal.connect(slot);
}

sigc::connection SPDocument::connectFilenameSet(SPDocument::FilenameSetSignal::slot_type slot)
{
    return filename_set_signal.connect(slot);
}

sigc::connection SPDocument::connectCommit(SPDocument::CommitSignal::slot_type slot)
{
    return commit_signal.connect(slot);
}

sigc::connection SPDocument::connectBeforeCommit(SPDocument::BeforeCommitSignal::slot_type slot)
{
    return before_commit_signal.connect(slot);
}

sigc::connection SPDocument::connectIdChanged(gchar const *id,
                                              SPDocument::IDChangedSignal::slot_type slot)
{
    return id_changed_signals[g_quark_from_string(id)].connect(slot);
}

sigc::connection SPDocument::connectResourcesChanged(gchar const *key,
                                                     SPDocument::ResourcesChangedSignal::slot_type slot)
{
    GQuark q = g_quark_from_string(key);
    return resources_changed_signals[q].connect(slot);
}

sigc::connection
SPDocument::connectReconstructionStart(SPDocument::ReconstructionStart::slot_type slot)
{
    return _reconstruction_start_signal.connect(slot);
}

sigc::connection
SPDocument::connectReconstructionFinish(SPDocument::ReconstructionFinish::slot_type  slot)
{
    return _reconstruction_finish_signal.connect(slot);
}

void SPDocument::_emitModified() {
    static guint const flags = SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_CHILD_MODIFIED_FLAG | SP_OBJECT_PARENT_MODIFIED_FLAG;
    root->emitModified(0);
    modified_signal.emit(flags);
    _node_cache_valid=false;
}

void
SPDocument::emitReconstructionStart()
{
    // printf("Starting Reconstruction\n");
    _reconstruction_start_signal.emit();
}

void
SPDocument::emitReconstructionFinish()
{
    // printf("Finishing Reconstruction\n");
    _reconstruction_finish_signal.emit();
    // indicates that gradients are reloaded (to rebuild the Auto palette)
    resources_changed_signals[g_quark_from_string("gradient")].emit();
    resources_changed_signals[g_quark_from_string("filter")].emit();

/**
    // Reference to the old persp3d object is invalid after reconstruction.
    initialize_current_persp3d();
**/
}

void SPDocument::set_reference_document(SPDocument* document) {
    _ref_document = document;
}

SPDocument* SPDocument::get_reference_document() {
    return _ref_document;
}

SPDocument::install_reference_document::install_reference_document(SPDocument* inject_into, SPDocument* reference) {
    g_assert(inject_into);
    _parent = inject_into;
    _parent->set_reference_document(reference);
}

SPDocument::install_reference_document::~install_reference_document() {
    _parent->set_reference_document(nullptr);
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
