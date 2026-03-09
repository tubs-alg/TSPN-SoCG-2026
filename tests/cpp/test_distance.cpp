// Tests for distance template specializations in tspn/utils/distance.h
//
// Covers all Circle-involving distance computations and SiteVariant dispatch,
// which previously had zero test coverage.

#include <gtest/gtest.h>

#include "tspn/utils/distance.h"

namespace tspn::utils {

// --- Circle-Point ---

TEST(Distance, CircleToPointInside) {
  Circle c(Point(0, 0), 5.0);
  Point p(1, 0);
  EXPECT_DOUBLE_EQ(distance(c, p), 0.0);
}

TEST(Distance, CircleToPointOnBoundary) {
  Circle c(Point(0, 0), 5.0);
  Point p(5, 0);
  EXPECT_DOUBLE_EQ(distance(c, p), 0.0);
}

TEST(Distance, CircleToPointOutside) {
  Circle c(Point(0, 0), 3.0);
  Point p(5, 0);
  EXPECT_NEAR(distance(c, p), 2.0, 1e-9);
}

TEST(Distance, PointToCircle) {
  Circle c(Point(0, 0), 3.0);
  Point p(5, 0);
  EXPECT_NEAR(distance(p, c), 2.0, 1e-9);
}

// --- Circle-Segment ---

TEST(Distance, CircleToSegmentIntersecting) {
  Circle c(Point(0, 0), 3.0);
  Segment s(Point(-1, 0), Point(1, 0));
  EXPECT_DOUBLE_EQ(distance(c, s), 0.0);
}

TEST(Distance, CircleToSegmentOutside) {
  Circle c(Point(0, 0), 2.0);
  Segment s(Point(5, 0), Point(5, 5));
  EXPECT_NEAR(distance(c, s), 3.0, 1e-9);
}

TEST(Distance, SegmentToCircle) {
  Circle c(Point(0, 0), 2.0);
  Segment s(Point(5, 0), Point(5, 5));
  EXPECT_NEAR(distance(s, c), 3.0, 1e-9);
}

// --- Circle-Linestring ---

TEST(Distance, CircleToLinestringIntersecting) {
  Circle c(Point(0, 0), 3.0);
  Linestring ls{{-10, 0}, {10, 0}};
  EXPECT_DOUBLE_EQ(distance(c, ls), 0.0);
}

TEST(Distance, CircleToLinestringFarAway) {
  Circle c(Point(0, 0), 1.0);
  Linestring ls{{5, 0}, {5, 5}};
  EXPECT_NEAR(distance(c, ls), 4.0, 1e-9);
}

TEST(Distance, LinestringToCircle) {
  Circle c(Point(0, 0), 1.0);
  Linestring ls{{5, 0}, {5, 5}};
  EXPECT_NEAR(distance(ls, c), 4.0, 1e-9);
}

// --- Circle-Ring ---

TEST(Distance, CircleToRingOverlapping) {
  Circle c(Point(0, 0), 10.0);
  Ring ring{{1, 1}, {2, 1}, {2, 2}, {1, 2}, {1, 1}};
  EXPECT_DOUBLE_EQ(distance(c, ring), 0.0);
}

TEST(Distance, CircleToRingSeparate) {
  Circle c(Point(0, 0), 1.0);
  Ring ring{{5, 0}, {6, 0}, {6, 1}, {5, 1}, {5, 0}};
  EXPECT_NEAR(distance(c, ring), 4.0, 1e-9);
}

TEST(Distance, RingToCircle) {
  Circle c(Point(0, 0), 1.0);
  Ring ring{{5, 0}, {6, 0}, {6, 1}, {5, 1}, {5, 0}};
  EXPECT_NEAR(distance(ring, c), 4.0, 1e-9);
}

// --- Circle-Polygon ---

TEST(Distance, CircleToPolygonSeparate) {
  Circle c(Point(0, 0), 1.0);
  Polygon poly{{{5, 0}, {6, 0}, {6, 1}, {5, 1}}};
  bg::correct(poly);
  // Distance from circle edge to nearest polygon point
  EXPECT_NEAR(distance(c, poly), 4.0, 1e-9);
}

TEST(Distance, PolygonToCircle) {
  Circle c(Point(0, 0), 1.0);
  Polygon poly{{{5, 0}, {6, 0}, {6, 1}, {5, 1}}};
  bg::correct(poly);
  EXPECT_NEAR(distance(poly, c), 4.0, 1e-9);
}

// --- Circle-Circle ---

TEST(Distance, CircleToCircleOverlapping) {
  Circle c1(Point(0, 0), 3.0);
  Circle c2(Point(2, 0), 3.0);
  EXPECT_DOUBLE_EQ(distance(c1, c2), 0.0);
}

TEST(Distance, CircleToCircleSeparate) {
  Circle c1(Point(0, 0), 1.0);
  Circle c2(Point(5, 0), 1.0);
  EXPECT_NEAR(distance(c1, c2), 3.0, 1e-9);
}

TEST(Distance, CircleToCircleTouching) {
  Circle c1(Point(0, 0), 2.5);
  Circle c2(Point(5, 0), 2.5);
  EXPECT_DOUBLE_EQ(distance(c1, c2), 0.0);
}

// --- SiteVariant dispatch ---

TEST(Distance, SiteVariantPointToPoint) {
  SiteVariant v1 = Point(0, 0);
  SiteVariant v2 = Point(3, 4);
  EXPECT_NEAR(distance(v1, v2), 5.0, 1e-9);
}

TEST(Distance, SiteVariantCircleToPoint) {
  SiteVariant v1 = Circle(Point(0, 0), 2.0);
  SiteVariant v2 = Point(5, 0);
  EXPECT_NEAR(distance(v1, v2), 3.0, 1e-9);
}

TEST(Distance, SiteVariantRingToRing) {
  Ring r1{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
  Ring r2{{5, 0}, {6, 0}, {6, 1}, {5, 1}, {5, 0}};
  SiteVariant v1 = r1;
  SiteVariant v2 = r2;
  EXPECT_NEAR(distance(v1, v2), 4.0, 1e-9);
}

TEST(Distance, PointToSiteVariant) {
  Point p(5, 0);
  SiteVariant v = Circle(Point(0, 0), 2.0);
  EXPECT_NEAR(distance(p, v), 3.0, 1e-9);
}

TEST(Distance, SiteVariantToPoint) {
  SiteVariant v = Circle(Point(0, 0), 2.0);
  Point p(5, 0);
  EXPECT_NEAR(distance(v, p), 3.0, 1e-9);
}

TEST(Distance, LinestringToSiteVariantCircle) {
  Linestring ls{{10, 0}, {10, 5}};
  SiteVariant v = Circle(Point(0, 0), 3.0);
  EXPECT_NEAR(distance(ls, v), 7.0, 1e-9);
}

} // namespace tspn::utils
