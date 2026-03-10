// Tests for root node strategies from tspn/strategies/root_node_strategy.h
//
// Verifies the actual selection logic of each strategy, not just that
// a root is produced. Each test checks the behavioral contract:
// which sites are selected and why.

#include <gtest/gtest.h>

#include "tspn_core/bnb.h"
#include "tspn_core/strategies/root_node_strategy.h"
#include "tspn_core/utils/distance.h"

namespace tspn {

// --- Helpers ---

// Compute the cumulative pairwise distance for a triple of site indices.
double triple_distance(const Instance &instance, unsigned a, unsigned b,
                       unsigned c) {
  return utils::distance(instance[a].definition(), instance[b].definition()) +
         utils::distance(instance[a].definition(), instance[c].definition()) +
         utils::distance(instance[b].definition(), instance[c].definition());
}

// Compute pairwise distance between two sites.
double pair_distance(const Instance &instance, unsigned a, unsigned b) {
  return utils::distance(instance[a].definition(), instance[b].definition());
}

// Extract geo_indices from a root node's fixed sequence.
std::set<unsigned> root_indices(const std::shared_ptr<Node> &root) {
  std::set<unsigned> indices;
  for (auto &te : root->get_fixed_sequence()) {
    indices.insert(te.geo_index());
  }
  return indices;
}

// Brute-force the optimal triple (max cumulative pairwise distance).
std::set<unsigned> optimal_triple(const Instance &instance) {
  double best = -1;
  std::set<unsigned> best_set;
  for (unsigned a = 0; a < instance.size(); ++a) {
    for (unsigned b = a + 1; b < instance.size(); ++b) {
      for (unsigned c = b + 1; c < instance.size(); ++c) {
        double d = triple_distance(instance, a, b, c);
        if (d > best) {
          best = d;
          best_set = {a, b, c};
        }
      }
    }
  }
  return best_set;
}

// Brute-force the optimal pair (max distance).
std::pair<unsigned, unsigned> optimal_pair(const Instance &instance) {
  double best = -1;
  std::pair<unsigned, unsigned> best_pair;
  for (unsigned a = 0; a < instance.size(); ++a) {
    for (unsigned b = a + 1; b < instance.size(); ++b) {
      double d = pair_distance(instance, a, b);
      if (d > best) {
        best = d;
        best_pair = {a, b};
      }
    }
  }
  return best_pair;
}

// =========================================================================
// LongestTriple: must select the 3 sites that maximize cumulative distance
// =========================================================================

TEST(RootNodeStrategy, LongestTripleSelectsOptimalTriple) {
  // Carefully placed sites where the optimal triple is unambiguous:
  // Corners of a large triangle are clearly better than the cluster.
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // idx 0 - corner
      Point(100, 0), // idx 1 - corner
      Point(50, 87), // idx 2 - corner (~equilateral)
      Point(50, 30), // idx 3 - interior, clearly worse
      Point(48, 32), // idx 4 - interior cluster
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestTriple rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);
  auto expected = optimal_triple(instance);

  EXPECT_EQ(selected, expected)
      << "LongestTriple should select the triple with maximum cumulative "
         "pairwise distance";
}

