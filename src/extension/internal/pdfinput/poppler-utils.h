// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PDF parsing utilities for libpoppler.
 *//*
 * Authors:
 *    Martin Owens
 * 
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef POPPLER_UTILS_H
#define POPPLER_UTILS_H

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "poppler-transition-api.h"

namespace Geom {
class Affine;
}
class Array;
class Dict;
class FNVHash;
class GfxFont;
class GfxState;
class GooString;
class Object;
class PDFDoc;
class Ref;
class XRef;

Geom::Affine stateToAffine(GfxState *state);
Geom::Affine ctmToAffine(const double *ctm);

void ctmout(const char *label, const double *ctm);
void affout(const char *label, Geom::Affine affine);

void pdf_debug_array(const Array *array, int depth = 0, XRef *xref = nullptr);
void pdf_debug_dict(const Dict *dict, int depth = 0, XRef *xref = nullptr);
void pdf_debug_object(const Object *obj, int depth = 0, XRef *xref = nullptr);

#if POPPLER_CHECK_VERSION(22, 4, 0)
typedef std::shared_ptr<GfxFont> FontPtr;
#else
typedef GfxFont *FontPtr;
#endif

class FontData
{
public:
    FontData(FontPtr font);
    std::string getSubstitute() const;
    std::string getSpecification() const;

    bool found = false;

    std::unordered_set<int> pages;
    std::string name;
    std::string family;

    std::string style;
    std::string weight;
    std::string stretch;
    std::string variation;

private:
    void _parseStyle();
};

typedef std::shared_ptr<std::map<FontPtr, FontData>> FontList;

FontList getPdfFonts(std::shared_ptr<PDFDoc> pdf_doc);
std::string getDictString(Dict *dict, const char *key);
std::string getString(const GooString *value);
std::string validateString(std::string const &in);

// Replacate poppler FontDict
class InkFontDict
{
public:
    // Build the font dictionary, given the PDF font dictionary.
    InkFontDict(XRef *xref, Ref *fontDictRef, Dict *fontDict);

    // Iterative access.
    int getNumFonts() const { return fonts.size(); }

    // Get the specified font.
    FontPtr lookup(const char *tag) const;
    FontPtr getFont(int i) const { return fonts[i]; }
    std::vector<FontPtr> fonts;

private:
    int hashFontObject(Object *obj);
    void hashFontObject1(const Object *obj, FNVHash *h);
};

#endif /* POPPLER_UTILS_H */
