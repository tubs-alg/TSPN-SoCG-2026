// Tests for geometry utilities from tspn/utils/geometry.h

#include <gtest/gtest.h>

#include "tspn_core/utils/geometry.h"

namespace tspn::utils {

TEST(Geometry, DistanceToSegment) {
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {0, 0}), 0.0, 1e-6);
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {0, 1}), 1.0, 1e-6);
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {0, -1}), 1.0, 1e-6);
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {-1, 0}), 1.0, 1e-6);
  EXPECT_NEAR(distance_to_segment({0, 0}, {10, 0}, {11, 0}), 1.0, 1e-6);
}

} // namespace tspn::utils
