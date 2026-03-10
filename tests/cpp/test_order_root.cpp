// Tests for OrderRootStrategy from tspn/strategies/root_node_strategy.h

#include <gtest/gtest.h>

#include "tspn_core/common.h"
#include "tspn_core/node.h"
#include "tspn_core/soc.h"
#include "tspn_core/strategies/root_node_strategy.h"

TEST(OrderRootStrategy, PicksNonOverlappingAnnotatedSites) {
  std::vector<tspn::SiteVariant> sites;
  sites.push_back(tspn::Polygon{{{0, 0}, {1, 0}, {1, 1}, {0, 1}}});
  sites.push_back(tspn::Polygon{{{2, 0}, {3, 0}, {3, 1}, {2, 1}}});
  sites.push_back(tspn::Polygon{{{4, 0}, {5, 0}, {5, 1}, {4, 1}}});
  sites.push_back(tspn::Polygon{{{6, 0}, {7, 0}, {7, 1}, {6, 1}}});

  tspn::Instance instance(sites, /*path=*/false);
  instance[0].annotations.order_index = 0;
  instance[1].annotations.order_index = 1;
  instance[2].annotations.order_index = 2;
  instance[3].annotations.order_index = 3;
  instance[0].annotations.overlapping_order_geo_indices = {1};

  tspn::SocSolver soc(false);
  tspn::OrderRootStrategy strat;
  auto root = strat.get_root_node(instance, soc);

  const auto &seq = root->get_fixed_sequence();
  EXPECT_GE(seq.size(), 2);
  bool has0 = false;
  bool has1 = false;
  for (const auto &te : seq) {
    if (te.geo_index() == 0) {
      has0 = true;
    }
    if (te.geo_index() == 1) {
      has1 = true;
    }
  }
  EXPECT_TRUE(has0);
  EXPECT_FALSE(has1);
}
