// Tests for Instance, Geometry, and TourElement from tspn/common.h
//
// Covers construction with different geometry types, TourElement operations
// (enable/disable, branch, equality), and Instance path/tour modes.

#include <gtest/gtest.h>

#include "tspn_core/common.h"

namespace tspn {

// --- Instance construction with different geometry types ---

TEST(Instance, ConstructWithPolygons) {
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{5, 0}, {6, 0}, {5, 1}}},
  };
  Instance inst(sites);
  EXPECT_EQ(inst.size(), 2u);
  EXPECT_TRUE(inst.is_tour());
  EXPECT_FALSE(inst.is_path());
}

TEST(Instance, ConstructAsPath) {
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{5, 0}, {6, 0}, {5, 1}}},
      Polygon{{{3, 3}, {4, 3}, {3, 4}}},
  };
  Instance inst(sites, /*path=*/true);
  EXPECT_TRUE(inst.is_path());
  EXPECT_FALSE(inst.is_tour());
}

TEST(Instance, ConstructWithPoints) {
  auto sites = std::vector<SiteVariant>{
      Point(0, 0),
      Point(5, 0),
      Point(5, 5),
  };
  Instance inst(sites);
  EXPECT_EQ(inst.size(), 3u);
  // Points are always convex
  for (const auto &geo : inst) {
    EXPECT_TRUE(geo.is_convex());
  }
}

TEST(Instance, ConstructWithCircles) {
  auto sites = std::vector<SiteVariant>{
      Circle(Point(0, 0), 1.0),
      Circle(Point(5, 0), 2.0),
  };
  Instance inst(sites);
  EXPECT_EQ(inst.size(), 2u);
  for (const auto &geo : inst) {
    EXPECT_TRUE(geo.is_convex());
  }
}

TEST(Instance, ConstructWithRings) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
      Ring{{5, 0}, {7, 0}, {7, 2}, {5, 2}, {5, 0}},
  };
  Instance inst(sites);
  EXPECT_EQ(inst.size(), 2u);
  for (const auto &geo : inst) {
    EXPECT_TRUE(geo.is_convex());
  }
}

TEST(Instance, ConstructWithMixedTypes) {
  auto sites = std::vector<SiteVariant>{
      Point(0, 0),
      Circle(Point(5, 0), 1.0),
      Ring{{10, 0}, {12, 0}, {12, 2}, {10, 2}, {10, 0}},
      Polygon{{{15, 0}, {17, 0}, {17, 2}, {15, 2}}},
  };
  Instance inst(sites);
  EXPECT_EQ(inst.size(), 4u);
}

TEST(Instance, AddSiteIncreasesSize) {
  Instance inst;
  EXPECT_EQ(inst.size(), 0u);
  Point p(0, 0);
  inst.add_site(p);
  EXPECT_EQ(inst.size(), 1u);
  Circle c(Point(5, 0), 1.0);
  inst.add_site(c);
  EXPECT_EQ(inst.size(), 2u);
}

TEST(Instance, RevisionTracksAdditions) {
  Instance inst;
  EXPECT_EQ(inst.revision, 0u);
  Point p(0, 0);
  inst.add_site(p);
  EXPECT_EQ(inst.revision, 1u);
  Circle c(Point(5, 0), 1.0);
  inst.add_site(c);
  EXPECT_EQ(inst.revision, 2u);
}

// --- Geometry properties ---

TEST(Geometry, ConvexPolygonDetected) {
  Polygon poly{{{0, 0}, {4, 0}, {4, 4}, {0, 4}}};
  bg::correct(poly);
  Geometry geo(0, poly);
  EXPECT_TRUE(geo.is_convex());
  EXPECT_FALSE(geo.has_holes());
}

TEST(Geometry, NonConvexPolygonDetected) {
  // L-shaped polygon
  Ring ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}};
  bg::correct(ring);
  Geometry geo(0, ring);
  EXPECT_FALSE(geo.is_convex());
  // Non-convex polygons should produce a decomposition
  EXPECT_GT(geo.decomposition().size(), 0u);
}

