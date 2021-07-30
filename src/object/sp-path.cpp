// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * SVG <path> implementation
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   David Turner <novalis@gnu.org>
 *   Abhishek Sharma
 *   Johan Engelen
 *
 * Copyright (C) 2004 David Turner
 * Copyright (C) 1999-2002 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 1999-2012 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glibmm/i18n.h>
#include <glibmm/regex.h>

#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "live_effects/lpeobject-reference.h"
#include "sp-lpe-item.h"

#include "display/curve.h"
#include <2geom/curves.h>
#include "helper/geom-curves.h"

#include "svg/svg.h"
#include "xml/repr.h"
#include "attributes.h"

#include "sp-path.h"
#include "sp-guide.h"

#include "document.h"
#include "desktop.h"

#include "desktop-style.h"
#include "ui/tools/tool-base.h"
#include "inkscape.h"
#include "style.h"

#define noPATH_VERBOSE

gint SPPath::nodesInPath() const
{
    return _curve ? _curve->nodes_in_path() : 0;
}

const char* SPPath::typeName() const {
    return "path";
}

const char* SPPath::displayName() const {
    return _("Path");
}

gchar* SPPath::description() const {
    int count = this->nodesInPath();
    char *lpe_desc = g_strdup("");
    
    if (hasPathEffect()) {
        Glib::ustring s;
        PathEffectList effect_list =  this->getEffectList();
        
        for (auto & it : effect_list)
        {
            LivePathEffectObject *lpeobj = it->lpeobject;
            
            if (!lpeobj || !lpeobj->get_lpe()) {
                break;
            }
            
            if (s.empty()) {
                s = lpeobj->get_lpe()->getName();
            } else {
                s = s + ", " + lpeobj->get_lpe()->getName();
            }
        }
        lpe_desc = g_strdup_printf(_(", path effect: %s"), s.c_str());
    }
    char *ret = g_strdup_printf(ngettext(
                _("%i node%s"), _("%i nodes%s"), count), count, lpe_desc);
    g_free(lpe_desc);
    return ret;
}

void SPPath::convert_to_guides() const {
    if (!this->_curve) {
        return;
    }

    std::list<std::pair<Geom::Point, Geom::Point> > pts;

    Geom::Affine const i2dt(this->i2dt_affine());
    Geom::PathVector const & pv = this->_curve->get_pathvector();
    
    for(const auto & pit : pv) {
        for(Geom::Path::const_iterator cit = pit.begin(); cit != pit.end_default(); ++cit) {
            // only add curves for straight line segments
            if( is_straight_curve(*cit) )
            {
                pts.emplace_back(cit->initialPoint() * i2dt, cit->finalPoint() * i2dt);
            }
        }
    }

    sp_guide_pt_pairs_to_guides(this->document, pts);
}

SPPath::SPPath() : SPShape(), connEndPair(this) {
}

SPPath::~SPPath() = default;

