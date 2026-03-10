/**
 * The partial sequence solution is a fundamental part of the BnB.
 * It essentially represents the relaxed solution, but also takes care of the
 * nasty computation of the optimal tour through the circles.
 * Using this class, you can easily build you own BnB-algorithm.
 * It is important that some of the operators are lazy.
 */
#pragma once

#include "tspn_core/common.h"
#include "tspn_core/details/lazy_trajectory.h"
#include "tspn_core/soc.h"
#include <list>
#include <set>
#include <vector>

namespace tspn {
class StashedTourElement {
public:
  StashedTourElement(TourElement &te)
      : tour_element{te}, earlier_geo_idxs{}, later_geo_idxs{} {};
  std::tuple<size_t, size_t>
  get_reinsert_positions(const std::vector<TourElement> &sequence) const;

  TourElement tour_element;
  std::set<unsigned> earlier_geo_idxs;
  std::set<unsigned> later_geo_idxs;
};

class PartialSequenceSolution {
  /**
   * This class simplifies the handling of the relaxed solution that is based
   * on a sequence of circles, that allow to trigger_lazy_computation the
   * optimal tour respecting this order using a second order cone program.
   */
public:
  PartialSequenceSolution(const Instance *instance, SocSolver *soc,
                          std::vector<TourElement> sequence_,
                          std::list<StashedTourElement> stash,
                          double feasibility_tol = 0.001)
      : stash{stash}, spanning_trajectory(instance, soc, std::move(sequence_)),
        FEASIBILITY_TOL{feasibility_tol}, instance{instance} {
    const auto &sequence = spanning_trajectory.sequence;
    if (sequence.empty() && !instance->is_path()) {
      throw std::invalid_argument("Cannot trigger_lazy_computation tour "
                                  "trajectory from empty sequence.");
    }
    assert(std::all_of(sequence.begin(), sequence.end(),
                       [&instance](const auto &te) {
                         return te.geo_index() < instance->size();
                       }));
  }

  bool trigger_lazy_computation(bool with_feasibility = false) const {
    const auto fresh = spanning_trajectory.trigger_computation();
    if (with_feasibility) {
      is_feasible();
    }
    return fresh;
  }

  void set_ub_cutoff(double cutoff) {
    spanning_trajectory.set_ub_cutoff(cutoff);
  }

  /**
   * Returns true if the i-th circle in the sequence is spanning. This
   * information is useful to simplify the solution.
   * @param i The index of the circle within the sequence. Not the index of  the
   * circle in the solution.
   * @return True if it spans the trajectory.
   */
  bool is_sequence_index_spanning(unsigned i) const {
    trigger_lazy_computation();
    return spanning_trajectory.get_spanning_information()[i];
  }

  const Point &trajectory_begin() const {
    return get_trajectory().points.front();
  }

  const Point &trajectory_end() const { return get_trajectory().points.back(); }

  /**
   * Returns the point that covers the i-th site in the sequence. These
   * are the potential turning points in the trajectory.
   * Requires: tour element is actually active, otherwise will return one past.
   * @param i
   * @return
   */
  const Point get_sequence_hitting_point(unsigned te_index) const {
    int found_active_elements = -1;
    const auto &sequence = get_sequence();
    for (unsigned pos = 0; pos < te_index; pos++) {
      if (sequence[pos].is_active()) {
        found_active_elements++;
      }
    }
    Point p = get_trajectory().get(found_active_elements);
    return p;
  }

  const Trajectory &get_trajectory() const {
    return spanning_trajectory.get_trajectory();
  }

  const std::vector<TourElement> &get_sequence() const {
    return spanning_trajectory.sequence;
  }

  double obj() const { return spanning_trajectory.get_tour_length(); }
  double lower_bound() const { return spanning_trajectory.get_lb(); }

  double distance(unsigned i) const { return spanning_trajectory.distance(i); }

  std::vector<double> &distances() const {
    return spanning_trajectory.get_distances();
  }

  bool covers(int i) const { return spanning_trajectory.get_coverage()[i]; }

  auto get_from_stash(unsigned geo_index) const {
    return std::find_if(stash.begin(), stash.end(),
                        [geo_index](const auto &ste) {
                          return ste.tour_element.geo_index() == geo_index;
                        });
  }
  bool is_stashed(unsigned geo_index) const;

  void remove_from_stash(unsigned geo_index);

  void add_to_stash(std::vector<TourElement> &sequence, size_t from);

  bool is_feasible() const { return spanning_trajectory.is_feasible(); };

  /**
   * Simplify the sequence and the solution by removing implicitly covered
   * parts.
   */
  void simplify();
  std::list<StashedTourElement> stash;

protected:
  details::LazyTrajectoryComputation spanning_trajectory;

private:
  bool simplified = false;
  double FEASIBILITY_TOL;
  const Instance *instance;
};

class Solution : public PartialSequenceSolution {
public:
  Solution(const Instance *instance, SocSolver *soc,
           std::vector<TourElement> sequence_, double feasibility_tol = 0.001)
      : PartialSequenceSolution(instance, soc, sequence_, {}, feasibility_tol) {
    assert(is_feasible());
  }

  Solution(const Instance *instance, SocSolver *soc,
           std::vector<TourElement> sequence_, Trajectory traj, double lb,
           double feasibility_tol = 0.001)
      : PartialSequenceSolution(instance, soc, sequence_, {}, feasibility_tol) {
    spanning_trajectory.update(traj, lb);
    assert(is_feasible());
  }

  Solution(const PartialSequenceSolution &sol) : PartialSequenceSolution(sol) {
    assert(is_feasible());
  }

  Solution(PartialSequenceSolution &&sol)
      : PartialSequenceSolution(std::move(sol)) {
    assert(is_feasible());
  }
};

} // namespace tspn
