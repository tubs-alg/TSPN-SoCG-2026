// Tests for search strategies from tspn/strategies/search_strategy.h

#include <gtest/gtest.h>

#include "tspn_core/strategies/search_strategy.h"

namespace tspn {

TEST(SearchStrategy, CheapestChildDepthFirst) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{2, 0}, {4, 0}, {3, 1}}},
      Polygon{{{5, 0}, {6, 0}, {6, 1}}},
      Polygon{{{2, 6}, {4, 6}, {3, 7}}},
  };
  Instance instance(polygons);
  FarthestPoly bs;
  SocSolver soc(false);
  auto root =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2),
                                                      TourElement(instance, 3)},
                             &instance, &soc);
  bs.setup(&instance, root, nullptr);
  CheapestChildDepthFirst ss;
  ss.init(root);
  auto node = ss.next();
  EXPECT_NE(node, nullptr);
  EXPECT_FALSE(bs.branch(*node));
  ss.notify_of_branch(*node);
  EXPECT_EQ(ss.next(), nullptr);
  auto root2 =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2)},
                             &instance, &soc);
  CheapestChildDepthFirst ss2;
  ss2.init(root2);
  node = ss2.next();
  EXPECT_TRUE(bs.branch(*node));
  ss2.notify_of_branch(*node);
  EXPECT_NE(ss2.next(), nullptr);
  EXPECT_NE(ss2.next(), nullptr);
  EXPECT_NE(ss2.next(), nullptr);
  EXPECT_EQ(ss2.next(), nullptr);
}

} // namespace tspn