void SPPath::build(SPDocument *document, Inkscape::XML::Node *repr) {
    /* Are these calls actually necessary? */
    this->readAttr(SPAttr::MARKER);
    this->readAttr(SPAttr::MARKER_START);
    this->readAttr(SPAttr::MARKER_MID);
    this->readAttr(SPAttr::MARKER_END);

    sp_conn_end_pair_build(this);

    SPShape::build(document, repr);
    // Our code depends on 'd' being an attribute (LPE's, etc.). To support 'd' as a property, we
    // check it here (after the style property has been evaluated, this allows us to properly
    // handled precedence of property vs attribute). If we read in a 'd' set by styling, convert it
    // to an attribute. We'll convert it back on output.

    d_source = style->d.style_src;

    if (style->d.set &&

        (d_source == SPStyleSrc::STYLE_PROP || d_source == SPStyleSrc::STYLE_SHEET) ) {

        if (char const *d_val = style->d.value()) {
            // Chrome shipped with a different syntax for property vs attribute.
            // The SVG Working group decided to follow the Chrome syntax (which may
            // allow future extensions of the 'd' property). The property syntax
            // wraps the path data with "path(...)". We must strip that!

            // Must be Glib::ustring or we get conversion errors!
            Glib::ustring input = d_val;
            Glib::ustring expression = R"A(path\("(.*)"\))A";
            Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create(expression);
            Glib::MatchInfo matchInfo;
            regex->match(input, matchInfo);

            if (matchInfo.matches()) {
                Glib::ustring  value = matchInfo.fetch(1);
                Geom::PathVector pv = sp_svg_read_pathv(value.c_str());

                auto curve = std::make_unique<SPCurve>(pv);
                if (curve) {

                    // Update curve
                    setCurveInsync(std::move(curve));

                    // Convert from property to attribute (convert back on write)
                    setAttributeOrRemoveIfEmpty("d", value);

                    SPCSSAttr *css = sp_repr_css_attr( getRepr(), "style");
                    sp_repr_css_unset_property ( css, "d");
                    sp_repr_css_set ( getRepr(), css, "style" );
                    sp_repr_css_attr_unref ( css );

                    style->d.style_src = SPStyleSrc::ATTRIBUTE;
                } else {
                    std::cerr << "SPPath::build: Failed to create curve: " << input << std::endl;
                }
            }
        }
        // If any if statement is false, do nothing... don't overwrite 'd' from attribute
    }


    // this->readAttr(SPAttr::INKSCAPE_ORIGINAL_D); // bug #1299948
    // Why we take the long way of doing this probably needs some explaining:
    //
    // Normally upon being built, reading the inkscape:original-d attribute
    // will cause the path to actually _write to its repr_ in response to this.
    // This is bad, bad news if the attached effect refers to a path which
    // hasn't been constructed yet.
    // 
    // What will happen is the effect parameter will cause the effect to
    // recalculate with a completely different value due to the parameter being
    // "empty" -- even worse, an undo event might be created with the bad value,
    // and undoing the current action could cause it to revert to the "bad"
    // state. (After that, the referred object will be constructed and the
    // reference will trigger the path effect to update and commit the right
    // value to "d".)
    //
    // This mild nastiness here (don't recalculate effects on build) prevents a
    // plethora of issues with effects with linked parameters doing wild and
    // stupid things on new documents upon a mere undo.

    if (gchar const* s = this->getRepr()->attribute("inkscape:original-d"))
    {
        // Write the value to _curve_before_lpe, do not recalculate effects
        Geom::PathVector pv = sp_svg_read_pathv(s);
        _curve_before_lpe.reset(new SPCurve(pv));
    }
    this->readAttr(SPAttr::D);

    /* d is a required attribute */
    char const *d = this->getAttribute("d");

    if (d == nullptr) {
        // First see if calculating the path effect will generate "d":
        this->update_patheffect(true);
        d = this->getAttribute("d");

        // I guess that didn't work, now we have nothing useful to write ("")
        if (d == nullptr) {
            this->setKeyValue( sp_attribute_lookup("d"), "");
        }
    }
}

void SPPath::release() {
    this->connEndPair.release();

    SPShape::release();
}

