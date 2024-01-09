// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_COLOR_PROFILE_H
#define SEEN_COLOR_PROFILE_H

#ifdef HAVE_CONFIG_H
# include "config.h"  // only include where actually required!
#endif

#include <set>
#include <vector>

#include <glibmm/ustring.h>
#include "cms-color-types.h"

#include "sp-object.h"

struct SPColor;

namespace Inkscape {

enum {
    RENDERING_INTENT_UNKNOWN = 0,
    RENDERING_INTENT_AUTO = 1,
    RENDERING_INTENT_PERCEPTUAL = 2,
    RENDERING_INTENT_RELATIVE_COLORIMETRIC = 3,
    RENDERING_INTENT_SATURATION = 4,
    RENDERING_INTENT_ABSOLUTE_COLORIMETRIC = 5
};

class ColorProfileImpl;


/**
 * Color Profile.
 */
class ColorProfile final : public SPObject {
public:
    ColorProfile();
    ~ColorProfile() override;
    int tag() const override { return tag_of<decltype(*this)>; }

    bool operator<(ColorProfile const &other) const;

    static Glib::ustring getNameFromProfile(cmsHPROFILE profile);
    static void sanitizeName(std::string &str);

    friend cmsHPROFILE colorprofile_get_handle( SPDocument*, unsigned int*, char const* );
    friend class CMSSystem;

    class FilePlusHome {
    public:
        FilePlusHome(Glib::ustring filename, bool isInHome);
        FilePlusHome(const FilePlusHome &filePlusHome);
        bool operator<(FilePlusHome const &other) const;
        Glib::ustring filename;
        bool isInHome;
    };
    class FilePlusHomeAndName: public FilePlusHome {
    public:
        FilePlusHomeAndName(FilePlusHome filePlusHome, Glib::ustring name);
        bool operator<(FilePlusHomeAndName const &other) const;
        Glib::ustring name;
    };

    static std::set<FilePlusHome> getBaseProfileDirs();
    static std::set<FilePlusHome> getProfileFiles();
    static std::set<FilePlusHomeAndName> getProfileFilesWithNames();
    //icColorSpaceSignature getColorSpace() const;
    ColorSpaceSig getColorSpace() const;
    //icProfileClassSignature getProfileClass() const;
    ColorProfileClassSig getProfileClass() const;
    cmsHTRANSFORM getTransfToSRGB8();
    cmsHTRANSFORM getTransfFromSRGB8();
    cmsHTRANSFORM getTransfGamutCheck();
    bool GamutCheck(SPColor color);
    int getChannelCount() const;

    char* href;
    char* local;
    char* name;
    char* intentStr;
    unsigned int rendering_intent; // FIXME: type the enum and hold that instead

protected:
    ColorProfileImpl *impl;

    void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
    void release() override;

    void set(SPAttr key, char const* value) override;

    Inkscape::XML::Node* write(Inkscape::XML::Document* doc, Inkscape::XML::Node* repr, unsigned int flags) override;
};

} // namespace Inkscape

#endif // !SEEN_COLOR_PROFILE_H

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
