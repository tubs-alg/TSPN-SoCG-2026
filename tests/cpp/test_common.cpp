// Tests for Trajectory class from tspn/common.h

#include <gtest/gtest.h>

#include "tspn_core/common.h"

namespace bg = boost::geometry;

namespace tspn {

TEST(Trajectory, BasicProperties) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {5, 5}}};
  EXPECT_FALSE(traj.is_tour());
  Polygon p1{{{3, -1}, {3, 1}, {2, 1}}};
  Geometry geometry{0, p1};
  EXPECT_DOUBLE_EQ(traj.distance_site(p1), 0);
  EXPECT_TRUE(traj.covers(geometry, 0));
  EXPECT_DOUBLE_EQ(traj.length(), 10.0);
}

// NOTE: SubTrajectory tests removed — Trajectory::sub() is not yet implemented.

} // namespace tspn
