// Tests for PartialSequenceSolution from tspn/relaxed_solution.h

#include <gtest/gtest.h>

#include "tspn/relaxed_solution.h"

namespace tspn {

TEST(PartialSequenceSolution, BasicSolution) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{3, 0}, {2, 0}, {3, 1}}},
      Polygon{{{3, 3}, {2, 3}, {3, 2}}},
      Polygon{{{0, 3}, {1, 3}, {0, 2}}},
  };
  Instance instance(polygons);
  SocSolver soc(false);
  auto sequence = {TourElement(instance, 0), TourElement(instance, 1),
                   TourElement(instance, 2), TourElement(instance, 3)};
  PartialSequenceSolution pss(&instance, &soc, sequence,
                              std::list<StashedTourElement>(), 0.001);
  EXPECT_NEAR(pss.obj(), 8.0, 1e-6);
  pss.simplify();
  EXPECT_NEAR(pss.obj(), 8.0, 1e-6);
  EXPECT_TRUE(pss.is_feasible());
}

} // namespace tspn
