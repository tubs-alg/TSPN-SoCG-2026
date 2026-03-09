// Tests for WKT parsing functions in tspn/common.h and tspn/types.h
//
// Covers parse_site_wkt for Point, Polygon, Ring, Circle types
// and the Circle WKT constructor. All previously untested.

#include <gtest/gtest.h>

#include "tspn/common.h"

namespace tspn {

// --- parse_site_wkt: Point ---

TEST(WktParsing, ParsePoint) {
  auto site = parse_site_wkt("POINT (3 4)");
  ASSERT_TRUE(std::holds_alternative<Point>(site));
  const auto &p = std::get<Point>(site);
  EXPECT_DOUBLE_EQ(p.get<0>(), 3.0);
  EXPECT_DOUBLE_EQ(p.get<1>(), 4.0);
}

TEST(WktParsing, ParsePointNegativeCoords) {
  auto site = parse_site_wkt("POINT (-1.5 -2.5)");
  ASSERT_TRUE(std::holds_alternative<Point>(site));
  const auto &p = std::get<Point>(site);
  EXPECT_DOUBLE_EQ(p.get<0>(), -1.5);
  EXPECT_DOUBLE_EQ(p.get<1>(), -2.5);
}

// --- parse_site_wkt: Polygon (no holes => Ring) ---

TEST(WktParsing, ParseSimplePolygonBecomesRing) {
  auto site = parse_site_wkt("POLYGON ((0 0, 4 0, 4 4, 0 4, 0 0))");
  // Simple polygon without holes is stored as Ring
  ASSERT_TRUE(std::holds_alternative<Ring>(site));
  const auto &ring = std::get<Ring>(site);
  EXPECT_GE(ring.size(), 4u);
}

// --- parse_site_wkt: Polygon with holes ---

TEST(WktParsing, ParsePolygonWithHoles) {
  auto site = parse_site_wkt(
      "POLYGON ((0 0, 10 0, 10 10, 0 10, 0 0), (3 3, 7 3, 7 7, 3 7, 3 3))");
  ASSERT_TRUE(std::holds_alternative<Polygon>(site));
  const auto &poly = std::get<Polygon>(site);
  EXPECT_EQ(poly.inners().size(), 1u);
}

// --- parse_site_wkt: Circle ---

TEST(WktParsing, ParseCircle) {
  auto site = parse_site_wkt("CIRCLE ((3 4), 5.0)");
  ASSERT_TRUE(std::holds_alternative<Circle>(site));
  const auto &c = std::get<Circle>(site);
  EXPECT_DOUBLE_EQ(c.center().get<0>(), 3.0);
  EXPECT_DOUBLE_EQ(c.center().get<1>(), 4.0);
  EXPECT_DOUBLE_EQ(c.radius(), 5.0);
}

TEST(WktParsing, ParseCircleWithPointKeyword) {
  // Circle WKT with explicit POINT keyword
  auto site = parse_site_wkt("CIRCLE (POINT (1 2), 3.0)");
  ASSERT_TRUE(std::holds_alternative<Circle>(site));
  const auto &c = std::get<Circle>(site);
  EXPECT_DOUBLE_EQ(c.center().get<0>(), 1.0);
  EXPECT_DOUBLE_EQ(c.center().get<1>(), 2.0);
  EXPECT_DOUBLE_EQ(c.radius(), 3.0);
}

// --- parse_site_wkt: errors ---

TEST(WktParsing, UnrecognizedTypeThrows) {
  EXPECT_THROW(parse_site_wkt("LINESTRING (0 0, 1 1)"), std::invalid_argument);
}

// --- Circle constructor ---

TEST(CircleWkt, BasicConstruction) {
  Circle c(Point(1, 2), 3.0);
  EXPECT_DOUBLE_EQ(c.center().get<0>(), 1.0);
  EXPECT_DOUBLE_EQ(c.center().get<1>(), 2.0);
  EXPECT_DOUBLE_EQ(c.radius(), 3.0);
}

TEST(CircleWkt, WktConstructorRejectsNonCircle) {
  EXPECT_THROW(Circle("POINT (1 2)"), std::invalid_argument);
}

} // namespace tspn
