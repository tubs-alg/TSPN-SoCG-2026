#include "tspn/relaxed_solution.h"
#include "tspn/soc.h"
//
// Created by Dominik Krupke on 15.01.23.
// slightly modified by Rouven Kniep on 30.05.23
//

namespace bg = boost::geometry;

namespace tspn {

std::tuple<size_t, size_t> StashedTourElement::get_reinsert_positions(
    const std::vector<TourElement> &sequence) const {
  /**
   * returns a set of insertion position bounds. might be empty.
   */
  size_t earliest = 0;
  size_t latest = sequence.size();
  for (size_t pos = 0; pos < sequence.size(); pos++) {
    if (later_geo_idxs.contains(sequence[pos].geo_index())) {
      latest = pos;
      break;
    }
  }
  for (size_t pos = sequence.size(); pos > 0; pos--) {
    // set to pos after first found
    if (earlier_geo_idxs.contains(sequence[pos - 1].geo_index())) {
      earliest = pos;
      break;
    }
  }
  return {earliest, latest};
}

bool PartialSequenceSolution::is_stashed(unsigned geo_index) const {
  const auto result = get_from_stash(geo_index);
  return result != stash.end();
}

void PartialSequenceSolution::remove_from_stash(unsigned geo_index) {
  stash.remove_if([geo_index](const auto &ste) {
    return ste.tour_element.geo_index() == geo_index;
  });
}

void PartialSequenceSolution::add_to_stash(std::vector<TourElement> &sequence,
                                           size_t from) {
  unsigned stashed_index = sequence[from].geo_index();
  sequence[from].disable();
  StashedTourElement &ste = stash.emplace_back(sequence[from]);
  for (size_t pos = 0; pos < from; pos++) {
    ste.earlier_geo_idxs.insert(sequence[pos].geo_index());
  }
  for (size_t pos = from + 1; pos < sequence.size(); pos++) {
    ste.later_geo_idxs.insert(sequence[pos].geo_index());
  }
  // keep transitive order info.
  // extend *this*
  for (auto &existing : stash) {
    unsigned existing_index = existing.tour_element.geo_index();
    if (existing_index != stashed_index) {
      if (existing.earlier_geo_idxs.contains(stashed_index)) {
        // order: new *before* existing
        ste.later_geo_idxs.insert(existing_index);
        // consequence: everything after existing is after new
        ste.later_geo_idxs.insert(existing.later_geo_idxs.begin(),
                                  existing.later_geo_idxs.end());
        // everything after new is after existing
        existing.earlier_geo_idxs.insert(ste.earlier_geo_idxs.begin(),
                                         ste.earlier_geo_idxs.end());

      } else if (existing.later_geo_idxs.contains(stashed_index)) {
        // order: new *after* existing
        ste.earlier_geo_idxs.insert(existing_index);
        // consequence: everything before existing is before new
        ste.earlier_geo_idxs.insert(existing.earlier_geo_idxs.begin(),
                                    existing.earlier_geo_idxs.end());
        // everything after new is after existing
        existing.later_geo_idxs.insert(ste.later_geo_idxs.begin(),
                                       ste.later_geo_idxs.end());
      }
    }
  }
}

void PartialSequenceSolution::simplify() {

  if (simplified) {
    return;
  }
  // if trajectory not valid (cutoff), no children will be spawned anyways => no
  // checking necessary
  if (!spanning_trajectory.get_trajectory().is_valid()) {
    return;
  }

  auto &sequence = spanning_trajectory.sequence;

  if(sequence.size() < 12) {
    return;
  }

  std::vector<size_t> removed_positions = {};
  for (size_t pos = 1; pos < sequence.size(); pos++) {
    // DO NOT disable first element! otherwise insertion positions for new
    // elements will cause quite the ruffus.
    // alternatively: do checks for both BEFORE and AFTER in finding sequence
    // indices. but.. that'd be slow.
    const auto &te = sequence[pos];
    if (!is_sequence_index_spanning(pos) && te.can_be_disabled() &&
        !std::holds_alternative<Point>(te.geometry()->convex_hull())) {
      add_to_stash(sequence, pos);
      removed_positions.push_back(pos);
    }
  }
  for (auto pos = removed_positions.rbegin(); pos != removed_positions.rend();
       pos++) {
    // std::cout << "erasing stashed pos from seq " << *pos << "\n";
    sequence.erase(sequence.begin() + *pos);
  }
  simplified = true;
}
} // namespace tspn
