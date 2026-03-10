/**
 * Computing the distance to a trajectory seems to be an expensive operation,
 * so we cache it.
 */
#pragma once

#include "tspn_core/common.h"
#include "tspn_core/details/lazy_trajectory.h"
#include <vector>
namespace tspn::details {

class DistanceCache {
  /**
   * Just a simple cached distance calculator.
   * TODO: Improve runtime. Current implementation of just using the trajectory
   * method is slow.
   */
public:
  explicit DistanceCache(const Instance *instance,
                         const std::vector<TourElement> *sequence,
                         Trajectory *traj)
      : instance{instance}, sequence{sequence}, traj{traj} {}

  double operator()(unsigned i) {
    assert(i < instance->size());
    if (i >= cache.size()) {
      fill_cache();
    }
    return cache[i];
  }

  const Instance *instance;
  const std::vector<TourElement> *sequence;
  Trajectory *traj;

  void setup_trajectory(Trajectory *nt) {
    cache.clear();
    traj = nt;
  }
  void wipe() {
    traj = nullptr;
    cache.clear();
  }

  void fill_cache() {
    cache.reserve(instance->size());
    for (unsigned i = cache.size(); i < instance->size(); i++) {
      cache.push_back(
          traj->distance_geometry_sequence((*instance)[i], *sequence));
    }
  }
  std::vector<double> &get_distances() const { return cache; }

private:
  mutable std::vector<double> cache;
};

} // namespace tspn::details
