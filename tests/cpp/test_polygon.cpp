// Tests focused on Polygon handling throughout the codebase.
//
// Covers non-convex polygons, polygons with holes, polygon decomposition
// quality, polygon distances, trajectory coverage of polygons, TourElement
// branching with polygons, BnB solving with non-convex/holed polygons,
// and pre-simplification of polygon instances.

#include <gtest/gtest.h>

#include "tspn/bnb.h"
#include "tspn/details/convex_partition.h"
#include "tspn/details/pre_simplify.h"
#include "tspn/utils/distance.h"

namespace tspn {

// ===================================================================
// Helper: build common polygon shapes
// ===================================================================

// L-shaped polygon (non-convex, no holes)
static Polygon make_L_polygon(double x0, double y0, double size) {
  Polygon p{{{x0, y0},
             {x0 + size, y0},
             {x0 + size, y0 + size / 2},
             {x0 + size / 2, y0 + size / 2},
             {x0 + size / 2, y0 + size},
             {x0, y0 + size}}};
  bg::correct(p);
  return p;
}

// Star-shaped polygon (non-convex, many reflex vertices)
static Polygon make_star_polygon(double cx, double cy, double outer_r,
                                 double inner_r, int points) {
  Ring ring;
  for (int i = 0; i < 2 * points; ++i) {
    double angle = i * M_PI / points;
    double r = (i % 2 == 0) ? outer_r : inner_r;
    bg::append(ring, Point(cx + r * std::cos(angle), cy + r * std::sin(angle)));
  }
  bg::append(ring, ring.front()); // close the ring
  Polygon p;
  bg::assign(p, ring);
  bg::correct(p);
  return p;
}

// Square polygon with a square hole
static Polygon make_polygon_with_hole(double x0, double y0, double outer_size,
                                      double hole_margin) {
  Polygon poly;
  bg::append(poly.outer(), Point(x0, y0));
  bg::append(poly.outer(), Point(x0 + outer_size, y0));
  bg::append(poly.outer(), Point(x0 + outer_size, y0 + outer_size));
  bg::append(poly.outer(), Point(x0, y0 + outer_size));
  bg::append(poly.outer(), Point(x0, y0));
  poly.inners().resize(1);
  bg::append(poly.inners()[0], Point(x0 + hole_margin, y0 + hole_margin));
  bg::append(poly.inners()[0],
             Point(x0 + outer_size - hole_margin, y0 + hole_margin));
  bg::append(poly.inners()[0], Point(x0 + outer_size - hole_margin,
                                     y0 + outer_size - hole_margin));
  bg::append(poly.inners()[0],
             Point(x0 + hole_margin, y0 + outer_size - hole_margin));
  bg::append(poly.inners()[0], Point(x0 + hole_margin, y0 + hole_margin));
  bg::correct(poly);
  return poly;
}

// Simple convex polygon (square)
static Polygon make_square(double x0, double y0, double size) {
  Polygon p{
      {{x0, y0}, {x0 + size, y0}, {x0 + size, y0 + size}, {x0, y0 + size}}};
  bg::correct(p);
  return p;
}

// ===================================================================
// Geometry construction with polygons
// ===================================================================

TEST(Polygon, NonConvexPolygonProperties) {
  Polygon lshape = make_L_polygon(0, 0, 4);
  Geometry geo(0, lshape);
  EXPECT_FALSE(geo.is_convex());
  EXPECT_FALSE(geo.has_holes());
  EXPECT_GT(geo.decomposition().size(), 0u);
  // Each decomposition piece should be convex
  for (const auto &piece : geo.decomposition()) {
    EXPECT_TRUE(utils::is_convex(piece));
  }
}

TEST(Polygon, PolygonWithHoleProperties) {
  Polygon holed = make_polygon_with_hole(0, 0, 10, 3);
  Geometry geo(0, holed);
  EXPECT_FALSE(geo.is_convex());
  EXPECT_TRUE(geo.has_holes());
  EXPECT_GT(geo.decomposition().size(), 0u);
  for (const auto &piece : geo.decomposition()) {
    EXPECT_TRUE(utils::is_convex(piece));
  }
}

TEST(Polygon, StarPolygonProperties) {
  Polygon star = make_star_polygon(0, 0, 5, 2, 5);
  Geometry geo(0, star);
  EXPECT_FALSE(geo.is_convex());
  EXPECT_FALSE(geo.has_holes());
  EXPECT_GT(geo.decomposition().size(), 0u);
}

TEST(Polygon, ConvexPolygonNoDecomposition) {
  Polygon square = make_square(0, 0, 4);
  Geometry geo(0, square);
  EXPECT_TRUE(geo.is_convex());
  EXPECT_FALSE(geo.has_holes());
  // Convex polygon: decomposition should be empty (no splitting needed)
  EXPECT_EQ(geo.decomposition().size(), 0u);
}

TEST(Polygon, ConvexHullOfNonConvexPolygon) {
  Polygon lshape = make_L_polygon(0, 0, 4);
  Geometry geo(0, lshape);
  SiteVariant hull = geo.convex_hull();
  EXPECT_TRUE(utils::is_convex(hull));
  // Hull should be larger area than original (fills the L's concavity)
  Box hull_bbox = utils::bbox(hull);
  Box orig_bbox = utils::bbox(geo.definition());
  // Bounding boxes should match (hull covers the same extent)
  EXPECT_NEAR(hull_bbox.min_corner().get<0>(), orig_bbox.min_corner().get<0>(),
              1e-9);
  EXPECT_NEAR(hull_bbox.max_corner().get<0>(), orig_bbox.max_corner().get<0>(),
              1e-9);
}

TEST(Polygon, ConvexHullOfPolygonWithHoles) {
  Polygon holed = make_polygon_with_hole(0, 0, 10, 3);
  Geometry geo(0, holed);
  SiteVariant hull = geo.convex_hull();
  EXPECT_TRUE(utils::is_convex(hull));
  // Hull should be the outer boundary (a square), ignoring the hole
}

// ===================================================================
// Polygon distances
// ===================================================================

TEST(PolygonDistance, PolygonToPolygonSeparate) {
  Polygon p1 = make_square(0, 0, 2);
  Polygon p2 = make_square(5, 0, 2);
  EXPECT_NEAR(utils::distance(p1, p2), 3.0, 1e-9);
}

TEST(PolygonDistance, PolygonToPolygonTouching) {
  Polygon p1 = make_square(0, 0, 2);
  Polygon p2 = make_square(2, 0, 2);
  EXPECT_NEAR(utils::distance(p1, p2), 0.0, 1e-9);
}

TEST(PolygonDistance, PolygonToPolygonOverlapping) {
  Polygon p1 = make_square(0, 0, 3);
  Polygon p2 = make_square(1, 1, 3);
  EXPECT_NEAR(utils::distance(p1, p2), 0.0, 1e-9);
}

TEST(PolygonDistance, PolygonToRing) {
  Polygon poly = make_square(0, 0, 2);
  Ring ring{{5, 0}, {7, 0}, {7, 2}, {5, 2}, {5, 0}};
  EXPECT_NEAR(utils::distance(poly, ring), 3.0, 1e-9);
}

TEST(PolygonDistance, PolygonToPoint) {
  Polygon poly = make_square(0, 0, 2);
  Point p(5, 1);
  EXPECT_NEAR(utils::distance(poly, p), 3.0, 1e-9);
}

TEST(PolygonDistance, PointInsidePolygon) {
  Polygon poly = make_square(0, 0, 4);
  Point p(2, 2);
  EXPECT_NEAR(utils::distance(poly, p), 0.0, 1e-9);
}

TEST(PolygonDistance, LinestringToPolygon) {
  Polygon poly = make_square(5, 0, 2);
  Linestring ls{{0, 0}, {0, 5}};
  EXPECT_NEAR(utils::distance(ls, poly), 5.0, 1e-9);
}

TEST(PolygonDistance, LinestringTouchingPolygon) {
  Polygon poly = make_square(0, 0, 2);
  Linestring ls{{1, 0}, {1, 5}};
  EXPECT_NEAR(utils::distance(ls, poly), 0.0, 1e-9);
}

TEST(PolygonDistance, NonConvexPolygonDistance) {
  Polygon lshape = make_L_polygon(0, 0, 4);
  // Point in the concavity of the L (not inside the polygon)
  Point p(3, 3);
  EXPECT_GT(utils::distance(lshape, p), 0.0);
}

TEST(PolygonDistance, SiteVariantPolygonDispatch) {
  SiteVariant s1 = make_square(0, 0, 2);
  SiteVariant s2 = make_square(5, 0, 2);
  EXPECT_NEAR(utils::distance(s1, s2), 3.0, 1e-9);
}

// ===================================================================
// Trajectory covering polygons
// ===================================================================

TEST(PolygonTrajectory, TrajectoryCoversTouchingPolygon) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  auto sites = std::vector<SiteVariant>{make_square(4, -1, 2)};
  Instance inst(sites);
  EXPECT_TRUE(traj.covers(inst[0], 0.001));
}

