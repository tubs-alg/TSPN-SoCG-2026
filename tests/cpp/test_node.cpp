// Tests for Node class from tspn/node.h

#include <gtest/gtest.h>

#include "tspn_core/node.h"

namespace tspn {

TEST(Node, BasicNode) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{1, 0}, {0, 0}, {0, 1}}},
      Polygon{{{1, 2}, {0, 2}, {0, 3}}},
  };
  Instance seq(polygons);
  EXPECT_TRUE(seq.is_tour());
  SocSolver soc(false);
  Node node({TourElement(seq, 0), TourElement(seq, 1)}, &seq, &soc);
  const auto &tour = node.get_relaxed_solution();
  EXPECT_NEAR(tour.obj(), 2.0, 1e-6);
  EXPECT_NEAR(node.get_lower_bound(), 2.0, 1e-6);
  EXPECT_TRUE(node.is_feasible());
}

} // namespace tspn
