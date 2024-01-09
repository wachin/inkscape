// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * PDF Parsing utility functions and classes.
 *//*
 * 
 * Copyright (C) 2022 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef PDF_ENUMS_H
#define PDF_ENUMS_H

#include <map>

enum class FontStrategy : unsigned char
{
    RENDER_MISSING,
    RENDER_ALL,
    SUBSTITUTE_MISSING,
    KEEP_MISSING,
    DELETE_MISSING,
    DELETE_ALL
};
enum class FontFallback : unsigned char
{
    DELETE_TEXT = 0,
    AS_SHAPES,
    AS_TEXT,
    AS_SUB,
};
typedef std::map<int, FontFallback> FontStrategies;

#endif /* PDF_ENUMS_H */
