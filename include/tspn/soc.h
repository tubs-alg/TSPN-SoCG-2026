/**
 * An important observation for the close enought TSP is that if the order
 * of the circles to be visited is given, we can compute the optimal trajectory
 * via a second order cone program (poly-time). This file provides this
 * functionality.
 *
 * It is probably easier to use via RelaxedSequenceSolution.
 *
 * Dominik Krupke, January 2023, Tel Aviv
 */
#pragma once

#include "doctest/doctest.h"
#include "tspn/common.h"
#include "tspn/utils/drawer.h"
#include <vector>

namespace tspn {
/*compute the minimum cost of inserting
 * tested_insertion[1]->...->tested_insertion[n-2]
 * between tested_insertion[0] and tested_insertion[n-1]
 */

class SocSolver {
public:
  explicit SocSolver(bool use_cutoff);

  /**
   * Computes the shortest tour through the sequence of tour elements. Will also
   * give LB and mathematical tour length along with the trajectory.
   * @param poly_sequence A sequence of tourelements.
   * @param path Defines if we want a tour or a path. The tour will return
   * to the hitting point of the first polygon, closing the trajectory. Note
   * that the path is not just a tour with one segment missing, but can look
   * completely different.
   * @return The lb, tour length, and trajectory.
   */
  std::tuple<double, double, Trajectory>
  compute_trajectory_with_information(const std::vector<TourElement> &sequence,
                                      bool path) const;

  /**
   * Like `compute_trajectory_with_information` but discarding the info.
   */
  Trajectory compute_trajectory(const std::vector<TourElement> &sequence,
                                bool path = false) const;

  /**
   * Update the cutoff, if used. thread-safe.
   */
  void update_cutoff(double new_cutoff);

private:
  bool use_cutoff;
  double cutoff_value = 1e100;
};

TEST_CASE("Simple_SOCP_test") {
  // define the Polygon sequence we want to have the trajectory for
  auto polygons = std::vector<SiteVariant>{
      Polygon{{{1, 0}, {0, 0}, {0, 1}}},
      Polygon{{{1, 2}, {0, 2}, {0, 3}}},
  };
  Instance instance(polygons);
  // compute the tour trajectory
  TourElement te0 = TourElement(instance, 0);
  TourElement te1 = TourElement(instance, 1);
  SocSolver solver(false);
  std::vector<TourElement> seq{te0, te1};
  auto traj = solver.compute_trajectory(seq, false);
  CHECK(traj.length() == doctest::Approx(2));
  // compute the path trajectory
  traj = solver.compute_trajectory(seq, true);
  CHECK(traj.length() == doctest::Approx(1));
  tspn::utils::draw_geometries("Simple_SOCP_test.svg", instance, traj);
}
} // namespace tspn