TEST(RootNodeStrategy, LongestTripleBeatsSuboptimalTriple) {
  // Verify the selected triple's cumulative distance is actually maximal.
  std::vector<SiteVariant> sites = {
      Point(0, 0), Point(10, 0), Point(20, 0), Point(10, 15), Point(5, 3),
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestTriple rns;
  auto root = rns.get_root_node(instance, soc);

  auto sel = root->get_fixed_sequence();
  double selected_dist = triple_distance(
      instance, sel[0].geo_index(), sel[1].geo_index(), sel[2].geo_index());

  // Check against every possible triple.
  for (unsigned a = 0; a < instance.size(); ++a) {
    for (unsigned b = a + 1; b < instance.size(); ++b) {
      for (unsigned c = b + 1; c < instance.size(); ++c) {
        double d = triple_distance(instance, a, b, c);
        EXPECT_GE(selected_dist, d - 1e-9)
            << "Triple (" << a << "," << b << "," << c
            << ") has larger cumulative distance " << d << " vs selected "
            << selected_dist;
      }
    }
  }
}

// =========================================================================
// LongestPair: must select the 2 sites with maximum distance
// =========================================================================

TEST(RootNodeStrategy, LongestPairSelectsOptimalPair) {
  std::vector<SiteVariant> sites = {
      Point(0, 0),    // idx 0
      Point(100, 0),  // idx 1 - far right
      Point(50, 10),  // idx 2 - middle
      Point(50, -10), // idx 3 - middle
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestPair rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);
  auto [exp_a, exp_b] = optimal_pair(instance);
  std::set<unsigned> expected = {exp_a, exp_b};

  EXPECT_EQ(selected, expected)
      << "LongestPair should select the pair with maximum distance";
}

TEST(RootNodeStrategy, LongestPairDistanceIsMaximal) {
  std::vector<SiteVariant> sites = {
      Circle(Point(0, 0), 2.0),
      Circle(Point(20, 0), 1.0),
      Circle(Point(10, 15), 3.0),
      Circle(Point(5, 5), 0.5),
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestPair rns;
  auto root = rns.get_root_node(instance, soc);

  auto sel = root->get_fixed_sequence();
  double selected_dist =
      pair_distance(instance, sel[0].geo_index(), sel[1].geo_index());

  for (unsigned a = 0; a < instance.size(); ++a) {
    for (unsigned b = a + 1; b < instance.size(); ++b) {
      EXPECT_GE(selected_dist, pair_distance(instance, a, b) - 1e-9);
    }
  }
}

// =========================================================================
// LongestEdgePlusFurthestSite: max pair first, then most distant third site
// =========================================================================

TEST(RootNodeStrategy, LongestEdgePlusFurthestSiteSelectsMaxPairAndFurthest) {
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // idx 0
      Point(50, 0),  // idx 1
      Point(100, 0), // idx 2 - forms max pair with 0
      Point(50, 80), // idx 3 - far from the max-pair edge
      Point(50, 5),  // idx 4 - close to center, bad third
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestEdgePlusFurthestSite rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);

  // Max pair should be {0, 2} (distance 100).
  EXPECT_TRUE(selected.count(0) && selected.count(2))
      << "Should include the maximum-distance pair (0, 2)";

  // Third site should be 3 (farthest from both 0 and 2).
  // dist(0,3) + dist(2,3) vs dist(0,1) + dist(2,1) vs dist(0,4) + dist(2,4)
  EXPECT_TRUE(selected.count(3))
      << "Third site should be idx 3, which maximizes cumulative distance to "
         "the max pair";
}

// =========================================================================
// LongestTripleWithPoint: same as LongestTriple but root must contain a Point
// =========================================================================

TEST(RootNodeStrategy, LongestTripleWithPointIncludesPoint) {
  std::vector<SiteVariant> sites = {
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},       // idx 0
      Point(50, 0),                            // idx 1 - point
      Polygon{{{100, 0}, {101, 0}, {100, 1}}}, // idx 2
      Polygon{{{50, 80}, {51, 80}, {50, 81}}}, // idx 3
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestTripleWithPoint rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);
  // Verify at least one selected site is a Point
  bool has_point = false;
  for (unsigned idx : selected) {
    if (std::holds_alternative<Point>(instance[idx].definition())) {
      has_point = true;
    }
  }
  EXPECT_TRUE(has_point) << "Root must contain at least one Point site";
}

