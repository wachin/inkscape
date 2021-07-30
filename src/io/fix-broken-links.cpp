// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * tracks external resources such as image and css files.
 *
 * Copyright 2011  Jon A. Cruz  <jon@joncruz.org>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <string>
#include <vector>
#include <set>
#include <algorithm>

#include <gtkmm/recentmanager.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/uriutils.h>
#include <glibmm/convert.h>

#include "fix-broken-links.h"

#include "document.h"
#include "document-undo.h"
#include "verbs.h"

#include "object/sp-object.h"

#include "xml/node.h"

namespace Inkscape {

static std::vector<std::string> splitPath( std::string const &path )
{
    std::vector<std::string> parts;

    std::string prior;
    std::string tmp = path;
    while ( !tmp.empty() && (tmp != prior) ) {
        prior = tmp;

        parts.push_back( Glib::path_get_basename(tmp) );
        tmp = Glib::path_get_dirname(tmp);
    }
    if ( !parts.empty() ) {
        std::reverse(parts.begin(), parts.end());
        if ( (parts[0] == ".") && (path[0] != '.') ) {
            parts.erase(parts.begin());
        }
    }

    return parts;
}

static std::string convertPathToRelative( std::string const &path, std::string const &docbase )
{
    std::string result = path;

    if ( !path.empty() && Glib::path_is_absolute(path) ) {
        // Whack the parts into pieces

        std::vector<std::string> parts = splitPath(path);
        std::vector<std::string> baseParts = splitPath(docbase);

        // TODO debug g_message("+++++++++++++++++++++++++");
        for ( std::vector<std::string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
            // TODO debug g_message("    [%s]", it->c_str());
        }
        // TODO debug g_message(" - - - - - - - - - - - - - - - ");
        for ( std::vector<std::string>::iterator it = baseParts.begin(); it != baseParts.end(); ++it ) {
            // TODO debug g_message("    [%s]", it->c_str());
        }
        // TODO debug g_message("+++++++++++++++++++++++++");

        if ( !parts.empty() && !baseParts.empty() && (parts[0] == baseParts[0]) ) {
            // Both paths have the same root. We can proceed.
            while ( !parts.empty() && !baseParts.empty() && (parts[0] == baseParts[0]) ) {
                parts.erase( parts.begin() );
                baseParts.erase( baseParts.begin() );
            }

            // TODO debug g_message("+++++++++++++++++++++++++");
            for ( std::vector<std::string>::iterator it = parts.begin(); it != parts.end(); ++it ) {
                // TODO debug g_message("    [%s]", it->c_str());
            }
            // TODO debug g_message(" - - - - - - - - - - - - - - - ");
            for ( std::vector<std::string>::iterator it = baseParts.begin(); it != baseParts.end(); ++it ) {
                // TODO debug g_message("    [%s]", it->c_str());
            }
            // TODO debug g_message("+++++++++++++++++++++++++");

            if ( !parts.empty() ) {
                result.clear();

                for ( size_t i = 0; i < baseParts.size(); ++i ) {
                    parts.insert(parts.begin(), "..");
                }
                result = Glib::build_filename( parts );
                // TODO debug g_message("----> [%s]", result.c_str());
            }
        }
    }

    return result;
}


bool fixBrokenLinks(SPDocument *doc);
    

/**
 * Walk all links in a document and create a listing of unique broken links.
 *
 * @return a list of all broken links.
 */
static std::vector<Glib::ustring> findBrokenLinks(SPDocument *doc);

/**
 * Resolve broken links as a whole and return a map for those that can be found.
 *
 * Note: this will allow for future enhancements including relinking to new locations
 * with the most broken files found, etc.
 *
 * @return a map of found links.
 */
static std::map<Glib::ustring, Glib::ustring> locateLinks(Glib::ustring const & docbase, std::vector<Glib::ustring> const & brokenLinks);


/**
 * Try to parse href into a local filename using standard methods.
 *
 * @return true if successful.
 */
static bool extractFilepath(Glib::ustring const &href, std::string &filename);

/**
 * Try to parse href into a local filename using some non-standard methods.
 * This means the href is likely invalid and should be rewritten.
 *
 * @return true if successful.
 */
static bool reconstructFilepath(Glib::ustring const &href, std::string &filename);

static bool searchUpwards( std::string const &base, std::string const &subpath, std::string &dest );



static bool extractFilepath(Glib::ustring const &href, std::string &filename)
{                    
    bool isFile = false;

    filename.clear();

    auto scheme = Glib::uri_parse_scheme(href.raw());
    if ( !scheme.empty() ) {
        // TODO debug g_message("Scheme is now [%s]", scheme.c_str());
        if ( scheme == "file" ) {
            // TODO debug g_message("--- is a file URI                 [%s]", href.c_str());

            // throws Glib::ConvertError:
            try {
                filename = Glib::filename_from_uri(href);
                isFile = true;
            } catch(Glib::ConvertError e) {
                g_warning("%s", e.what().c_str());
            }
        }
    } else {
        // No scheme. Assuming it is a file path (absolute or relative).
        // throws Glib::ConvertError:
        filename = Glib::filename_from_utf8(href);
        isFile = true;
    }

    return isFile;
}

static bool reconstructFilepath(Glib::ustring const &href, std::string &filename)
{                    
    bool isFile = false;

    filename.clear();

    auto scheme = Glib::uri_parse_scheme(href.raw());
    if ( !scheme.empty() ) {
        if ( scheme == "file" ) {
            // try to build a relative filename for URIs like "file:image.png"
            // they're not standard conformant but not uncommon
            Glib::ustring href_new = Glib::ustring(href, 5);
            filename = Glib::filename_from_utf8(href_new);
            isFile = true;
        }
    }
    return isFile;
}


static std::vector<Glib::ustring> findBrokenLinks( SPDocument *doc )
{
    std::vector<Glib::ustring> result;
    std::set<Glib::ustring> uniques;

    if ( doc ) {
        std::vector<SPObject *> images = doc->getResourceList("image");
        for (auto image : images) {
            Inkscape::XML::Node *ir = image->getRepr();

            gchar const *href = ir->attribute("xlink:href");
            if ( href &&  ( uniques.find(href) == uniques.end() ) ) {
                std::string filename;
                if (extractFilepath(href, filename)) {
                    if (Glib::path_is_absolute(filename)) {
                        if (!Glib::file_test(filename, Glib::FILE_TEST_EXISTS)) {
                            result.emplace_back(href);
                            uniques.insert(href);
                        }
                    } else {
                        std::string combined = Glib::build_filename(doc->getDocumentBase(), filename);
                        if ( !Glib::file_test(combined, Glib::FILE_TEST_EXISTS) ) {
                            result.emplace_back(href);
                            uniques.insert(href);
                        }
                    }
                } else if (reconstructFilepath(href, filename)) {
                    result.emplace_back(href);
                    uniques.insert(href);
                }
            }
        }        
    }

    return result;
}


static std::map<Glib::ustring, Glib::ustring> locateLinks(Glib::ustring const & docbase, std::vector<Glib::ustring> const & brokenLinks)
{
    std::map<Glib::ustring, Glib::ustring> result;


    // Note: we use a vector because we want them to stay in order:
    std::vector<std::string> priorLocations;

    Glib::RefPtr<Gtk::RecentManager> recentMgr = Gtk::RecentManager::get_default();
    std::vector< Glib::RefPtr<Gtk::RecentInfo> > recentItems = recentMgr->get_items();
    for (auto & recentItem : recentItems) {
        Glib::ustring uri = recentItem->get_uri();
        auto scheme = Glib::uri_parse_scheme(uri.raw());
        if ( scheme == "file" ) {
            try {
                std::string path = Glib::filename_from_uri(uri);
                path = Glib::path_get_dirname(path);
                if ( std::find(priorLocations.begin(), priorLocations.end(), path) == priorLocations.end() ) {
                    // TODO debug g_message("               ==>[%s]", path.c_str());
                    priorLocations.push_back(path);
                }
            } catch (Glib::ConvertError e) {
                g_warning("%s", e.what().c_str());
            }
        }
    }

    // At the moment we expect this list to contain file:// references, or simple relative or absolute paths.
    for (const auto & brokenLink : brokenLinks) {
        // TODO debug g_message("========{%s}", it->c_str());

        std::string filename;
        if (extractFilepath(brokenLink, filename) || reconstructFilepath(brokenLink, filename)) {
            auto const docbase_native = Glib::filename_from_utf8(docbase);

            // We were able to get some path. Check it
            std::string origPath = filename;

            if (!Glib::path_is_absolute(filename)) {
                filename = Glib::build_filename(docbase_native, filename);
            }

            bool exists = Glib::file_test(filename, Glib::FILE_TEST_EXISTS);

            // search in parent folders
            if (!exists) {
                exists = searchUpwards(docbase_native, origPath, filename);
            }

            // Check if the MRU bases point us to it.
            if ( !exists ) {
                if ( !Glib::path_is_absolute(origPath) ) {
                    for ( std::vector<std::string>::iterator it = priorLocations.begin(); !exists && (it != priorLocations.end()); ++it ) {
                        exists = searchUpwards(*it, origPath, filename);
                    }
                }
            }

            if ( exists ) {
                if (Glib::path_is_absolute(filename)) {
                    filename = convertPathToRelative(filename, docbase_native);
                }

                bool isAbsolute = Glib::path_is_absolute(filename);
                Glib::ustring replacement =
                    isAbsolute ? Glib::filename_to_uri(filename) : Glib::filename_to_utf8(filename);
                result[brokenLink] = replacement;
            }
        }
    }

    return result;
}

bool fixBrokenLinks(SPDocument *doc)
{
    bool changed = false;
    if ( doc ) {
        // TODO debug g_message("FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP FIXUP");
        // TODO debug g_message("      base is [%s]", doc->getDocumentBase());

        std::vector<Glib::ustring> brokenHrefs = findBrokenLinks(doc);
        if ( !brokenHrefs.empty() ) {
            // TODO debug g_message("    FOUND SOME LINKS %d", static_cast<int>(brokenHrefs.size()));
            for ( std::vector<Glib::ustring>::iterator it = brokenHrefs.begin(); it != brokenHrefs.end(); ++it ) {
                // TODO debug g_message("        [%s]", it->c_str());
            }
        }

        Glib::ustring base;
        if (doc->getDocumentBase()) {
            base = doc->getDocumentBase();
        }

        std::map<Glib::ustring, Glib::ustring> mapping = locateLinks(base, brokenHrefs);
        for ( std::map<Glib::ustring, Glib::ustring>::iterator it = mapping.begin(); it != mapping.end(); ++it )
        {
            // TODO debug g_message("     [%s] ==> {%s}", it->first.c_str(), it->second.c_str());
        }

        bool savedUndoState = DocumentUndo::getUndoSensitive(doc);
        DocumentUndo::setUndoSensitive(doc, true);
        
        std::vector<SPObject *> images = doc->getResourceList("image");
        for (auto image : images) {
            Inkscape::XML::Node *ir = image->getRepr();

            gchar const *href = ir->attribute("xlink:href");
            if ( href ) {
                // TODO debug g_message("                  consider [%s]", href);
                
                if ( mapping.find(href) != mapping.end() ) {
                    // TODO debug g_message("                     Found a replacement");

                    ir->setAttributeOrRemoveIfEmpty( "xlink:href", mapping[href] );
                    if ( ir->attribute( "sodipodi:absref" ) ) {
                        ir->removeAttribute("sodipodi:absref"); // Remove this attribute
                    }

                    SPObject *updated = doc->getObjectByRepr(ir);
                    if (updated) {
                        // force immediate update of dependent attributes
                        updated->updateRepr();
                    }

                    changed = true;
                }
            }
        }
        if ( changed ) {
            DocumentUndo::done( doc, SP_VERB_DIALOG_XML_EDITOR, _("Fixup broken links") );
        }
        DocumentUndo::setUndoSensitive(doc, savedUndoState);
    }

    return changed;
}

static bool searchUpwards( std::string const &base, std::string const &subpath, std::string &dest )
{
    bool exists = false;
    // TODO debug g_message("............");

    std::vector<std::string> parts = splitPath(subpath);
    std::vector<std::string> baseParts = splitPath(base);

    while ( !exists && !baseParts.empty() ) {
        std::vector<std::string> current;
        current.insert(current.begin(), parts.begin(), parts.end());
        // TODO debug g_message("         ---{%s}", Glib::build_filename( baseParts ).c_str());
        while ( !exists && !current.empty() ) {
            std::vector<std::string> combined;
            combined.insert( combined.end(), baseParts.begin(), baseParts.end() );
            combined.insert( combined.end(), current.begin(), current.end() );
            std::string filepath = Glib::build_filename( combined );
            exists = Glib::file_test(filepath, Glib::FILE_TEST_EXISTS);
            // TODO debug g_message("            ...[%s] %s", filepath.c_str(), (exists ? "XXX" : ""));
            if ( exists ) {
                dest = filepath;
            }
            current.erase( current.begin() );
        }
        baseParts.pop_back();
    }

    return exists;
}

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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
