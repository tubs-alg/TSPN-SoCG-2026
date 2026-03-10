// Tests for convex partitioning in tspn/details/convex_partition.h
//
// Covers partition() for different geometry types: convex ring (no-op),
// non-convex ring (partitioned), polygon with holes (decomposed),
// points and circles (empty result).

#include <gtest/gtest.h>

#include "tspn_core/details/convex_partition.h"
#include "tspn_core/utils/geometry.h"
#include <boost/geometry/algorithms/is_convex.hpp>

namespace tspn::details {

TEST(ConvexPartition, PointReturnsEmpty) {
  SiteVariant site = Point(0, 0);
  auto result = partition(site);
  EXPECT_TRUE(result.empty());
}

TEST(ConvexPartition, CircleReturnsEmpty) {
  SiteVariant site = Circle(Point(0, 0), 5.0);
  auto result = partition(site);
  EXPECT_TRUE(result.empty());
}

TEST(ConvexPartition, ConvexRingReturnsEmpty) {
  // A convex ring needs no partition
  Ring ring{{0, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0}};
  bg::correct(ring);
  SiteVariant site = ring;
  auto result = partition(site);
  EXPECT_TRUE(result.empty());
}

TEST(ConvexPartition, NonConvexRingPartitioned) {
  // L-shaped ring: should be partitioned into convex pieces
  Ring ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}};
  bg::correct(ring);
  SiteVariant site = ring;
  auto result = partition(site);
  EXPECT_GE(result.size(), 2u);
  // Each piece should be a convex ring
  for (const auto &piece : result) {
    ASSERT_TRUE(std::holds_alternative<Ring>(piece));
    EXPECT_TRUE(bg::is_convex(std::get<Ring>(piece)));
  }
}

TEST(ConvexPartition, ConvexPolygonReturnsEmpty) {
  Polygon poly{{{0, 0}, {4, 0}, {4, 4}, {0, 4}}};
  bg::correct(poly);
  SiteVariant site = poly;
  auto result = partition(site);
  EXPECT_TRUE(result.empty());
}

TEST(ConvexPartition, NonConvexPolygonNoHoles) {
  // L-shaped polygon without holes
  Polygon poly{{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}}};
  bg::correct(poly);
  SiteVariant site = poly;
  auto result = partition(site);
  EXPECT_GE(result.size(), 2u);
  for (const auto &piece : result) {
    ASSERT_TRUE(std::holds_alternative<Ring>(piece));
    EXPECT_TRUE(bg::is_convex(std::get<Ring>(piece)));
  }
}

TEST(ConvexPartition, PolygonWithHolesDecomposed) {
  // Square with a hole: uses decompose fallback
  Polygon poly;
  bg::append(poly.outer(), Point(0, 0));
  bg::append(poly.outer(), Point(10, 0));
  bg::append(poly.outer(), Point(10, 10));
  bg::append(poly.outer(), Point(0, 10));
  bg::append(poly.outer(), Point(0, 0));
  poly.inners().resize(1);
  bg::append(poly.inners()[0], Point(3, 3));
  bg::append(poly.inners()[0], Point(7, 3));
  bg::append(poly.inners()[0], Point(7, 7));
  bg::append(poly.inners()[0], Point(3, 7));
  bg::append(poly.inners()[0], Point(3, 3));
  bg::correct(poly);
  SiteVariant site = poly;
  auto result = partition(site);
  EXPECT_GE(result.size(), 1u);
  // Each piece should be convex
  for (const auto &piece : result) {
    ASSERT_TRUE(std::holds_alternative<Ring>(piece));
    EXPECT_TRUE(bg::is_convex(std::get<Ring>(piece)));
  }
}

TEST(ConvexPartition, TriangleIsConvex) {
  Ring ring{{0, 0}, {4, 0}, {2, 3}, {0, 0}};
  bg::correct(ring);
  SiteVariant site = ring;
  auto result = partition(site);
  // Triangle is convex, no partition needed
  EXPECT_TRUE(result.empty());
}

} // namespace tspn::details