TEST(RootNodeStrategy, LongestTripleWithPointIsOptimalAmongPointTriples) {
  // The selected triple must maximize cumulative distance among all triples
  // that include at least one point.
  std::vector<SiteVariant> sites = {
      Point(0, 0),                             // idx 0
      Polygon{{{100, 0}, {101, 0}, {100, 1}}}, // idx 1
      Point(50, 87),                           // idx 2
      Polygon{{{49, 30}, {51, 30}, {50, 31}}}, // idx 3
      Point(80, 50),                           // idx 4
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestTripleWithPoint rns;
  auto root = rns.get_root_node(instance, soc);

  auto sel = root->get_fixed_sequence();
  double selected_dist = triple_distance(
      instance, sel[0].geo_index(), sel[1].geo_index(), sel[2].geo_index());

  // Check against all triples containing at least one point.
  for (unsigned a = 0; a < instance.size(); ++a) {
    for (unsigned b = a + 1; b < instance.size(); ++b) {
      for (unsigned c = b + 1; c < instance.size(); ++c) {
        bool has_pt = std::holds_alternative<Point>(instance[a].definition()) ||
                      std::holds_alternative<Point>(instance[b].definition()) ||
                      std::holds_alternative<Point>(instance[c].definition());
        if (has_pt) {
          double d = triple_distance(instance, a, b, c);
          EXPECT_GE(selected_dist, d - 1e-9)
              << "Point-triple (" << a << "," << b << "," << c
              << ") has larger distance " << d;
        }
      }
    }
  }
}

TEST(RootNodeStrategy, LongestTripleWithPointThrowsWithoutPoints) {
  std::vector<SiteVariant> sites = {
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{5, 0}, {6, 0}, {6, 1}}},
      Polygon{{{0, 5}, {1, 5}, {1, 6}}},
      Polygon{{{5, 5}, {6, 5}, {6, 6}}},
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestTripleWithPoint rns;
  EXPECT_THROW(rns.get_root_node(instance, soc), std::logic_error);
}

// =========================================================================
// LongestPointTriple: maximize number of points in root, then distance
// =========================================================================

TEST(RootNodeStrategy, LongestPointTripleMaximizesPointCount) {
  // 3 points + 2 polygons. With 3+ points available, all 3 should be points.
  std::vector<SiteVariant> sites = {
      Point(0, 0),                             // idx 0
      Point(100, 0),                           // idx 1
      Point(50, 87),                           // idx 2
      Polygon{{{48, 30}, {52, 30}, {50, 34}}}, // idx 3 - interior polygon
      Polygon{{{48, 32}, {52, 32}, {50, 36}}}, // idx 4 - interior polygon
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestPointTriple rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);
  unsigned point_count = 0;
  for (unsigned idx : selected) {
    if (std::holds_alternative<Point>(instance[idx].definition())) {
      ++point_count;
    }
  }
  EXPECT_EQ(point_count, 3u)
      << "With 3+ points available, all root sites should be points";
}

TEST(RootNodeStrategy, LongestPointTripleUsesAllPointsWhenOnlyTwo) {
  // 2 points + 3 polygons. Both points must be in the root.
  std::vector<SiteVariant> sites = {
      Point(0, 0),                                // idx 0
      Point(100, 0),                              // idx 1
      Polygon{{{50, 80}, {51, 80}, {50, 81}}},    // idx 2
      Polygon{{{50, 0}, {51, 0}, {51, 1}}},       // idx 3
      Polygon{{{50, -80}, {51, -80}, {50, -81}}}, // idx 4
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestPointTriple rns;
  auto root = rns.get_root_node(instance, soc);

  auto selected = root_indices(root);
  EXPECT_TRUE(selected.count(0)) << "Point at idx 0 must be in root";
  EXPECT_TRUE(selected.count(1)) << "Point at idx 1 must be in root";
  // Third should be the polygon maximizing cumulative distance to both points
  // That's idx 2 or idx 4 (both at y=80, equally far), not idx 3 (close).
  unsigned third = 0;
  for (unsigned idx : selected) {
    if (idx != 0 && idx != 1)
      third = idx;
  }
  EXPECT_TRUE(third == 2 || third == 4)
      << "Third site should be the polygon farthest from both points, got idx "
      << third;
}

TEST(RootNodeStrategy, LongestPointTripleThrowsWithoutPoints) {
  std::vector<SiteVariant> sites = {
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{5, 0}, {6, 0}, {6, 1}}},
      Polygon{{{0, 5}, {1, 5}, {1, 6}}},
      Polygon{{{5, 5}, {6, 5}, {6, 6}}},
  };
  Instance instance(sites);
  SocSolver soc(false);
  LongestPointTriple rns;
  EXPECT_THROW(rns.get_root_node(instance, soc), std::logic_error);
}