TEST(PolygonTrajectory, TrajectoryDoesNotCoverDistantPolygon) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  auto sites = std::vector<SiteVariant>{make_square(4, 5, 2)};
  Instance inst(sites);
  EXPECT_FALSE(traj.covers(inst[0], 0.001));
}

TEST(PolygonTrajectory, TrajectoryCoversNonConvexPolygon) {
  // Trajectory passes through the non-convex polygon
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  // Trajectory passing through the L
  Trajectory traj{Linestring{{-1, 1}, {3, 1}}};
  EXPECT_TRUE(traj.covers(inst[0], 0.001));
}

TEST(PolygonTrajectory, TrajectoryNearNonConvexConcavity) {
  // Trajectory passes through the concavity of the L but not the polygon
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  // Point in the concave gap of the L: (3, 3) is outside the L
  Trajectory traj{Linestring{{3, 3}, {5, 5}}};
  EXPECT_FALSE(traj.covers(inst[0], 0.001));
}

TEST(PolygonTrajectory, TrajectoryCoversMixedPolygonTypes) {
  auto sites = std::vector<SiteVariant>{
      make_square(0, -1, 2),   // convex
      make_L_polygon(5, 0, 3), // non-convex
      make_square(10, -1, 2),  // convex
  };
  Instance inst(sites);
  Trajectory traj{Linestring{{1, 0}, {6, 1}, {11, 0}}};
  EXPECT_TRUE(traj.covers(inst.begin(), inst.end(), 0.001));
}