TEST(Geometry, PolygonWithHoles) {
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
  Geometry geo(0, poly);
  EXPECT_TRUE(geo.has_holes());
  EXPECT_FALSE(geo.is_convex());
}

TEST(Geometry, GeoIndexPreserved) {
  Polygon poly{{{0, 0}, {1, 0}, {0, 1}}};
  Geometry geo(42, poly);
  EXPECT_EQ(geo.geo_index(), 42u);
}

TEST(Geometry, HullInstance) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}},
      Ring{{10, 0}, {12, 0}, {12, 2}, {10, 2}, {10, 0}},
  };
  Instance inst(sites);
  Instance hull_inst = inst.hull_instance();
  EXPECT_EQ(hull_inst.size(), inst.size());
  // First geometry was non-convex, hull should be convex
  EXPECT_TRUE(hull_inst[0].is_convex());
}

// --- TourElement ---

TEST(TourElement, BasicConstruction) {
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{5, 0}, {6, 0}, {5, 1}}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_EQ(te.geo_index(), 0u);
  EXPECT_TRUE(te.is_active());
  EXPECT_FALSE(te.is_exact());
}

TEST(TourElement, EnableDisable) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_active());

  te.disable();
  EXPECT_FALSE(te.is_active());

  te.enable();
  EXPECT_TRUE(te.is_active());
}

TEST(TourElement, CanBeDisabledRespectsToggleCount) {
  constants::reset_parameters();
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.can_be_disabled());

  // Disable then re-enable uses up one toggle
  te.disable();
  te.enable();
  // After one toggle cycle, should not be disableable anymore
  // (MAX_TE_TOGGLE_COUNT=1)
  EXPECT_FALSE(te.can_be_disabled());
}

TEST(TourElement, DisableAlreadyDisabledIsNoop) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  te.disable();
  EXPECT_FALSE(te.is_active());
  te.disable(); // Should not change anything
  EXPECT_FALSE(te.is_active());
}

TEST(TourElement, EnableAlreadyEnabledIsNoop) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_active());
  te.enable(); // Should not change anything or increment counters
  EXPECT_TRUE(te.is_active());
}

TEST(TourElement, Equality) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
      Ring{{5, 0}, {7, 0}, {7, 2}, {5, 2}, {5, 0}},
  };
  Instance inst(sites);
  TourElement te1(inst, 0);
  TourElement te2(inst, 0);
  TourElement te3(inst, 1);
  EXPECT_TRUE(te1 == te2);
  EXPECT_FALSE(te1 == te3);
  EXPECT_TRUE(te1 != te3);
}

TEST(TourElement, ListOrder) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}},
      Ring{{5, 0}, {7, 0}, {7, 2}, {5, 2}, {5, 0}},
  };
  Instance inst(sites);
  TourElement te0(inst, 0);
  TourElement te1(inst, 1);
  EXPECT_TRUE(te0.listorder(te1));
  EXPECT_FALSE(te1.listorder(te0));
}

TEST(TourElement, BranchNonConvex) {
  // Use a non-convex ring so branching produces decomposition elements
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}},
  };
  Instance inst(sites);
  EXPECT_FALSE(inst[0].is_convex());
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_lazy()); // Non-convex => lazy
  auto branches = te.branch();
  EXPECT_GT(branches.size(), 0u);
  for (const auto &b : branches) {
    EXPECT_FALSE(b.is_lazy());
    EXPECT_EQ(b.geo_index(), 0u);
  }
}

TEST(TourElement, SetExactNonConvex) {
  auto sites = std::vector<SiteVariant>{
      Ring{{0, 0}, {4, 0}, {4, 2}, {2, 2}, {2, 4}, {0, 4}, {0, 0}},
  };
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_lazy());
  auto exact_te = te.set_exact();
  EXPECT_TRUE(exact_te.is_exact());
  EXPECT_FALSE(exact_te.is_lazy());
}

} // namespace tspn