// =========================================================================
// Path mode: all strategies must have origin first, target last
// =========================================================================

TEST(RootNodeStrategy, PathModeOriginTargetEndpoints) {
  // Path: origin=idx0, target=idx1, intermediates=idx2..4
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // origin
      Point(20, 0),  // target
      Point(10, 10), // intermediate
      Point(5, 5),   // intermediate
      Point(15, 5),  // intermediate
  };
  Instance instance(sites, true);
  SocSolver soc(false);

  auto test_path_endpoints = [&](RootNodeStrategy &rns,
                                 const std::string &name) {
    auto root = rns.get_root_node(instance, soc);
    auto &seq = root->get_fixed_sequence();
    EXPECT_EQ(seq.front().geo_index(), 0u)
        << name << ": origin must be first in path root";
    EXPECT_EQ(seq.back().geo_index(), 1u)
        << name << ": target must be last in path root";
  };

  LongestEdgePlusFurthestSite s1;
  test_path_endpoints(s1, "LongestEdgePlusFurthestSite");
  LongestTriple s2;
  test_path_endpoints(s2, "LongestTriple");
  LongestPair s3;
  test_path_endpoints(s3, "LongestPair");
  RandomPair s4;
  test_path_endpoints(s4, "RandomPair");
  RandomRoot s5;
  test_path_endpoints(s5, "RandomRoot");
}

TEST(RootNodeStrategy, PathModeIntermediateMaximizesDistance) {
  // In path mode, strategies using path_furtest_site_root select the
  // intermediate site that maximizes dist(origin, x) + dist(target, x).
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // origin
      Point(20, 0),  // target
      Point(10, 50), // idx 2 - far from both (best intermediate)
      Point(10, 1),  // idx 3 - close to the line
  };
  Instance instance(sites, true);
  SocSolver soc(false);

  LongestEdgePlusFurthestSite rns;
  auto root = rns.get_root_node(instance, soc);
  auto &seq = root->get_fixed_sequence();

  // Should pick idx 2 as intermediate (not idx 3)
  ASSERT_EQ(seq.size(), 3u);
  EXPECT_EQ(seq[1].geo_index(), 2u)
      << "Intermediate should be idx 2 (farthest from origin+target)";
}

// =========================================================================
// Trivial cases: <=3 sites for tour, <=2 for path
// =========================================================================

TEST(RootNodeStrategy, TrivialTourIncludesAllSites) {
  std::vector<SiteVariant> sites = {Point(0, 0), Point(5, 0), Point(2, 3)};
  Instance instance(sites);
  SocSolver soc(false);

  LongestEdgePlusFurthestSite s1;
  auto r1 = s1.get_root_node(instance, soc);
  EXPECT_EQ(root_indices(r1).size(), 3u) << "Trivial tour: all 3 sites in root";
  EXPECT_TRUE(r1->is_feasible()) << "All sites covered => feasible";

  LongestTriple s2;
  auto r2 = s2.get_root_node(instance, soc);
  EXPECT_EQ(root_indices(r2).size(), 3u);
}

TEST(RootNodeStrategy, TrivialPathTwoSites) {
  std::vector<SiteVariant> sites = {Point(0, 0), Point(10, 0)};
  Instance instance(sites, true);
  SocSolver soc(false);

  LongestEdgePlusFurthestSite rns;
  auto root = rns.get_root_node(instance, soc);
  EXPECT_EQ(root->get_fixed_sequence().size(), 2u);
  EXPECT_TRUE(root->is_feasible());
}

// =========================================================================
// RandomPair: always selects 2 distinct sites for tours
// =========================================================================

