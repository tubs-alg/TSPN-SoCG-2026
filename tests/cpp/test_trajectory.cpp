// Tests for Trajectory class from tspn/common.h
//
// Covers length, covers, distance_site, get_spanning_info,
// get_simplified_points, is_simple, is_valid, and edge cases
// that were previously untested.

#include <gtest/gtest.h>

#include "tspn_core/common.h"

namespace tspn {

// --- Length ---

TEST(TrajectoryExtended, LengthOfPath) {
  Trajectory traj{Linestring{{0, 0}, {3, 0}, {3, 4}}};
  EXPECT_NEAR(traj.length(), 7.0, 1e-9);
}

TEST(TrajectoryExtended, LengthOfTour) {
  // Square tour: 4 sides of length 2
  Trajectory traj{Linestring{{0, 0}, {2, 0}, {2, 2}, {0, 2}, {0, 0}}};
  EXPECT_NEAR(traj.length(), 8.0, 1e-9);
}

TEST(TrajectoryExtended, LengthCaching) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}}};
  double first = traj.length();
  double second = traj.length();
  EXPECT_DOUBLE_EQ(first, second);
  EXPECT_NEAR(first, 5.0, 1e-9);
}

// --- is_tour / is_path ---

TEST(TrajectoryExtended, TourDetection) {
  Trajectory tour{Linestring{{0, 0}, {5, 0}, {5, 5}, {0, 0}}};
  EXPECT_TRUE(tour.is_tour());
  EXPECT_FALSE(tour.is_path());
}

TEST(TrajectoryExtended, PathDetection) {
  Trajectory path{Linestring{{0, 0}, {5, 0}, {5, 5}}};
  EXPECT_TRUE(path.is_path());
  EXPECT_FALSE(path.is_tour());
}

// --- is_valid ---

TEST(TrajectoryExtended, ValidTrajectory) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}}, true};
  EXPECT_TRUE(traj.is_valid());
}

TEST(TrajectoryExtended, InvalidTrajectory) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}}, false};
  EXPECT_FALSE(traj.is_valid());
}

// --- is_simple ---

TEST(TrajectoryExtended, SimplePathNoSelfIntersection) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {5, 5}}};
  EXPECT_TRUE(traj.is_simple());
}

TEST(TrajectoryExtended, NonSimplePathSelfIntersecting) {
  // Figure-8 shape: self-intersects
  Trajectory traj{Linestring{{0, 0}, {2, 2}, {0, 2}, {2, 0}}};
  EXPECT_FALSE(traj.is_simple());
}

// --- num_points ---

TEST(TrajectoryExtended, NumPoints) {
  Trajectory traj{Linestring{{0, 0}, {1, 0}, {2, 0}, {3, 0}}};
  EXPECT_EQ(traj.num_points(), 4u);
}

// --- get / front / back ---

TEST(TrajectoryExtended, GetFrontBack) {
  Trajectory traj{Linestring{{1, 2}, {3, 4}, {5, 6}}};
  EXPECT_TRUE(bg::equals(traj.front(), Point(1, 2)));
  EXPECT_TRUE(bg::equals(traj.back(), Point(5, 6)));
  EXPECT_TRUE(bg::equals(traj.get(1), Point(3, 4)));
}

// --- covers ---

TEST(TrajectoryExtended, CoversGeometryOnTrajectory) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  auto sites = std::vector<SiteVariant>{
      Ring{{4, -1}, {6, -1}, {6, 1}, {4, 1}, {4, -1}},
  };
  Instance inst(sites);
  EXPECT_TRUE(traj.covers(inst[0], 0.001));
}

TEST(TrajectoryExtended, DoesNotCoverDistantGeometry) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  auto sites = std::vector<SiteVariant>{
      Ring{{4, 5}, {6, 5}, {6, 7}, {4, 7}, {4, 5}},
  };
  Instance inst(sites);
  EXPECT_FALSE(traj.covers(inst[0], 0.001));
}

