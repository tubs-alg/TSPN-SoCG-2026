// Tests for SolutionPool in tspn/details/solution_pool.h
//
// Covers add_solution, upper bound tracking, and best solution retrieval.

#include <gtest/gtest.h>

#include "tspn/details/solution_pool.h"

namespace tspn {

TEST(SolutionPool, InitiallyEmpty) {
  SolutionPool pool;
  EXPECT_TRUE(pool.empty());
  EXPECT_EQ(pool.get_best_solution(), nullptr);
  EXPECT_EQ(pool.get_upper_bound(), std::numeric_limits<double>::infinity());
}

TEST(SolutionPool, AddSolutionUpdatesUpperBound) {
  // Create a minimal instance with two polygons for a valid solution
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{3, 0}, {4, 0}, {3, 1}}},
  };
  Instance inst(sites);
  SocSolver soc(false);
  std::vector<TourElement> seq = {TourElement(inst, 0), TourElement(inst, 1)};
  Solution sol(&inst, &soc, seq);

  SolutionPool pool;
  pool.add_solution(sol);
  EXPECT_FALSE(pool.empty());
  EXPECT_LT(pool.get_upper_bound(), std::numeric_limits<double>::infinity());
  EXPECT_NE(pool.get_best_solution(), nullptr);
}

TEST(SolutionPool, BetterSolutionUpdatesUpperBound) {
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{3, 0}, {4, 0}, {3, 1}}},
      Polygon{{{6, 0}, {7, 0}, {6, 1}}},
  };
  Instance inst(sites);
  SocSolver soc(false);

  // First solution: all three elements (longer tour)
  std::vector<TourElement> seq_long = {
      TourElement(inst, 0), TourElement(inst, 1), TourElement(inst, 2)};
  Solution sol_long(&inst, &soc, seq_long);

  // Second solution: just two elements (shorter tour)
  std::vector<TourElement> seq_short = {TourElement(inst, 0),
                                        TourElement(inst, 1)};
  Solution sol_short(&inst, &soc, seq_short);

  SolutionPool pool;
  // Add the longer tour first
  pool.add_solution(sol_long);
  double ub_after_first = pool.get_upper_bound();

  // Add the shorter tour — should update the bound
  pool.add_solution(sol_short);
  EXPECT_LT(pool.get_upper_bound(), ub_after_first);
}

TEST(SolutionPool, WorseSolutionIgnored) {
  auto sites = std::vector<SiteVariant>{
      Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      Polygon{{{3, 0}, {4, 0}, {3, 1}}},
      Polygon{{{6, 0}, {7, 0}, {6, 1}}},
  };
  Instance inst(sites);
  SocSolver soc(false);

  std::vector<TourElement> seq_short = {TourElement(inst, 0),
                                        TourElement(inst, 1)};
  Solution sol_short(&inst, &soc, seq_short);

  std::vector<TourElement> seq_long = {
      TourElement(inst, 0), TourElement(inst, 1), TourElement(inst, 2)};
  Solution sol_long(&inst, &soc, seq_long);

  SolutionPool pool;
  // Add short (better) first
  pool.add_solution(sol_short);
  double ub = pool.get_upper_bound();

  // Adding longer (worse) should not change bound
  pool.add_solution(sol_long);
  EXPECT_DOUBLE_EQ(pool.get_upper_bound(), ub);
}

} // namespace tspn