TEST(RootNodeStrategy, RandomPairSelectsDistinctSites) {
  std::vector<SiteVariant> sites = {
      Point(0, 0), Point(5, 0), Point(10, 0), Point(5, 5), Point(7, 3),
  };
  Instance instance(sites);
  SocSolver soc(false);

  // Run multiple times to increase confidence (randomized strategy)
  for (int trial = 0; trial < 5; ++trial) {
    RandomPair rns;
    auto root = rns.get_root_node(instance, soc);
    auto &seq = root->get_fixed_sequence();
    EXPECT_EQ(seq.size(), 2u);
    EXPECT_NE(seq[0].geo_index(), seq[1].geo_index())
        << "RandomPair must select distinct sites";
  }
}

// =========================================================================
// RandomRoot: correct size and constraints
// =========================================================================

TEST(RootNodeStrategy, RandomRootTourPicksThreeFromLargerInstance) {
  std::vector<SiteVariant> sites = {
      Point(0, 0), Point(5, 0), Point(10, 0),
      Point(5, 5), Point(7, 3), Point(3, 8),
  };
  Instance instance(sites);
  SocSolver soc(false);

  RandomRoot rns;
  auto root = rns.get_root_node(instance, soc);
  auto selected = root_indices(root);
  EXPECT_EQ(selected.size(), 3u)
      << "RandomRoot tour with >3 sites should pick exactly 3";
}

TEST(RootNodeStrategy, RandomRootTourTrivialReturnsAll) {
  std::vector<SiteVariant> sites = {Point(0, 0), Point(5, 0)};
  Instance instance(sites);
  SocSolver soc(false);

  RandomRoot rns;
  auto root = rns.get_root_node(instance, soc);
  EXPECT_EQ(root_indices(root).size(), 2u)
      << "RandomRoot with <=3 sites returns all";
}

// =========================================================================
// OrderRootStrategy: respects annotations and overlap exclusions
// =========================================================================

TEST(RootNodeStrategy, OrderRootNoAnnotationsFallsBackToMaxPair) {
  std::vector<SiteVariant> sites = {
      Point(0, 0),
      Point(100, 0),
      Point(50, 10),
      Point(50, -10),
  };
  Instance instance(sites);
  SocSolver soc(false);
  OrderRootStrategy rns;
  auto root = rns.get_root_node(instance, soc);

  // Without annotations, falls back to max pair heuristic
  auto selected = root_indices(root);
  auto [exp_a, exp_b] = optimal_pair(instance);
  EXPECT_TRUE(selected.count(exp_a) && selected.count(exp_b))
      << "Fallback should select the maximum-distance pair";
}

// =========================================================================
// Root LB is a valid lower bound (less than or equal to any feasible tour)
// =========================================================================

TEST(RootNodeStrategy, RootLBIsValidForAllStrategies) {
  // For a small instance, we can solve it and verify root LB <= optimal.
  std::vector<SiteVariant> sites = {
      Point(0, 0),
      Point(10, 0),
      Point(10, 10),
      Point(0, 10),
  };
  Instance instance(sites);
  SocSolver soc(false);

  // Solve to get optimal value
  LongestEdgePlusFurthestSite rns_solve;
  auto root_solve = rns_solve.get_root_node(instance, soc);
  FarthestPoly bs(false);
  CheapestChildDepthFirst ss;
  BranchAndBoundAlgorithm bnb(&instance, root_solve, bs, ss);
  bnb.optimize(30, 0.001, false);
  double optimal_ub = bnb.get_upper_bound();

  // Now verify that every strategy's root LB <= optimal
  auto check_lb = [&](RootNodeStrategy &rns, const std::string &name) {
    auto root = rns.get_root_node(instance, soc);
    EXPECT_LE(root->get_lower_bound(), optimal_ub + 1e-6)
        << name << ": root LB must be <= optimal tour length";
  };

  LongestEdgePlusFurthestSite s1;
  check_lb(s1, "LongestEdgePlusFurthestSite");
  LongestTriple s2;
  check_lb(s2, "LongestTriple");
  LongestPair s3;
  check_lb(s3, "LongestPair");
  RandomPair s4;
  check_lb(s4, "RandomPair");
  RandomRoot s5;
  check_lb(s5, "RandomRoot");
}

} // namespace tspn
