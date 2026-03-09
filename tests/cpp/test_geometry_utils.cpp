// Tests for geometry utility functions in tspn/utils/geometry.h
//
// Covers bbox, is_convex, convex_hull, correct, equals, to_segments, to_ring
// which had minimal or zero test coverage.

#include <gtest/gtest.h>

#include "tspn/utils/geometry.h"

namespace tspn::utils {

// --- bbox ---

TEST(GeometryUtils, BboxOfRing) {
  SiteVariant site = Ring{{0, 0}, {4, 0}, {4, 3}, {0, 3}, {0, 0}};
  Box box = bbox(site);
  EXPECT_DOUBLE_EQ(box.min_corner().get<0>(), 0.0);
  EXPECT_DOUBLE_EQ(box.min_corner().get<1>(), 0.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<0>(), 4.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<1>(), 3.0);
}

TEST(GeometryUtils, BboxOfCircle) {
  SiteVariant site = Circle(Point(5, 5), 2.0);
  Box box = bbox(site);
  EXPECT_DOUBLE_EQ(box.min_corner().get<0>(), 3.0);
  EXPECT_DOUBLE_EQ(box.min_corner().get<1>(), 3.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<0>(), 7.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<1>(), 7.0);
}

TEST(GeometryUtils, BboxOfPoint) {
  SiteVariant site = Point(3, 7);
  Box box = bbox(site);
  EXPECT_DOUBLE_EQ(box.min_corner().get<0>(), 3.0);
  EXPECT_DOUBLE_EQ(box.min_corner().get<1>(), 7.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<0>(), 3.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<1>(), 7.0);
}

TEST(GeometryUtils, BboxOfPolygon) {
  Polygon poly{{{1, 2}, {5, 2}, {5, 8}, {1, 8}}};
  bg::correct(poly);
  SiteVariant site = poly;
  Box box = bbox(site);
  EXPECT_DOUBLE_EQ(box.min_corner().get<0>(), 1.0);
  EXPECT_DOUBLE_EQ(box.min_corner().get<1>(), 2.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<0>(), 5.0);
  EXPECT_DOUBLE_EQ(box.max_corner().get<1>(), 8.0);
}

// --- is_convex ---

TEST(GeometryUtils, ConvexSquareRing) {
  Ring ring{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  bg::correct(ring);
  SiteVariant site = ring;
  EXPECT_TRUE(is_convex(site));
}

TEST(GeometryUtils, NonConvexLShapeRing) {
  // L-shape: not convex
  SiteVariant site =
      Ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}};
  EXPECT_FALSE(is_convex(site));
}

TEST(GeometryUtils, CircleIsAlwaysConvex) {
  SiteVariant site = Circle(Point(0, 0), 5.0);
  EXPECT_TRUE(is_convex(site));
}

TEST(GeometryUtils, PointIsAlwaysConvex) {
  SiteVariant site = Point(3, 4);
  EXPECT_TRUE(is_convex(site));
}

TEST(GeometryUtils, SiteVariantConvexDispatch) {
  Ring sq_ring{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  bg::correct(sq_ring);
  SiteVariant square = sq_ring;
  EXPECT_TRUE(is_convex(square));

  SiteVariant lshape =
      Ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}};
  EXPECT_FALSE(is_convex(lshape));

  SiteVariant circle = Circle(Point(0, 0), 1.0);
  EXPECT_TRUE(is_convex(circle));
}

// --- convex_hull ---

TEST(GeometryUtils, ConvexHullOfConvexRingIsSelf) {
  Ring ring{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  SiteVariant site = ring;
  SiteVariant hull = convex_hull(site);
  ASSERT_TRUE(std::holds_alternative<Ring>(hull));
  const auto &hull_ring = std::get<Ring>(hull);
  // Should have same number of points
  EXPECT_EQ(hull_ring.size(), ring.size());
}

TEST(GeometryUtils, ConvexHullOfNonConvexRing) {
  // L-shape gets a convex hull
  Ring ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}};
  SiteVariant site = ring;
  SiteVariant hull = convex_hull(site);
  ASSERT_TRUE(std::holds_alternative<Ring>(hull));
  EXPECT_TRUE(is_convex(hull));
}

