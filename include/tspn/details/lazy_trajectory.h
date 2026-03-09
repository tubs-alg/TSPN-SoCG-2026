//
// Created by Dominik Krupke on 15.01.23.
//
#pragma once

#include "tspn/common.h"
#include "tspn/details/distance_cache.h"
#include "tspn/soc.h"
#include <vector>

namespace tspn::details {
class LazyTrajectoryComputation {
public:
  LazyTrajectoryComputation(const Instance *instance_, SocSolver *soc_,
                            std::vector<TourElement> sequence_)
      : instance{instance_}, soc{soc_}, sequence{std::move(sequence_)},
        distances{instance_, &sequence, nullptr} {}

  void wipe();
  void update(Trajectory &traj, double lb_);
  void update(Trajectory &&traj, double lb_);

  void set_ub_cutoff(double cutoff) { soc->update_cutoff(cutoff); }

  Trajectory &get_trajectory() const;
  std::vector<bool> &get_spanning_information() const;
  std::vector<bool> &get_coverage() const;
  std::vector<double> &get_distances() const;
  bool is_feasible() const;
  double get_lb() const;
  double get_tour_length() const;
  double distance(unsigned geo_index) const;

  bool trigger_computation() const;
  bool trigger_spanning() const;
  bool trigger_coverage() const;
  bool trigger_feasible() const;

  const Instance *instance;
  SocSolver *soc;

  std::vector<TourElement> sequence;

private:
  mutable std::optional<double> lb;
  mutable std::optional<double> tour_length;
  mutable std::optional<Trajectory> trajectory;
  mutable std::vector<bool> coverage_info;
  mutable std::optional<std::vector<bool>> spanning_info;
  mutable unsigned feasible_below = 0;
  mutable std::optional<bool> _feasible;
  mutable DistanceCache distances;
};
} // namespace tspn::details
