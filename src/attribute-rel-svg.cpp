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
 * attribute-rel-svg.cpp
 *
 *  Created on: Jul 25, 2011
 *      Author: abhishek
 */

/** \class SPAttributeRelSVG
 *
 * SPAttributeRelSVG class stores the mapping of element->attribute
 * relationship and provides a static function to access that
 * mapping indirectly(only reading).
 */

#include <fstream>
#include <sstream>

#include "attribute-rel-svg.h"

#include "io/resource.h"
#include "path-prefix.h"
#include "preferences.h"

SPAttributeRelSVG * SPAttributeRelSVG::instance = nullptr;
bool SPAttributeRelSVG::foundFile = false;

/*
 * This function returns true if element is an SVG element.
 */
bool SPAttributeRelSVG::isSVGElement(Glib::ustring element)
{
    if (SPAttributeRelSVG::instance == nullptr) {
        SPAttributeRelSVG::instance = new SPAttributeRelSVG();
    }

    // Always valid if data file not found!
    if( !foundFile ) return true;

    // Strip off "svg:" from the element's name
    Glib::ustring temp = element;
    if ( temp.find("svg:") != std::string::npos ) {
        temp.erase( temp.find("svg:"), 4 );
    }

    return (SPAttributeRelSVG::instance->attributesOfElements.count(temp) > 0);
}

/*
 * This functions checks whether an element -> attribute pair is allowed or not
 */
bool SPAttributeRelSVG::findIfValid(Glib::ustring attribute, Glib::ustring element)
{
    if (SPAttributeRelSVG::instance == nullptr) {
        SPAttributeRelSVG::instance = new SPAttributeRelSVG();
    }

    // Always valid if data file not found!
    if( !foundFile ) return true;

    // Strip off "svg:" from the element's name
    Glib::ustring temp = element;
    if ( temp.find("svg:") != std::string::npos ) {
        temp.erase( temp.find("svg:"), 4 );
    }
    
    // Check for attributes with -, role, aria etc. to allow for more accessibility
    // clang-format off
    if (attribute[0] == '-'
        || attribute.substr(0,4) == "role"
        || attribute.substr(0,4) == "aria"
        || attribute.substr(0,5) == "xmlns"
        || attribute.substr(0,9) == "inkscape:"
        || attribute.substr(0,9) == "sodipodi:"
        || attribute.substr(0,4) == "rdf:"
        || attribute.substr(0,3) == "cc:"
        || attribute.substr(0,4) == "ns1:"  // JessyInk
        || (SPAttributeRelSVG::instance->attributesOfElements[temp].find(attribute)
            != SPAttributeRelSVG::instance->attributesOfElements[temp].end()) ) {
    // clang-format on
        return true;
    } else {
        //g_warning( "Invalid attribute: %s used on <%s>", attribute.c_str(), element.c_str() );
        return false;
    }
}

/*
 * One timer singleton constructor, to load the element -> attributes data
 * into memory.
 */
SPAttributeRelSVG::SPAttributeRelSVG()
{
    std::fstream f;
    
    // Read data from standard path
    using namespace Inkscape::IO::Resource;
    auto filepath = get_path_string(SYSTEM, ATTRIBUTES, "svgprops");

    f.open(filepath.c_str(), std::ios::in);

    if (!f.is_open()) {
        // Display warning for file not open
        g_warning("Could not open the data file for XML attribute-element map construction: %s", filepath.c_str());
        f.close();
        return ;
    }

    foundFile = true;

    while (!f.eof()){
        std::stringstream ss;
        std::string s;

        std::getline(f,s,'"');
        std::getline(f,s,'"');
        if(s.size() > 0 && s[0] != '\n'){
            std::string prop = s;
            getline(f,s);
            ss << s;

            while(std::getline(ss,s,'"')){
                std::string element;
                std::getline(ss,s,'"');
                element = s;
                attributesOfElements[element].insert(prop);
            }
        }
    }
    
    f.close();
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
