// Tests for the SOCP solver from tspn/soc.h

#include <gtest/gtest.h>

#include "tspn/soc.h"
#include "tspn/utils/drawer.h"

namespace tspn {

TEST(SocSolver, SimpleSocpTest) {
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{1, 0}, {0, 0}, {0, 1}}},
      Polygon{{{1, 2}, {0, 2}, {0, 3}}},
  };
  Instance instance(polygons);
  TourElement te0 = TourElement(instance, 0);
  TourElement te1 = TourElement(instance, 1);
  SocSolver solver(false);
  std::vector<TourElement> seq{te0, te1};
  auto traj = solver.compute_trajectory(seq, false);
  EXPECT_NEAR(traj.length(), 2.0, 1e-6);
  traj = solver.compute_trajectory(seq, true);
  EXPECT_NEAR(traj.length(), 1.0, 1e-6);
}

} // namespace tspn
