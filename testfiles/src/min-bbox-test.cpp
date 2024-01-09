// SPDX-License-Identifier: GPL-2.0-or-later
#include <2geom/convex-hull.h>
#include <2geom/transforms.h>
#include <gtest/gtest.h>
#include <helper/geom.h>

// Get the axis-aligned bouding box of a set of points, transforming by affine first.
auto aligned_bbox(std::vector<Geom::Point> const &pts, Geom::Affine const &affine = Geom::identity())
{
    Geom::OptRect rect;
    for (auto &pt : pts) {
        rect.expandTo(pt * affine);
    }
    return rect;
}

double area(Geom::OptRect const &rect)
{
    return rect ? rect->area() : 0.0;
}

// Get an approximation to the minimum bouding box area.
double approx_min(std::vector<Geom::Point> const &pts)
{
    int constexpr N = 100;

    double min = std::numeric_limits<double>::max();

    for (int i = 0; i < N; i++) {
        auto t = (double)i / N * M_PI * 0.5;
        min = std::min(min, area(aligned_bbox(pts, Geom::Rotate(t))));
    }

    return min;
}

// Get a crude random double.
double ranf()
{
    int constexpr N = 1000;
    return (double)(rand() % N) / N;
}

// Get a random collection of points.
auto randpts()
{
    std::vector<Geom::Point> pts;

    int count = 5 + (rand() % 10);
    for (int i = 0; i < count; i++) {
        pts.emplace_back(ranf(), ranf());
    }

    return pts;
}

TEST(MinBBoxTest, random)
{
    for (int i = 0; i < 100; i++) {
        auto const pts = randpts();
        auto [affine, rect] = min_bounding_box(pts);

        ASSERT_TRUE(affine.isRotation());

        auto rect2 = aligned_bbox(pts, affine);
        for (int i = 0; i < 2; i++) {
            ASSERT_NEAR(rect.min()[i], rect2->min()[i], 1e-5);
            ASSERT_NEAR(rect.max()[i], rect2->max()[i], 1e-5);
        }

        ASSERT_LE(rect.area(), approx_min(pts));
    }
}
