#include "tspn_core/details/lazy_trajectory.h"
#include "tspn_core/utils/distance.h"
namespace tspn::details {

void LazyTrajectoryComputation::wipe() {
  lb.reset();
  tour_length.reset();
  trajectory.reset();
  coverage_info.clear();
  spanning_info.reset();
  feasible_below = 0;
  _feasible.reset();
  distances.wipe();
}

void LazyTrajectoryComputation::update(Trajectory &traj, double lb_) {
  wipe();
  trajectory = traj;
  lb = lb_;
  tour_length = (*trajectory).length();
  distances.setup_trajectory(&(*trajectory));
}

void LazyTrajectoryComputation::update(Trajectory &&traj, double lb_) {
  wipe();
  trajectory = std::move(traj);
  lb = lb_;
  tour_length = (*trajectory).length();
  distances.setup_trajectory(&(*trajectory));
}

Trajectory &LazyTrajectoryComputation::get_trajectory() const {
  trigger_computation();
  return *trajectory;
}

std::vector<bool> &LazyTrajectoryComputation::get_spanning_information() const {
  trigger_spanning();
  return *spanning_info;
}

std::vector<bool> &LazyTrajectoryComputation::get_coverage() const {
  trigger_coverage();
  return coverage_info;
}

std::vector<double> &LazyTrajectoryComputation::get_distances() const {
  trigger_computation();
  distances.fill_cache();
  return distances.get_distances();
}

bool LazyTrajectoryComputation::is_feasible() const {
  if (_feasible && !*_feasible) {
    return false;
  }
  if (!get_trajectory().is_valid()) {
    return false;
  }
  trigger_coverage();
  if (!_feasible || *_feasible) {
    _feasible = true;
    // will be cached if the instance hasn't changed. Otherwise, only the
    // unchecked instances will be checked.
    for (; feasible_below < instance->size(); feasible_below++) {
      if (!coverage_info[feasible_below]) {
        _feasible = false;
        break;
      }
    }
  }
  return *_feasible;
}

double LazyTrajectoryComputation::get_lb() const {
  trigger_computation();
  return *lb;
}

double LazyTrajectoryComputation::get_tour_length() const {
  trigger_computation();
  return *tour_length;
}

double LazyTrajectoryComputation::distance(unsigned geo_index) const {
  return distances(geo_index);
}

bool LazyTrajectoryComputation::trigger_computation() const {
  if (trajectory) {
    return false;
  }
  auto result =
      soc->compute_trajectory_with_information(sequence, instance->is_path());
  lb = std::get<0>(result);
  tour_length = std::get<1>(result);
  trajectory = std::move(std::get<2>(result));
  distances.setup_trajectory(&(*trajectory));
  return true;
}

bool LazyTrajectoryComputation::trigger_spanning() const {
  if (spanning_info) {
    return false;
  }
  trigger_computation();
  spanning_info = std::move(trajectory->get_spanning_info());
  return true;
}

bool LazyTrajectoryComputation::trigger_coverage() const {
  if (coverage_info.size() == instance->size()) {
    return false;
  }
  trigger_computation();
  coverage_info.reserve(instance->size());
  for (unsigned i = coverage_info.size(); i < instance->size(); i++) {
    coverage_info.push_back(distances(i) <=
                            tspn::constants::FEASIBILITY_TOLERANCE);
  }
  return true;
}

} // namespace tspn::details
