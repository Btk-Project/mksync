#include <gtest/gtest.h>

#include "mksync/core/math_types.hpp"

using namespace mks::base;

TEST(Math, Point)
{
    Point pt1(0, 0);
    Point pt2(1, 1);
    Point pt3 = pt1 + pt2;
    EXPECT_EQ(pt3.x, 1);
    EXPECT_EQ(pt3.y, 1);

    pt3 = Vec2(pt1) - Vec2(pt2);
    EXPECT_EQ(pt3.x, -1);
    EXPECT_EQ(pt3.y, -1);

    pt3 = pt2 * 2;
    EXPECT_EQ(pt3.x, 2);
    EXPECT_EQ(pt3.y, 2);

    auto pt4 = pt2 / 2.0;
    EXPECT_EQ(pt4.x, 0.5);
    EXPECT_EQ(pt4.y, 0.5);

    Point pt5(1.114514, 2);
    EXPECT_EQ(pt5.x, 1.114514);
    EXPECT_EQ(pt5.y, 2);

    Point pt6(314728U, 2);
    EXPECT_EQ(pt6.x, 314728);
    EXPECT_EQ(pt6.y, 2);

    auto pt7 = Vec2(pt6) + Vec2(pt5);
    EXPECT_DOUBLE_EQ(pt7.x, 314729.114514);
    EXPECT_DOUBLE_EQ(pt7.y, 4);
}

TEST(Math, Vec)
{
    Vec2 v1(1, 1);
    Vec2 v2(2.1F, 34U);
    Vec2 v3 = v1 + v2;
    EXPECT_FLOAT_EQ(v3.x, 3.1F);
    EXPECT_FLOAT_EQ(v3.y, 35);

    v3 = v1 - v2;
    EXPECT_FLOAT_EQ(v3.x, -1.1F);
    EXPECT_FLOAT_EQ(v3.y, -33);

    v3 = v2 * 2;
    EXPECT_FLOAT_EQ(v3.x, 4.2F);
    EXPECT_FLOAT_EQ(v3.y, 68);

    auto v4 = v1 / 2.0;
    EXPECT_DOUBLE_EQ(v4.x, 0.5);
    EXPECT_DOUBLE_EQ(v4.y, 0.5);

    Vec2 v5(123U, 1255);
    EXPECT_EQ(v5.x, 123);
    EXPECT_EQ(v5.y, 1255);

    auto v6 = v5.cross(v1);
    EXPECT_EQ(v6, (123 * 1) - (1255 * 1));

    auto v7 = v5.dot(v1);
    EXPECT_EQ(v7, (123 * 1) + (1255 * 1));
}

TEST(Math, Rect)
{
    Rect r1(0, 0, 1.5, 1.5);
    Rect r2(1, 1, 2, 2);
    EXPECT_EQ(r1.left(), 0);
    EXPECT_EQ(r1.top(), 0);
    EXPECT_EQ(r1.right(), 1.5);
    EXPECT_EQ(r1.bottom(), 1.5);

    EXPECT_EQ(r2.left(), 1);
    EXPECT_EQ(r2.top(), 1);
    EXPECT_EQ(r2.right(), 3);
    EXPECT_EQ(r2.bottom(), 3);
    Rect r3 = Rect<>::minimum_bounding_rectangle(r1, r2);
    EXPECT_EQ(r3.pos.x, 0);
    EXPECT_EQ(r3.pos.x, 0);
    EXPECT_EQ(r3.size.width, 3);
    EXPECT_EQ(r3.size.height, 3);

    EXPECT_TRUE(r1.contains(Point(0, 0)));
    EXPECT_FALSE(r1.contains(Point(2, 2)));

    EXPECT_TRUE(r1.intersects(r2));
    EXPECT_FALSE(r1.intersects(Rect(2, 2, 1, 1)));
    EXPECT_TRUE(r1.intersects(Rect(0, 0, 1, 1)));
    EXPECT_TRUE(r1.intersects(Rect(-1, -1, 3, 3)));
    EXPECT_TRUE(r1.intersects(Rect(0.5, 0.5, 0.5, 0.5)));
    EXPECT_FALSE(r1.intersects(Rect(1.5, 1.5, 0.5, 0.5)));
    EXPECT_TRUE(r1.intersects(Rect(0.5, 1, 2, 2)));
    EXPECT_FALSE(r1.intersects(Rect(-1, -1, 1, 1)));
    EXPECT_TRUE(r1.intersects(Rect(-1, -1, 1.5, 1.1)));

    EXPECT_EQ(r1, r1);
    EXPECT_NE(r1, r2);
    auto r4 = r1.intersection(r2);
    EXPECT_TRUE(r4 == Rect(1, 1, 0.5, 0.5));
    EXPECT_TRUE(r1.intersection(Rect(-1, -1, 4, 4)) == r1);
    EXPECT_TRUE(r1.intersection(Rect(0, 0, 1, 1)) == Rect(0, 0, 1, 1));
    EXPECT_TRUE(r1.intersection(Rect(0.5, 0.5, 0.5, 0.5)) == Rect(0.5, 0.5, 0.5, 0.5));
    EXPECT_TRUE(r1.intersection(Rect(1.5, 1.5, 0.5, 0.5)) == Rect<int>());
    EXPECT_TRUE(r1.intersection(Rect(0.5, 1, 2, 2)) == Rect(0.5, 1, 1, 0.5));
}

TEST(Math, Line)
{
    Line l1(Point(0, 0), Point(1, 1));
    EXPECT_EQ(l1.p1, Point(0, 0));
    EXPECT_EQ(l1.p2, Point(1, 1));
    EXPECT_EQ(l1.length(), 1.4142135623730951);
    EXPECT_EQ(l1.k(), 1.0);
    EXPECT_EQ(l1.b(), 0.0);

    auto pt = l1.intersect(Line(Point(0.0, 1.0), Point(1.0, 0.0)));
    EXPECT_TRUE(pt == Point(0.5, 0.5));

    pt = l1.intersect(Line(Point(0.0, 1.0), Point(1.0, 2.0)));
    EXPECT_TRUE(pt == Point<int>());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}