#ifndef TSPN_DP_H
#define TSPN_DP_H

#include "doctest/doctest.h"

#include <cassert>
#include <map>
#include <vector>

#include "tspn/common.h"
#include "voronoi.h"

namespace tspn {

namespace bg = boost::geometry;

namespace dp {
struct Cost {
  unsigned int previous_index;
  double value;

  explicit Cost(
      unsigned int prev_idx = std::numeric_limits<unsigned int>::infinity(),
      double val = std::numeric_limits<double>::infinity())
      : previous_index(prev_idx), value(val) {}
};

using CostMatrix = std::unordered_map<unsigned int, Cost>;

struct DPSolution { // use to store additional information about forward and
                    // backward traversal.
  Trajectory trajectory;
  double lower_bound;
  std::vector<CostMatrix> cost;
  std::vector<CostMatrix> backward_cost;
};

class DPSolver {
public:
  explicit DPSolver(
      const std::vector<CroppedVoronoiDiagram> &cropped_voronoi_diagrams)
      : m_cropped_voronoi_diagrams(cropped_voronoi_diagrams) {
    assert(!cropped_voronoi_diagrams.empty());
    assert(cropped_voronoi_diagrams.size() >= 3);
  }

  [[nodiscard]] DPSolution
  compute_trajectory(Point depot,
                     const std::vector<TourElement> &poly_sequence) const {

    auto forward_costs = compute_cost_for_sequence(depot, poly_sequence);
    auto backward_costs = compute_cost_for_sequence(
        depot,
        std::vector<TourElement>(poly_sequence.rbegin(), poly_sequence.rend()));

    auto min_total_cost =
        compute_min_total_cost(depot, poly_sequence, forward_costs);
    return DPSolution{
        extract_trajectory(depot, min_total_cost, poly_sequence, forward_costs),
        min_total_cost.value, std::move(forward_costs),
        std::move(backward_costs)};
  }

private:
  std::vector<CroppedVoronoiDiagram> m_cropped_voronoi_diagrams;

  Cost
  compute_min_total_cost(Point &depot,
                         const std::vector<TourElement> &poly_sequence,
                         const std::vector<CostMatrix> &forward_costs) const {
    auto &last_polygon =
        m_cropped_voronoi_diagrams[poly_sequence.back().geo_index()];
    auto min_total_cost = Cost();

    for (unsigned i = 0; i < last_polygon.n_sampling_points(); i++) {
      auto cost = forward_costs[poly_sequence.size() - 1].at(i).value +
                  bg::distance(depot, last_polygon.get_point(i));
      if (cost < min_total_cost.value) {
        min_total_cost = Cost{i, cost};
      }
    }

    return min_total_cost;
  }

  Trajectory
  extract_trajectory(Point &depot, const Cost &min_total_cost,
                     const std::vector<TourElement> &poly_sequence,
                     const std::vector<CostMatrix> &forward_costs) const {
    std::vector<Point> trajectory;
    trajectory.push_back(depot);

    auto previous_index = min_total_cost.previous_index;

    for (int matrix_idx = (int)poly_sequence.size() - 1; matrix_idx >= 0;
         --matrix_idx) {
      auto point =
          m_cropped_voronoi_diagrams[poly_sequence[matrix_idx].geo_index()]
              .get_point(previous_index);
      trajectory.push_back(point);
      previous_index =
          forward_costs.at(matrix_idx).at(previous_index).previous_index;
    }

    trajectory.push_back(depot);

    std::reverse(trajectory.begin(), trajectory.end());

    return Trajectory(std::move(trajectory), true);
  }

