// Tests for the Branch and Bound algorithm from tspn/bnb.h

#include <gtest/gtest.h>

#include "tspn_core/bnb.h"
#include "tspn_core/utils/drawer.h"

namespace tspn {

TEST(BranchAndBound, BasicFourPolygons) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {0, -1}, {-1, 0}}},
      Polygon{{{3, 0}, {3, -1}, {2, -1}}},
      Polygon{{{6, 0}, {6, -1}, {5, -1}}},
      Polygon{{{3, 6}, {3, 5}, {2, 6}}},
  };
  Instance instance(polygons);
  EXPECT_EQ(instance.size(), 4);
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(5);
}

TEST(BranchAndBound, OptimalSolution) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{2, 0}, {4, 0}, {3, 1}}},
      Polygon{{{5, 0}, {6, 0}, {6, 1}}},
      Polygon{{{2, 6}, {4, 6}, {3, 7}}},
  };
  Instance instance(polygons);
  EXPECT_EQ(instance.size(), 4);

  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(300);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_NEAR(bnb.get_solution()->get_trajectory().length(), 16.6491, 0.2);
  EXPECT_NEAR(bnb.get_upper_bound(), 16.6491, 0.2);
}

TEST(BranchAndBound, GridInstance) {
  Instance instance;
  for (double x = 0; x <= 8; x += 2.0) {
    for (double y = 0; y <= 8; y += 2.0) {
      Polygon poly{{{x - 0.5, y - 0.5},
                    {x - 0.5, y + 0.5},
                    {x + 0.5, y + 0.5},
                    {x + 0.5, y - 0.5}}};
      instance.add_site(poly);
    }
  }
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy{false, false};
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(10);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_LE(bnb.get_upper_bound(), 41);
}

TEST(BranchAndBound, RandomBranching) {
  Instance instance;
  for (double x = 0; x <= 8; x += 2.0) {
    for (double y = 0; y <= 8; y += 2.0) {
      Polygon poly{{{x - 0.5, y - 0.5},
                    {x - 0.5, y + 0.5},
                    {x + 0.5, y + 0.5},
                    {x + 0.5, y - 0.5}}};
      instance.add_site(poly);
    }
  }
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy{true, false, true};
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(20);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_LE(bnb.get_upper_bound(), 41);
}

TEST(BranchAndBound, PathMode) {
  Instance instance;
  for (double x = 0; x <= 8; x += 2.0) {
    for (double y = 0; y <= 8; y += 2.0) {
      Polygon poly{{{x - 0.5, y - 0.5},
                    {x - 0.5, y + 0.5},
                    {x + 0.5, y + 0.5},
                    {x + 0.5, y - 0.5}}};
      instance.add_site(poly);
    }
  }
  instance.path = true;
  LongestEdgePlusFurthestSite root_node_strategy{};
  FarthestPoly branching_strategy;
  CheapestChildDepthFirst search_strategy;
  SocSolver soc(false);
  BranchAndBoundAlgorithm bnb(&instance,
                              root_node_strategy.get_root_node(instance, soc),
                              branching_strategy, search_strategy);
  bnb.optimize(20);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_TRUE(bnb.get_solution()->get_trajectory().covers(
      instance.begin(), instance.end(), 0.001));
  EXPECT_NEAR(bnb.get_upper_bound(), 32.2531, 0.4);
}

} // namespace tspn