// ===================================================================
// TourElement with polygons
// ===================================================================

TEST(PolygonTourElement, ConvexPolygonNotLazy) {
  auto sites = std::vector<SiteVariant>{make_square(0, 0, 2)};
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_FALSE(te.is_lazy());
  EXPECT_TRUE(te.is_active());
}

TEST(PolygonTourElement, NonConvexPolygonIsLazy) {
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_lazy());
}

TEST(PolygonTourElement, BranchNonConvexPolygon) {
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  TourElement te(inst, 0);
  ASSERT_TRUE(te.is_lazy());
  auto branches = te.branch();
  EXPECT_GE(branches.size(), 2u);
  for (const auto &b : branches) {
    EXPECT_FALSE(b.is_lazy());
    EXPECT_FALSE(b.is_exact());
    EXPECT_EQ(b.geo_index(), 0u);
    // Each branch's active convex region should be convex
    EXPECT_TRUE(utils::is_convex(b.active_convex_region()));
  }
}

TEST(PolygonTourElement, SetExactNonConvexPolygon) {
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  TourElement te(inst, 0);
  ASSERT_TRUE(te.is_lazy());
  auto exact = te.set_exact();
  EXPECT_TRUE(exact.is_exact());
  EXPECT_FALSE(exact.is_lazy());
  // modeled_site for exact should be the original definition
  const auto &modeled = exact.modeled_site();
  EXPECT_TRUE(std::holds_alternative<Polygon>(modeled) ||
              std::holds_alternative<Ring>(modeled));
}

TEST(PolygonTourElement, ActiveConvexRegionConvexPolygon) {
  auto sites = std::vector<SiteVariant>{make_square(0, 0, 2)};
  Instance inst(sites);
  TourElement te(inst, 0);
  // For convex polygon, active region is the convex hull (= itself)
  const auto &region = te.active_convex_region();
  EXPECT_TRUE(utils::is_convex(region));
}

TEST(PolygonTourElement, ActiveConvexRegionBranchedElement) {
  auto sites = std::vector<SiteVariant>{make_L_polygon(0, 0, 4)};
  Instance inst(sites);
  TourElement te(inst, 0);
  auto branches = te.branch();
  ASSERT_FALSE(branches.empty());
  // Each branched element returns its decomposition piece, not the hull
  for (const auto &b : branches) {
    const auto &region = b.active_convex_region();
    EXPECT_TRUE(utils::is_convex(region));
  }
}

TEST(PolygonTourElement, PolygonWithHoleIsLazy) {
  auto sites = std::vector<SiteVariant>{make_polygon_with_hole(0, 0, 10, 3)};
  Instance inst(sites);
  TourElement te(inst, 0);
  EXPECT_TRUE(te.is_lazy());
}