  std::vector<CostMatrix> compute_cost_for_sequence(
      Point &depot, const std::vector<TourElement> &poly_sequence) const {
    auto forward_costs = std::vector<CostMatrix>(poly_sequence.size());

    // fill inn the first cost with their index
    auto &first_polygon =
        m_cropped_voronoi_diagrams[poly_sequence.front().geo_index()];
    for (unsigned i = 0; i < first_polygon.n_sampling_points(); i++) {
      forward_costs[0][i] =
          Cost{std::numeric_limits<unsigned int>::infinity(),
               bg::distance(depot, first_polygon.get_point(i)) -
                   first_polygon.error_for_point(i)};
    }

    for (unsigned i = 1; i < poly_sequence.size(); i++) {
      auto &previous_cost = forward_costs[i - 1];
      auto &cost = forward_costs[i];
      compute_cost(poly_sequence[i - 1].geo_index(),
                   poly_sequence[i].geo_index(), previous_cost, cost);
    }

    return forward_costs;
  }

  void compute_cost(unsigned int previous_idx, unsigned int current_idx,
                    CostMatrix &previous_cost, CostMatrix &cost) const {
    /**
     * @param i The index of the current polygon in the sequence.
     * @param previous_cost A map containing the cost to the previous sampling
     * points.
     * @param cost A map containing used to store the cost to the current
     * sampling points.
     */

    if (current_idx >= m_cropped_voronoi_diagrams.size() ||
        previous_idx >= m_cropped_voronoi_diagrams.size()) {
      throw std::runtime_error("Invalid index");
    }

    auto n_previous_sp =
        m_cropped_voronoi_diagrams[previous_idx].n_sampling_points();
    auto n_current_sp =
        m_cropped_voronoi_diagrams[current_idx].n_sampling_points();

    for (unsigned j = 0; j < n_current_sp; j++) {
      cost[j] = Cost();

      auto current_point = m_cropped_voronoi_diagrams[current_idx].get_point(j);

      for (unsigned k = 0; k < n_previous_sp; k++) {
        auto other_point =
            m_cropped_voronoi_diagrams[previous_idx].get_point(k);

        auto distance = bg::distance(current_point, other_point);

        auto current_cost =
            previous_cost[k].value + distance -
            m_cropped_voronoi_diagrams[current_idx].error_for_point(j);

        if (current_cost < cost[j].value) {
          cost[j].previous_index = k;
          cost[j].value = current_cost;
        }
      }
    }
  }
};

TEST_CASE("Voronoi of polygon") {
  Instance instance;
  std::vector<double> x_shifts = {0, 2, 4};
  std::vector<double> y_shifts = {1, 2, 3};

  std::vector<CroppedVoronoiDiagram> cropped_voronoi_diagrams;

  for (unsigned i = 0; i < x_shifts.size(); i++) {
    std::vector<Point> points;
    points.emplace_back(0 + x_shifts[i], 0 + y_shifts[i]);
    points.emplace_back(1 + x_shifts[i], 1 + y_shifts[i]);
    points.emplace_back(0 + x_shifts[i], 1 + y_shifts[i]);

    auto polygon = Polygon{{
        {-1 + x_shifts[i], -1 + y_shifts[i]},
        {-1 + x_shifts[i], 2 + y_shifts[i]},
        {2 + x_shifts[i], 3 + y_shifts[i]},
        {3 + x_shifts[i], 3 + y_shifts[i]},
        {5 + x_shifts[i], -2 + y_shifts[i]},
        {4 + x_shifts[i], -1 + y_shifts[i]},
    }};

    instance.add_site(polygon);

    tspn::CroppedVoronoiDiagram vor(polygon);
    vor.insert(points.begin(), points.end());
    vor.recompute();

    cropped_voronoi_diagrams.push_back(vor);
  }

  DPSolver solver(cropped_voronoi_diagrams);

  auto trajectory =
      solver.compute_trajectory(Point(0, 0), {
                                                 TourElement(instance, 0),
                                                 TourElement(instance, 1),
                                                 TourElement(instance, 2),
                                             });

  CHECK(trajectory.trajectory.length() == doctest::Approx(13.1113));
  CHECK(trajectory.trajectory.is_tour());
}
} // namespace dp
} // namespace tspn

#endif // TSPN_DP_H
