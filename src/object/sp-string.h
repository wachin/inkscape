// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_SP_STRING_H
#define SEEN_SP_STRING_H

/*
 * string elements
 * extracted from sp-text
 */

#include <glibmm/ustring.h>

#include "sp-object.h"

class SPString : public SPObject {
public:
	SPString();
	~SPString() override;

    Glib::ustring  string;

	void build(SPDocument* doc, Inkscape::XML::Node* repr) override;
	void release() override;

	void read_content() override;

	void update(SPCtx* ctx, unsigned int flags) override;
};

MAKE_SP_OBJECT_DOWNCAST_FUNCTIONS(SP_STRING, SPString)
MAKE_SP_OBJECT_TYPECHECK_FUNCTIONS(SP_IS_STRING, SPString)

#endif