TEST(PolygonTourElement, BranchPolygonWithHole) {
  auto sites = std::vector<SiteVariant>{make_polygon_with_hole(0, 0, 10, 3)};
  Instance inst(sites);
  TourElement te(inst, 0);
  ASSERT_TRUE(te.is_lazy());
  auto branches = te.branch();
  EXPECT_GE(branches.size(), 2u);
  for (const auto &b : branches) {
    EXPECT_FALSE(b.is_lazy());
    EXPECT_TRUE(utils::is_convex(b.active_convex_region()));
  }
}

// ===================================================================
// Convex partition quality for polygons
// ===================================================================

TEST(PolygonPartition, LShapePartitionCoversOriginal) {
  Polygon lshape = make_L_polygon(0, 0, 4);
  SiteVariant site = SiteVariant(lshape);
  auto pieces = details::partition(site);
  ASSERT_GE(pieces.size(), 2u);
  // Verify: a point inside the L is inside at least one piece
  Point test_point(1, 1); // clearly inside the L
  bool found_in_piece = false;
  for (const auto &piece : pieces) {
    if (utils::distance(piece, SiteVariant(test_point)) == 0.0) {
      found_in_piece = true;
      break;
    }
  }
  EXPECT_TRUE(found_in_piece);
}

TEST(PolygonPartition, StarPartitionAllConvex) {
  Polygon star = make_star_polygon(0, 0, 5, 2, 6);
  SiteVariant site = SiteVariant(star);
  auto pieces = details::partition(site);
  EXPECT_GE(pieces.size(), 2u);
  for (const auto &piece : pieces) {
    EXPECT_TRUE(utils::is_convex(piece));
  }
}

TEST(PolygonPartition, PolygonWithHolePartition) {
  Polygon holed = make_polygon_with_hole(0, 0, 10, 3);
  SiteVariant site = SiteVariant(holed);
  auto pieces = details::partition(site);
  EXPECT_GE(pieces.size(), 2u);
  for (const auto &piece : pieces) {
    EXPECT_TRUE(utils::is_convex(piece));
  }
  // A point outside the hole (in the ring) should be in some piece
  Point ring_point(1, 5);
  bool found = false;
  for (const auto &piece : pieces) {
    if (utils::distance(piece, SiteVariant(ring_point)) == 0.0) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// ===================================================================
// Node / relaxed solution with non-convex polygons
// ===================================================================

TEST(PolygonNode, NodeWithNonConvexPolygons) {
  auto sites = std::vector<SiteVariant>{
      make_L_polygon(0, 0, 3),
      make_square(6, 0, 2),
  };
  Instance inst(sites);
  SocSolver soc(false);
  std::vector<TourElement> seq = {TourElement(inst, 0), TourElement(inst, 1)};
  Node node(seq, &inst, &soc);
  EXPECT_GT(node.get_lower_bound(), 0.0);
  EXPECT_TRUE(node.get_relaxed_solution().get_trajectory().is_valid());
}

TEST(PolygonNode, NodeWithPolygonWithHoles) {
  auto sites = std::vector<SiteVariant>{
      make_polygon_with_hole(0, 0, 6, 2),
      make_square(10, 0, 2),
  };
  Instance inst(sites);
  SocSolver soc(false);
  std::vector<TourElement> seq = {TourElement(inst, 0), TourElement(inst, 1)};
  Node node(seq, &inst, &soc);
  EXPECT_GT(node.get_lower_bound(), 0.0);
  EXPECT_TRUE(node.get_relaxed_solution().get_trajectory().is_valid());
}

TEST(PolygonNode, NodeWithMixedConvexity) {
  auto sites = std::vector<SiteVariant>{
      make_square(0, 0, 2),    // convex
      make_L_polygon(5, 0, 3), // non-convex
      make_square(10, 0, 2),   // convex
  };
  Instance inst(sites);
  SocSolver soc(false);
  std::vector<TourElement> seq = {TourElement(inst, 0), TourElement(inst, 1),
                                  TourElement(inst, 2)};
  Node node(seq, &inst, &soc);
  EXPECT_GT(node.get_lower_bound(), 0.0);
}

// ===================================================================
// BnB solving with non-convex polygons
// ===================================================================

TEST(PolygonBnB, SolveWithNonConvexPolygons) {
  auto sites = std::vector<SiteVariant>{
      make_L_polygon(0, 0, 3),
      make_L_polygon(8, 0, 3),
      make_square(4, 8, 2),
  };
  Instance instance(sites);
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(100);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.01));
}

