// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LPE tests
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2020 Authors
 *
 * Released under GNU GPL version 2 or later, read the file 'COPYING' for more information
 */

#include <gtest/gtest.h>
#include <testfiles/lpespaths-test.h>
#include <src/document.h>
#include <src/inkscape.h>
#include <src/live_effects/lpe-bool.h>
#include <src/object/sp-ellipse.h>
#include <src/object/sp-lpe-item.h>

using namespace Inkscape;
using namespace Inkscape::LivePathEffect;

class LPETest : public LPESPathsTest {
public:
    void run() {
        testDoc(svg);
    }
};

// A) FILE BASED TESTS
TEST_F(LPETest, Inkscape_0_92) { run(); }
TEST_F(LPETest, Inkscape_1_0)  { run(); }
TEST_F(LPETest, Inkscape_1_1)  { run(); }
TEST_F(LPETest, Inkscape_1_2)  { run(); }
TEST_F(LPETest, Inkscape_1_3)  { run(); }
// B) CUSTOM TESTS
// BOOL LPE
TEST_F(LPETest, Bool_canBeApplyedToNonSiblingPaths)
{
    std::string svg("\
<svg width='100' height='100'\
  xmlns:sodipodi='http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd'\
  xmlns:inkscape='http://www.inkscape.org/namespaces/inkscape'>\
  <defs>\
    <inkscape:path-effect\
      id='path-effect1'\
      effect='bool_op'\
      operation='diff'\
      operand-path='#circle1'\
      lpeversion='1'\
      hide-linked='true' />\
  </defs>\
  <path id='rect1'\
    inkscape:path-effect='#path-effect1'\
    sodipodi:type='rect'\
    width='100' height='100' fill='#ff0000' />\
  <g id='group1'>\
    <circle id='circle1'\
      r='40' cy='50' cx='50' fill='#ffffff' style='display:inline'/>\
  </g>\
</svg>");

    SPDocument *doc = SPDocument::createNewDocFromMem(svg.c_str(), svg.size(), true);
    doc->ensureUpToDate();

    auto lpe_item = cast<SPLPEItem>(doc->getObjectById("rect1"));
    ASSERT_TRUE(lpe_item != nullptr);

    auto lpe_bool_op_effect = dynamic_cast<LPEBool *>(lpe_item->getFirstPathEffectOfType(EffectType::BOOL_OP));
    ASSERT_TRUE(lpe_bool_op_effect != nullptr);

    auto operand_path = lpe_bool_op_effect->getParameter("operand-path")->param_getSVGValue();
    auto circle = cast<SPGenericEllipse>(doc->getObjectById(operand_path.substr(1)));
    ASSERT_TRUE(circle != nullptr);
}