void SPPath::set(SPAttr key, const gchar* value) {
    switch (key) {
        case SPAttr::INKSCAPE_ORIGINAL_D:
            if (value) {
                Geom::PathVector pv = sp_svg_read_pathv(value);
                setCurveBeforeLPE(std::make_unique<SPCurve>(pv));
            } else {
                bool haslpe = this->hasPathEffectOnClipOrMaskRecursive(this);
                if (!haslpe) {
                    this->setCurveBeforeLPE(nullptr);
                } else {
                    //This happends on undo, fix bug:#1791784
                    this->removeAllPathEffects(false);
                }
            }
            // In 2020-8-15 next line is commented and added
            // a todo to see regressions, after this in a commit this line is uncomented
            // again this line in a MR that near 1.1 release is finaly rollbacked. 
            // This rollback happends near release (1.1) and think is beter leave 
            // uncomented for release as all check are done this way
            // Commentesd near 1.2 branching to seee isues
            // If we find necesary and readd it agasin all plesse format the commments 
            // abobe to nor comment again in the future
            // sp_lpe_item_update_patheffect(this, true, true);
            break;

       case SPAttr::D:
            if (value) {
                Geom::PathVector pv = sp_svg_read_pathv(value);
                setCurve(std::make_unique<SPCurve>(pv));
            } else {
                this->setCurve(nullptr);
            }
            break;

        case SPAttr::MARKER:
            sp_shape_set_marker(this, SP_MARKER_LOC, value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::MARKER_START:
            sp_shape_set_marker(this, SP_MARKER_LOC_START, value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::MARKER_MID:
            sp_shape_set_marker(this, SP_MARKER_LOC_MID, value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;
        case SPAttr::MARKER_END:
            sp_shape_set_marker(this, SP_MARKER_LOC_END, value);
            this->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG);
            break;

        case SPAttr::CONNECTOR_TYPE:
        case SPAttr::CONNECTOR_CURVATURE:
        case SPAttr::CONNECTION_START:
        case SPAttr::CONNECTION_END:
        case SPAttr::CONNECTION_START_POINT:
        case SPAttr::CONNECTION_END_POINT:
            this->connEndPair.setAttr(key, value);
            break;

        default:
            SPShape::set(key, value);
            break;
    }
}

Inkscape::XML::Node* SPPath::write(Inkscape::XML::Document *xml_doc, Inkscape::XML::Node *repr, guint flags) {
    if ((flags & SP_OBJECT_WRITE_BUILD) && !repr) {
        repr = xml_doc->createElement("svg:path");
    }

#ifdef PATH_VERBOSE
g_message("sp_path_write writes 'd' attribute");
#endif

    if (this->_curve) {
        repr->setAttribute("d", sp_svg_write_path(this->_curve->get_pathvector()));
    } else {
        repr->removeAttribute("d");
    }

    if (flags & SP_OBJECT_WRITE_EXT) {
        if ( this->_curve_before_lpe != nullptr ) {
            repr->setAttribute("inkscape:original-d", sp_svg_write_path(this->_curve_before_lpe->get_pathvector()));
        } else {
            repr->removeAttribute("inkscape:original-d");
        }
    }

    this->connEndPair.writeRepr(repr);

    SPShape::write(xml_doc, repr, flags);

    return repr;
}

void SPPath::update_patheffect(bool write) {
    SPShape::update_patheffect(write);
}

void SPPath::update(SPCtx *ctx, guint flags) {
    if (flags & (SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG | SP_OBJECT_VIEWPORT_MODIFIED_FLAG)) {
        flags &= ~SP_OBJECT_USER_MODIFIED_FLAG_B; // since we change the description, it's not a "just translation" anymore
    }

    SPShape::update(ctx, flags);
    this->connEndPair.update();
}

Geom::Affine SPPath::set_transform(Geom::Affine const &transform) {
    if (!_curve) { // 0 nodes, nothing to transform
        return Geom::identity();
    }
    if (pathEffectsEnabled() && !optimizeTransforms()) {
        return transform;
    }
    if (hasPathEffectRecursive() && pathEffectsEnabled()) {
        if (!_curve_before_lpe) {
            // we are inside a LPE group creating a new element 
            // and the original-d curve is not defined, 
            // This fix a issue with calligrapic tool that make a transform just when draw
            setCurveBeforeLPE(std::move(_curve));
        }
        _curve_before_lpe->transform(transform);
    } else {
        _curve->transform(transform);
    }
    // Adjust stroke
    this->adjust_stroke(transform.descrim());

    // Adjust pattern fill
    this->adjust_pattern(transform);

    // Adjust gradient fill
    this->adjust_gradient(transform);

    // nothing remains - we've written all of the transform, so return identity
    return Geom::identity();
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
