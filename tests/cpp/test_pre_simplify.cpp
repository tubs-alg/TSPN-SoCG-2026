// Tests for instance pre-simplification in tspn/details/pre_simplify.h
//
// Covers remove_supersites, remove_holes, and the combined simplify pipeline.

#include <gtest/gtest.h>

#include "tspn_core/details/pre_simplify.h"
#include "tspn_core/utils/geometry.h"

namespace tspn::details {

TEST(PreSimplify, RemoveSupersitesDropsContainingSite) {
  // Small ring inside a big ring: big ring should be removed
  std::vector<SiteVariant> sites = {
      Ring{{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}}, // big
      Ring{{3, 3}, {7, 3}, {7, 7}, {3, 7}, {3, 3}},     // small, inside big
  };
  for (auto &s : sites)
    tspn::utils::correct(s);
  remove_supersites(sites);
  // The big site contains the small one, so it should be removed
  EXPECT_EQ(sites.size(), 1u);
}

TEST(PreSimplify, RemoveSupersitesKeepsNonOverlapping) {
  std::vector<SiteVariant> sites = {
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
      Ring{{5, 5}, {7, 5}, {7, 7}, {5, 7}, {5, 5}},
  };
  for (auto &s : sites)
    tspn::utils::correct(s);
  remove_supersites(sites);
  EXPECT_EQ(sites.size(), 2u);
}

TEST(PreSimplify, RemoveHolesDropsUnnecessaryHole) {
  // Polygon with a hole, and another site far away from the hole.
  // The hole's interior (corrected to a ring) must have positive distance
  // to at least one other site for the hole to be deemed unnecessary.
  // Here we place a site completely outside the hole area so it has
  // positive distance to the hole interior.
  Polygon poly;
  bg::append(poly.outer(), Point(0, 0));
  bg::append(poly.outer(), Point(20, 0));
  bg::append(poly.outer(), Point(20, 20));
  bg::append(poly.outer(), Point(0, 20));
  bg::append(poly.outer(), Point(0, 0));
  poly.inners().resize(1);
  bg::append(poly.inners()[0], Point(8, 8));
  bg::append(poly.inners()[0], Point(12, 8));
  bg::append(poly.inners()[0], Point(12, 12));
  bg::append(poly.inners()[0], Point(8, 12));
  bg::append(poly.inners()[0], Point(8, 8));
  bg::correct(poly);
  std::vector<SiteVariant> sites = {
      SiteVariant(poly),
      // A site far from the hole — its distance to the hole interior is
      // positive
      Ring{{30, 30}, {32, 30}, {32, 32}, {30, 32}, {30, 30}},
  };
  for (auto &s : sites)
    tspn::utils::correct(s);
  remove_holes(sites);
  // The hole should be removed since another site has positive distance
  if (std::holds_alternative<Polygon>(sites[0])) {
    EXPECT_EQ(std::get<Polygon>(sites[0]).inners().size(), 0u);
  } else {
    EXPECT_TRUE(std::holds_alternative<Ring>(sites[0]));
  }
}

TEST(PreSimplify, SimplifyPipeline) {
  // Two overlapping rings, one containing the other
  std::vector<SiteVariant> sites = {
      Ring{{0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}},
      Ring{{2, 2}, {8, 2}, {8, 8}, {2, 8}, {2, 2}},
  };
  for (auto &s : sites)
    tspn::utils::correct(s);
  auto result = simplify(sites);
  // The large site containing the small one should be removed
  EXPECT_EQ(result.size(), 1u);
}

} // namespace tspn::details