TEST(GeometryUtils, ConvexHullOfCircleIsCircle) {
  Circle c(Point(1, 2), 3.0);
  SiteVariant site = c;
  SiteVariant hull = convex_hull(site);
  ASSERT_TRUE(std::holds_alternative<Circle>(hull));
}

TEST(GeometryUtils, ConvexHullOfPointIsPoint) {
  Point p(3, 4);
  SiteVariant site = p;
  SiteVariant hull = convex_hull(site);
  ASSERT_TRUE(std::holds_alternative<Point>(hull));
}

// --- correct ---

TEST(GeometryUtils, CorrectRingOrientation) {
  // CW ring should get corrected to CCW (or vice versa depending on boost)
  Ring ring{{0, 0}, {0, 4}, {4, 4}, {4, 0}, {0, 0}};
  SiteVariant site = ring;
  correct(site);
  // After correction, should be valid for boost geometry operations
  const auto &corrected = std::get<Ring>(site);
  EXPECT_GE(corrected.size(), 4u);
}

TEST(GeometryUtils, CorrectDoesNothingToPoint) {
  SiteVariant site = Point(3, 4);
  correct(site); // Should not throw
  ASSERT_TRUE(std::holds_alternative<Point>(site));
}

TEST(GeometryUtils, CorrectDoesNothingToCircle) {
  SiteVariant site = Circle(Point(0, 0), 5.0);
  correct(site); // Should not throw
  ASSERT_TRUE(std::holds_alternative<Circle>(site));
}

// --- equals ---

TEST(GeometryUtils, EqualRingsIdentical) {
  Ring a{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  Ring b{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  EXPECT_TRUE(equals(a, b));
}

TEST(GeometryUtils, EqualRingsRotated) {
  Ring a{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  Ring b{{4, 0}, {4, 4}, {0, 4}, {0, 0}, {4, 0}};
  EXPECT_TRUE(equals(a, b));
}

TEST(GeometryUtils, UnequalRingsDifferentSize) {
  Ring a{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  Ring b{{0, 0}, {4, 0}, {4, 4}, {0, 0}};
  EXPECT_FALSE(equals(a, b));
}

TEST(GeometryUtils, UnequalRingsDifferentVertices) {
  Ring a{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  Ring b{{0, 0}, {5, 0}, {5, 5}, {0, 5}, {0, 0}};
  EXPECT_FALSE(equals(a, b));
}

// --- to_segments / to_ring ---

TEST(GeometryUtils, ToSegmentsTriangle) {
  Ring ring{{0, 0}, {3, 0}, {0, 4}, {0, 0}};
  auto segs = to_segments(ring);
  EXPECT_EQ(segs.size(), 3u);
}

TEST(GeometryUtils, ToRingRoundtrip) {
  Ring original{{0, 0}, {3, 0}, {3, 3}, {0, 3}, {0, 0}};
  auto segs = to_segments(original);
  Ring recovered = to_ring(segs);
  EXPECT_EQ(recovered.size(), original.size());
  for (size_t i = 0; i < original.size(); i++) {
    EXPECT_TRUE(bg::equals(original[i], recovered[i]));
  }
}

// --- distance_to_segment (additional edge cases) ---

TEST(GeometryUtils, DistanceToSegmentMidpoint) {
  // Point directly above the midpoint
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {5, 3}), 3.0, 1e-6);
}

TEST(GeometryUtils, DistanceToSegmentEndpointClosest) {
  // Point closest to the second endpoint
  EXPECT_NEAR(distance_to_segment({0, 0}, {3, 0}, {4, 0}), 1.0, 1e-6);
}

TEST(GeometryUtils, DistanceToSegmentOnSegment) {
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {5, 0}), 0.0, 1e-6);
}

} // namespace tspn::utils
