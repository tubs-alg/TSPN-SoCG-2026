// Tests for root node strategies from tspn/strategies/root_node_strategy.h

#include <gtest/gtest.h>

#include "tspn_core/strategies/root_node_strategy.h"

namespace tspn {

TEST(RootNodeStrategy, LongestEdgePlusFurthestSite) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {0, -1}, {-1, 0}}},
      Polygon{{{3, 0}, {3, -1}, {2, -1}}},
      Polygon{{{6, 0}, {6, -1}, {5, -1}}},
      Polygon{{{3, 6}, {3, 5}, {2, 6}}},
  };
  Instance instance(polygons);
  LongestEdgePlusFurthestSite rns;
  SocSolver soc(false);
  auto root = rns.get_root_node(instance, soc);
  EXPECT_TRUE(root->is_feasible());
}

} // namespace tspn
