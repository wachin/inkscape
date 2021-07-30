/** @file
 * @brief Unit tests for Interval, OptInterval, IntInterval, OptIntInterval.
 *//*
 * Authors:
 *   Thomas Holder
 *
 * Copyright 2021 Authors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later OR MPL-1.1
 */

#include <2geom/interval.h>
#include <gtest/gtest.h>

TEST(IntervalTest, EqualityTest)
{
    Geom::Interval a(3, 5), a2(a), b(4, 7);
    Geom::OptInterval empty, oa = a;

    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a != a);
    EXPECT_TRUE(a == a2);
    EXPECT_FALSE(a != a2);
    EXPECT_TRUE(empty == empty);
    EXPECT_FALSE(empty != empty);
    EXPECT_FALSE(a == empty);
    EXPECT_TRUE(a != empty);
    EXPECT_FALSE(empty == a);
    EXPECT_TRUE(empty != a);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
    EXPECT_TRUE(a == oa);
    EXPECT_FALSE(a != oa);

    Geom::IntInterval ia(3, 5), ia2(ia), ib(4, 7);
    Geom::OptIntInterval iempty, ioa = ia;

    EXPECT_TRUE(ia == ia);
    EXPECT_FALSE(ia != ia);
    EXPECT_TRUE(ia == ia2);
    EXPECT_FALSE(ia != ia2);
    EXPECT_TRUE(iempty == iempty);
    EXPECT_FALSE(iempty != iempty);
    EXPECT_FALSE(ia == iempty);
    EXPECT_TRUE(ia != iempty);
    EXPECT_FALSE(iempty == ia);
    EXPECT_TRUE(iempty != ia);
    EXPECT_FALSE(ia == ib);
    EXPECT_TRUE(ia != ib);
    EXPECT_TRUE(ia == ioa);
    EXPECT_FALSE(ia != ioa);
}

// vim: filetype=cpp:expandtab:shiftwidth=4:softtabstop=4:fileencoding=utf-8:textwidth=99 :
