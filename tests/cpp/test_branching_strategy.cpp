// Tests for branching strategies from tspn/strategies/branching_strategy.h

#include <gtest/gtest.h>

#include "tspn_core/strategies/branching_strategy.h"

namespace tspn {

TEST(BranchingStrategy, FarthestPoly) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{2, 0}, {4, 0}, {3, 1}}},
      Polygon{{{5, 0}, {6, 0}, {6, 1}}},
      Polygon{{{2, 6}, {4, 6}, {3, 7}}},
  };
  Instance instance(polygons);
  FarthestPoly bs(false);
  SocSolver soc(false);

  auto root =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2),
                                                      TourElement(instance, 3)},
                             &instance, &soc);
  bs.setup(&instance, root, nullptr);
  EXPECT_FALSE(bs.branch(*root));

  Node root2({TourElement(instance, 0), TourElement(instance, 1),
              TourElement(instance, 2)},
             &instance, &soc);
  EXPECT_TRUE(bs.branch(root2));
  EXPECT_EQ(root2.get_children().size(), 3);
}

} // namespace tspn
