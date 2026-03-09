#include "tspn/strategies/order_filtering.h"

#include <unordered_set>

namespace tspn {

void OrderFiltering::setup(const Instance *instance,
                           std::shared_ptr<Node> & /*root*/,
                           SolutionPool * /*solution_pool*/) {
  instance_ = instance;

  if (instance_ != nullptr) {
    unsigned annotated = 0;
    std::size_t overlap_total = 0;
    unsigned min_order = std::numeric_limits<unsigned>::max();
    unsigned max_order = 0;
    std::vector<unsigned> annotated_indices;

    for (const auto &geo : *instance_) {
      if (geo.annotations.order_index.has_value()) {
        annotated++;
        annotated_indices.push_back(geo.geo_index());
        min_order = std::min(min_order, *geo.annotations.order_index);
        max_order = std::max(max_order, *geo.annotations.order_index);
        overlap_total += geo.annotations.overlapping_order_geo_indices.size();
      }
    }
    double avg_overlap =
        annotated > 0 ? static_cast<double>(overlap_total) / annotated : 0.0;
    std::cout << "[OrderFiltering] annotated: " << annotated
              << " avg_overlap: " << avg_overlap;
    if (annotated > 0) {
      std::cout << " order_range: [" << min_order << ", " << max_order << "]";
    }
    if (!annotated_indices.empty()) {
      std::cout << " indices:";
      for (auto idx : annotated_indices) {
        std::cout << " " << idx;
      }
    }
    std::cout << std::endl;
  }
}

bool OrderFiltering::is_ok(const std::vector<TourElement> &seq,
                           const Node &parent) {
  if (instance_ == nullptr) {
    return true;
  }

  const auto &parent_seq = parent.get_fixed_sequence();
  if (seq.size() <= parent_seq.size()) {
    return true;
  }

  auto new_idx_opt = find_new_element(seq, parent_seq);
  if (!new_idx_opt.has_value()) {
    return true;
  }
  const unsigned new_idx = *new_idx_opt;

  const Geometry &new_geo = (*instance_)[new_idx];
  if (!new_geo.annotations.order_index.has_value()) {
    return true;
  }
  const unsigned order_j = *new_geo.annotations.order_index;

  std::optional<std::size_t> pos_j;
  for (std::size_t p = 0; p < seq.size(); ++p) {
    if (seq[p].geo_index() == new_idx) {
      pos_j = p;
      break;
    }
  }
  if (!pos_j.has_value()) {
    return true;
  }

  auto is_between_cyclic = [](unsigned a, unsigned b, unsigned c) {
    if (a < c) {
      return (a < b && b < c);
    }
    return (b > a) || (b < c);
  };

  for (std::size_t p_i = 0; p_i < seq.size(); ++p_i) {
    if (p_i == *pos_j) {
      continue;
    }
    const Geometry &geo_i = (*instance_)[seq[p_i].geo_index()];
    if (!geo_i.annotations.order_index.has_value()) {
      continue;
    }

    const auto &overlaps_i = geo_i.annotations.overlapping_order_geo_indices;

    // Skip constraint if any pair overlaps
    if (std::find(overlaps_i.begin(), overlaps_i.end(), new_idx) != overlaps_i.end()) {
        continue;
    }

    const unsigned order_i = *geo_i.annotations.order_index;

    for (std::size_t p_k = 0; p_k < seq.size(); ++p_k) {
      if (p_k == *pos_j || p_k == p_i) {
        continue;
      }
      const Geometry &geo_k = (*instance_)[seq[p_k].geo_index()];
      if (!geo_k.annotations.order_index.has_value()) {
        continue;
      }
      const auto &overlaps_k = geo_k.annotations.overlapping_order_geo_indices;

      if(std::find(overlaps_k.begin(), overlaps_k.end(), new_idx) != overlaps_k.end() || // new_idx overlaps k
         std::find(overlaps_i.begin(), overlaps_i.end(), seq[p_k].geo_index()) != overlaps_i.end()) { // i overlaps k
        continue;
      }

      const unsigned order_k = *geo_k.annotations.order_index;

      const bool i_before_j = p_i < *pos_j;
      const bool k_before_j = p_k < *pos_j;
      if ((i_before_j && k_before_j) || (!i_before_j && !k_before_j)) {
        continue;
      }

      if (order_i == order_j || order_k == order_j || order_i == order_k) {
        continue;
      }
      if ((i_before_j && !is_between_cyclic(order_i, order_j, order_k)) ||
          (k_before_j && !is_between_cyclic(order_k, order_j, order_i))) {
        return false;
      }

    }
  }

  return true;
}

std::optional<unsigned>
OrderFiltering::find_new_element(const std::vector<TourElement> &seq,
                                 const std::vector<TourElement> &parent_seq) const {
  std::unordered_set<unsigned> parent_ids;
  parent_ids.reserve(parent_seq.size());
  for (const auto &te : parent_seq) {
    parent_ids.insert(te.geo_index());
  }

  for (const auto &te : seq) {
    if (!parent_ids.contains(te.geo_index())) {
      return te.geo_index();
    }
  }

  return std::nullopt;
}

} // namespace tspn