TEST(PolygonBnB, SolveWithPolygonsWithHoles) {
  auto sites = std::vector<SiteVariant>{
      make_polygon_with_hole(0, 0, 6, 2),
      make_square(10, 0, 2),
      make_square(5, 8, 2),
  };
  Instance instance(sites);
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(100);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.01));
}

TEST(PolygonBnB, SolveWithMixedPolygonTypes) {
  auto sites = std::vector<SiteVariant>{
      make_square(0, 0, 2),
      make_L_polygon(5, 0, 3),
      make_polygon_with_hole(10, 0, 5, 1.5),
      make_star_polygon(3, 8, 2, 1, 5),
  };
  Instance instance(sites);
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(200);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.01));
}

TEST(PolygonBnB, SolveManyNonConvexPolygons) {
  // Grid of L-shaped polygons
  Instance instance;
  for (double x = 0; x <= 6; x += 3.0) {
    for (double y = 0; y <= 6; y += 3.0) {
      Polygon lshape = make_L_polygon(x, y, 2);
      instance.add_site(lshape);
    }
  }
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(50);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.01));
}

TEST(PolygonBnB, PathModeNonConvex) {
  auto sites = std::vector<SiteVariant>{
      make_L_polygon(0, 0, 3),
      make_square(5, 0, 2),
      make_L_polygon(10, 0, 3),
  };
  Instance instance(sites, /*path=*/true);
  EXPECT_TRUE(instance.is_path());
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(100);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().is_path());
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.01));
}

// ===================================================================
// Pre-simplification with polygons
// ===================================================================

TEST(PolygonPreSimplify, LargePolygonContainingSmall) {
  // Large polygon entirely contains a small polygon -> large is redundant
  std::vector<SiteVariant> sites = {
      make_square(0, 0, 10), // large
      make_square(3, 3, 2),  // small, inside large
  };
  for (auto &s : sites)
    utils::correct(s);
  details::remove_supersites(sites);
  EXPECT_EQ(sites.size(), 1u);
}

TEST(PolygonPreSimplify, NonOverlappingPolygonsKept) {
  std::vector<SiteVariant> sites = {
      make_square(0, 0, 2),
      make_square(5, 5, 2),
      make_L_polygon(10, 0, 3),
  };
  for (auto &s : sites)
    utils::correct(s);
  details::remove_supersites(sites);
  EXPECT_EQ(sites.size(), 3u);
}

TEST(PolygonPreSimplify, PolygonContainingPoint) {
  std::vector<SiteVariant> sites = {
      make_square(0, 0, 4),
      SiteVariant(Point(2, 2)), // inside the square
  };
  for (auto &s : sites)
    utils::correct(s);
  details::remove_supersites(sites);
  // The square contains the point, so the square should be removed
  EXPECT_EQ(sites.size(), 1u);
}

TEST(PolygonPreSimplify, SimplifyMixedPolygonsAndCircles) {
  Circle big_circle(Point(5, 5), 10.0); // huge circle
  std::vector<SiteVariant> sites = {
      SiteVariant(big_circle),
      make_square(3, 3, 2), // inside the circle
  };
  for (auto &s : sites)
    utils::correct(s);
  details::remove_supersites(sites);
  // Circle contains the polygon -> circle removed
  EXPECT_EQ(sites.size(), 1u);
}

// ===================================================================
// Instance hull_instance with polygons
// ===================================================================

TEST(PolygonInstance, HullInstanceNonConvex) {
  auto sites = std::vector<SiteVariant>{
      make_L_polygon(0, 0, 4),
      make_L_polygon(10, 0, 4),
  };
  Instance inst(sites);
  Instance hull_inst = inst.hull_instance();
  EXPECT_EQ(hull_inst.size(), 2u);
  // Both hulls should be convex
  EXPECT_TRUE(hull_inst[0].is_convex());
  EXPECT_TRUE(hull_inst[1].is_convex());
}

TEST(PolygonInstance, HullInstanceWithHoles) {
  auto sites = std::vector<SiteVariant>{
      make_polygon_with_hole(0, 0, 8, 2),
      make_square(12, 0, 3),
  };
  Instance inst(sites);
  Instance hull_inst = inst.hull_instance();
  EXPECT_TRUE(hull_inst[0].is_convex());
  EXPECT_TRUE(hull_inst[1].is_convex());
  // Hull of polygon-with-hole should not have holes
  EXPECT_FALSE(hull_inst[0].has_holes());
}

} // namespace tspn
