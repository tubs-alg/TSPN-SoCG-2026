// Tests for the parameter management system in tspn/common.h
//
// Covers set/get/reset for all parameters, validation (reject non-positive),
// and the generic name-based accessors.

#include <gtest/gtest.h>

#include "tspn_core/common.h"

namespace tspn::constants {

class ParametersTest : public ::testing::Test {
protected:
  void SetUp() override { reset_parameters(); }
  void TearDown() override { reset_parameters(); }
};

TEST_F(ParametersTest, DefaultValues) {
  EXPECT_DOUBLE_EQ(FEASIBILITY_TOLERANCE, 0.001);
  EXPECT_DOUBLE_EQ(SPANNING_TOLERANCE, 0.0009);
  EXPECT_EQ(MAX_TE_TOGGLE_COUNT, 1u);
  EXPECT_EQ(MAX_GEOM_TOGGLE_COUNT, 1u);
}

TEST_F(ParametersTest, SetFeasibilityTolerance) {
  set_feasibility_tolerance(0.05);
  EXPECT_DOUBLE_EQ(FEASIBILITY_TOLERANCE, 0.05);
}

TEST_F(ParametersTest, SetFeasibilityToleranceRejectsNonPositive) {
  EXPECT_THROW(set_feasibility_tolerance(0.0), std::invalid_argument);
  EXPECT_THROW(set_feasibility_tolerance(-1.0), std::invalid_argument);
}

TEST_F(ParametersTest, SetSpanningTolerance) {
  set_spanning_tolerance(0.1);
  EXPECT_DOUBLE_EQ(SPANNING_TOLERANCE, 0.1);
}

TEST_F(ParametersTest, SetSpanningToleranceRejectsNonPositive) {
  EXPECT_THROW(set_spanning_tolerance(0.0), std::invalid_argument);
  EXPECT_THROW(set_spanning_tolerance(-0.5), std::invalid_argument);
}

TEST_F(ParametersTest, SetToggleCounts) {
  set_max_te_toggle_count(5);
  EXPECT_EQ(MAX_TE_TOGGLE_COUNT, 5u);
  set_max_geom_toggle_count(3);
  EXPECT_EQ(MAX_GEOM_TOGGLE_COUNT, 3u);
}

TEST_F(ParametersTest, ResetRestoresDefaults) {
  set_feasibility_tolerance(999.0);
  set_spanning_tolerance(999.0);
  set_max_te_toggle_count(999);
  set_max_geom_toggle_count(999);
  reset_parameters();
  EXPECT_DOUBLE_EQ(FEASIBILITY_TOLERANCE, 0.001);
  EXPECT_DOUBLE_EQ(SPANNING_TOLERANCE, 0.0009);
  EXPECT_EQ(MAX_TE_TOGGLE_COUNT, 1u);
  EXPECT_EQ(MAX_GEOM_TOGGLE_COUNT, 1u);
}

// --- Generic name-based accessors ---

TEST_F(ParametersTest, SetFloatParameterByName) {
  set_float_parameter("FEASIBILITY_TOLERANCE", 0.5);
  EXPECT_DOUBLE_EQ(get_float_parameter("FEASIBILITY_TOLERANCE"), 0.5);

  set_float_parameter("spanning_tolerance", 0.3);
  EXPECT_DOUBLE_EQ(get_float_parameter("spanning_tolerance"), 0.3);
}

TEST_F(ParametersTest, SetFloatParameterUnknownThrows) {
  EXPECT_THROW(set_float_parameter("UNKNOWN_PARAM", 1.0),
               std::invalid_argument);
}

TEST_F(ParametersTest, GetFloatParameterUnknownThrows) {
  EXPECT_THROW(get_float_parameter("NONEXISTENT"), std::invalid_argument);
}

TEST_F(ParametersTest, SetIntParameterByName) {
  set_int_parameter("MAX_TE_TOGGLE_COUNT", 10);
  EXPECT_EQ(get_int_parameter("MAX_TE_TOGGLE_COUNT"), 10u);

  set_int_parameter("max_geom_toggle_count", 7);
  EXPECT_EQ(get_int_parameter("max_geom_toggle_count"), 7u);
}

TEST_F(ParametersTest, SetIntParameterUnknownThrows) {
  EXPECT_THROW(set_int_parameter("BOGUS", 1), std::invalid_argument);
}

TEST_F(ParametersTest, GetIntParameterUnknownThrows) {
  EXPECT_THROW(get_int_parameter("NOPE"), std::invalid_argument);
}

} // namespace tspn::constants