TEST(TrajectoryExtended, CoversAllGeometries) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {10, 0}}};
  auto sites = std::vector<SiteVariant>{
      Ring{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}},
      Ring{{4, -1}, {6, -1}, {6, 1}, {4, 1}, {4, -1}},
      Ring{{9, -1}, {11, -1}, {11, 1}, {9, 1}, {9, -1}},
  };
  Instance inst(sites);
  EXPECT_TRUE(traj.covers(inst.begin(), inst.end(), 0.001));
}

// --- distance_site ---

TEST(TrajectoryExtended, DistanceSiteOnTrajectory) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  SiteVariant site = Point(5, 0);
  EXPECT_NEAR(traj.distance_site(site), 0.0, 1e-9);
}

TEST(TrajectoryExtended, DistanceSiteOffTrajectory) {
  Trajectory traj{Linestring{{0, 0}, {10, 0}}};
  SiteVariant site = Point(5, 3);
  EXPECT_NEAR(traj.distance_site(site), 3.0, 1e-9);
}

// --- get_spanning_info ---

TEST(TrajectoryExtended, SpanningInfoPathEndpointsAlwaysSpan) {
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {10, 0}}};
  auto info = traj.get_spanning_info();
  EXPECT_EQ(info.size(), 3u);
  // Endpoints of a path always span
  EXPECT_TRUE(info[0]);
  EXPECT_TRUE(info[2]);
  // Middle point on a straight line does not span
  EXPECT_FALSE(info[1]);
}

TEST(TrajectoryExtended, SpanningInfoAllSpanOnZigzag) {
  // All points are turning points in a zigzag
  Trajectory traj{Linestring{{0, 0}, {5, 5}, {10, 0}}};
  auto info = traj.get_spanning_info();
  EXPECT_EQ(info.size(), 3u);
  EXPECT_TRUE(info[0]);
  EXPECT_TRUE(info[1]); // Turning point
  EXPECT_TRUE(info[2]);
}

TEST(TrajectoryExtended, SpanningInfoTour) {
  // Square tour: all corners are turning points
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {5, 5}, {0, 5}, {0, 0}}};
  auto info = traj.get_spanning_info();
  EXPECT_EQ(info.size(), 5u);
  EXPECT_TRUE(info[0]);
  EXPECT_TRUE(info[1]);
  EXPECT_TRUE(info[2]);
  EXPECT_TRUE(info[3]);
}

// --- get_simplified_points ---

TEST(TrajectoryExtended, SimplifiedRemovesCollinear) {
  // Middle point is on the line, should be removed
  Trajectory traj{Linestring{{0, 0}, {5, 0}, {10, 0}}};
  auto simplified = traj.get_simplified_points();
  EXPECT_EQ(simplified.size(), 2u); // Only endpoints remain
}

TEST(TrajectoryExtended, SimplifiedKeepsTurningPoints) {
  Trajectory traj{Linestring{{0, 0}, {5, 5}, {10, 0}}};
  auto simplified = traj.get_simplified_points();
  EXPECT_EQ(simplified.size(), 3u); // All are spanning
}

// --- sub-trajectory edge cases ---

TEST(TrajectoryExtended, SubTrajectoryFullRange) {
  Trajectory traj{Linestring{{0, 0}, {1, 0}, {2, 0}, {3, 0}}};
  auto sub = traj.sub(0, 3);
  EXPECT_EQ(sub.points.size(), 4u);
}

TEST(TrajectoryExtended, SubTrajectorySinglePoint) {
  Trajectory traj{Linestring{{0, 0}, {1, 0}, {2, 0}}};
  auto sub = traj.sub(1, 1);
  EXPECT_EQ(sub.points.size(), 1u);
  EXPECT_TRUE(bg::equals(sub.points[0], Point(1, 0)));
}

} // namespace tspn
