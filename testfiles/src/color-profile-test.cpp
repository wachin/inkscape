// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Unit tests for color profile.
 *
 * Author:
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2015 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "gtest/gtest.h"

#include "attributes.h"
#include "cms-system.h"
#include "object/color-profile.h"
#include "doc-per-case-test.h"

namespace {

/**
 * Test fixture to inherit a shared doc and create a color profile instance per test.
 */
class ProfTest : public DocPerCaseTest
{
public:
    ProfTest() :
        DocPerCaseTest(),
        _prof(0)
    {
    }

protected:
    void SetUp() override
    {
        DocPerCaseTest::SetUp();
        _prof = new Inkscape::ColorProfile();
        ASSERT_TRUE( _prof != NULL );
        _prof->document = _doc.get();
    }

    void TearDown() override
    {
        if (_prof) {
            delete _prof;
            _prof = NULL;
        }
        DocPerCaseTest::TearDown();
    }

    Inkscape::ColorProfile *_prof;
};

typedef ProfTest ColorProfileTest;

TEST_F(ColorProfileTest, SetRenderingIntent)
{
    struct {
        gchar const *attr;
        guint intVal;
    }
    const cases[] = {
        {"auto", (guint)Inkscape::RENDERING_INTENT_AUTO},
        {"perceptual", (guint)Inkscape::RENDERING_INTENT_PERCEPTUAL},
        {"relative-colorimetric", (guint)Inkscape::RENDERING_INTENT_RELATIVE_COLORIMETRIC},
        {"saturation", (guint)Inkscape::RENDERING_INTENT_SATURATION},
        {"absolute-colorimetric", (guint)Inkscape::RENDERING_INTENT_ABSOLUTE_COLORIMETRIC},
        {"something-else", (guint)Inkscape::RENDERING_INTENT_UNKNOWN},
        {"auto2", (guint)Inkscape::RENDERING_INTENT_UNKNOWN},
    };

    for (auto i : cases) {
        _prof->setKeyValue( SPAttr::RENDERING_INTENT, i.attr);
        ASSERT_EQ( (guint)i.intVal, _prof->rendering_intent ) << i.attr;
    }
}

TEST_F(ColorProfileTest, SetLocal)
{
    gchar const* cases[] = {
        "local",
        "something",
    };

    for (auto & i : cases) {
        _prof->setKeyValue( SPAttr::LOCAL, i);
        ASSERT_TRUE( _prof->local != NULL );
        if ( _prof->local ) {
            ASSERT_EQ( std::string(i), _prof->local );
        }
    }
    _prof->setKeyValue( SPAttr::LOCAL, NULL);
    ASSERT_EQ( (gchar*)0, _prof->local );
}

TEST_F(ColorProfileTest, SetName)
{
    gchar const* cases[] = {
        "name",
        "something",
    };

    for (auto & i : cases) {
        _prof->setKeyValue( SPAttr::NAME, i);
        ASSERT_TRUE( _prof->name != NULL );
        if ( _prof->name ) {
            ASSERT_EQ( std::string(i), _prof->name );
        }
    }
    _prof->setKeyValue( SPAttr::NAME, NULL );
    ASSERT_EQ( (gchar*)0, _prof->name );
}


} // namespace

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
