// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file Test the computation of visual bounding boxes.
 */
/*
 * Authors:
 *   Rafał Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2023 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <gtest/gtest.h>

#include <2geom/rect.h>
#include <iostream>
#include <iomanip>
#include "inkscape.h"
#include "document.h"
#include "object/sp-item.h"
#include "object/sp-rect.h"

class InkscapeInit // Initialize the Inkscape Application singleton.
{
public:
    InkscapeInit()
    {
        if (!Inkscape::Application::exists()) {
            Inkscape::Application::create(false);
        }
    }
};

class VisualBoundsTest : public ::testing::Test
{
protected:

    VisualBoundsTest()
        : _init{}
        , _document{SPDocument::createNewDoc(INKSCAPE_TESTS_DIR "/data/visual-bounds.svg", false)}
    {
        _document->ensureUpToDate();
        _findTestCount();
    }

public:
    SPItem *getItemById(char const *const id)
    {
        auto obj = _document->getObjectById(id);
        if (!obj) {
            return nullptr;
        }
        return cast<SPItem>(obj);
    }
    size_t testCount() const { return _test_count; }
private:
    void _findTestCount()
    {
        auto tspan = _document->getObjectById("num_tests");
        if (!tspan) {
            std::cerr << "Could not get the element with id=\"num_tests\".\n";
            return;
        }
        auto content = tspan->firstChild();
        if (!content) {
            std::cerr << "Could not get the content of the element with id=\"num_tests\".\n";
            return;
        }
        auto repr = content->getRepr();
        if (!repr) {
            std::cerr << "Could not get the repr of the content of the element with id=\"num_tests\".\n";
            return;
        }
        auto text = repr->content();
        if (!text) {
            std::cerr << "Could not get the text content of the element with id=\"num_tests\".\n";
            return;
        }
        try {
            _test_count = std::stoul(text);
        } catch (std::invalid_argument const &e) {
            std::cerr << "Could not parse an integer from the content of element with id=\"num_tests\".\n";
            return;
        }
    }

    InkscapeInit _init;
    std::unique_ptr<SPDocument> _document;
    size_t _test_count = 0;
};

TEST_F(VisualBoundsTest, ShapeBounds)
{
    size_t const id_maxlen = 7 + 1;
    char object_id[id_maxlen], bbox_id[id_maxlen];
    double const epsilon = 1e-4;

    for (size_t i = 1; i<= testCount(); i++) {
        snprintf(object_id, id_maxlen, "obj-%lu", i);
        snprintf(bbox_id, id_maxlen, "vbb-%lu", i);

        auto const *item = getItemById(object_id);
        auto const *bbox = getItemById(bbox_id);
        ASSERT_TRUE(item && bbox);

        Geom::Rect const expected_bbox = cast<SPRect>(bbox)->getRect();

        auto const actual_opt_bbox = item->visualBounds(item->transform);
        ASSERT_TRUE(bool(actual_opt_bbox));
        Geom::Rect const actual_bbox = *actual_opt_bbox;

        // Check that the item's visual bounding box is as expected, up to epsilon.
        for (auto const dim : {Geom::X, Geom::Y}) {
            EXPECT_GE(actual_bbox[dim].min(), expected_bbox[dim].min() - epsilon);
            EXPECT_LE(actual_bbox[dim].min(), expected_bbox[dim].min() + epsilon);

            EXPECT_GE(actual_bbox[dim].max(), expected_bbox[dim].max() - epsilon);
            EXPECT_LE(actual_bbox[dim].max(), expected_bbox[dim].max() + epsilon);
        }
    }
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