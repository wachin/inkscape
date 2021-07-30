// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_PNG_WRITE_H
#define SEEN_SP_PNG_WRITE_H

/*
 * PNG file format utilities
 *
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Peter Bostrom
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <glib.h> // Only for gchar.
#include <vector>

#include <2geom/forward.h>

class SPDocument;
class SPItem;

enum ExportResult {
    EXPORT_ERROR = 0,
    EXPORT_OK,
    EXPORT_ABORTED
};

/**
 * Export the given document as a Portable Network Graphics (PNG) file.
 *
 * @return EXPORT_OK if succeeded, EXPORT_ABORTED if no action was taken, EXPORT_ERROR (false) if an error occurred.
 */
ExportResult sp_export_png_file(SPDocument *doc, gchar const *filename,
				double x0, double y0, double x1, double y1,
				unsigned long int width, unsigned long int height, double xdpi, double ydpi,
				unsigned long bgcolor,
				unsigned int (*status) (float, void *), void *data, bool force_overwrite = false, const std::vector<SPItem*> &items_only = std::vector<SPItem*>(), 
                                bool interlace = false, int color_type = 6, int bit_depth = 8, int zlib = 6, int antialiasing = 2);

ExportResult sp_export_png_file(SPDocument *doc, gchar const *filename,
				Geom::Rect const &area,
				unsigned long int width, unsigned long int height, double xdpi, double ydpi,
				unsigned long bgcolor,
				unsigned int (*status) (float, void *), void *data, bool force_overwrite = false, const std::vector<SPItem*> &items_only = std::vector<SPItem*>(), 
                                bool interlace = false, int color_type = 6, int bit_depth = 8, int zlib = 6, int antialiasing = 2);

#endif // SEEN_SP_PNG_WRITE_H
