#include "tspn/strategies/root_node_strategy.h"
#include "tspn/utils/distance.h"
#include "tspn/utils/geometry.h"
#include <algorithm>
#include <boost/geometry.hpp>
#include <numeric>
#include <random>
#include <unordered_set>
#include <vector>

namespace bg = boost::geometry;

namespace tspn {
std::pair<unsigned, unsigned> find_max_pair(const Instance &instance) {
  /**
   * Find the poly pair with the longest distance between.
   */
  double max_dist = 0;
  std::pair<unsigned, unsigned> best_pair;
  for (unsigned i = 0; i < instance.size(); i++) {
    for (unsigned j = 0; j < i; j++) {
      auto dist = tspn::utils::distance(instance[i].definition(),
                                        instance[j].definition());
      if (dist >= max_dist) {
        best_pair = {i, j};
        max_dist = dist;
      }
    }
  }
  return best_pair;
}

int most_distanced_site(const Instance &instance, const Geometry &geo_a,
                        const Geometry &geo_b) {
  int most_distanced_index = -1;
  double max_dist = -1; // ensure immediate overwrite
  for (unsigned i = 0; i < instance.size(); i++) {
    if (i != geo_a.geo_index() && i != geo_b.geo_index()) {
      double dist =
          tspn::utils::distance(geo_a.definition(), instance[i].definition()) +
          tspn::utils::distance(geo_b.definition(), instance[i].definition());

      if (dist > max_dist) {
        max_dist = dist;
        most_distanced_index = i;
      }
    }
  }
  return most_distanced_index;
}
std::shared_ptr<Node> path_furtest_site_root(Instance &instance,
                                             SocSolver &soc) {
  assert(instance.is_path());
  if (size(instance) <= 1) {
    throw std::logic_error("cannot build path root node from less than two "
                           "sites (origin+target).");
  }
  std::vector<TourElement> seq;
  auto origin = instance[0];
  seq.push_back(TourElement(instance, 0));
  auto target = instance[1];
  if (size(instance) >= 3) {
    int mdp = most_distanced_site(instance, origin, target);
    seq.push_back(TourElement(instance, mdp));
  }
  seq.push_back(TourElement(instance, 1));
  return std::make_shared<Node>(seq, &instance, &soc);
}

std::shared_ptr<Node>
LongestEdgePlusFurthestSite::get_root_node(Instance &instance, SocSolver &soc) {
  /**
   * Compute a root note consisting of three sites by first finding
   * the most distanced pair and then adding a third site that has the
   * longest sum of distance to the two end points.
   */
  std::cout << "using LongestEdgePlusFurthestSite Root" << std::endl;
  if (instance.is_path()) {
    return path_furtest_site_root(instance, soc);
  } else {
    if (instance.size() <= 3) { // trivial case
      std::vector<TourElement> seq;
      for (unsigned i = 0; i < instance.size(); i++) {
        seq.push_back(TourElement(instance, i));
      }
      return std::make_shared<Node>(seq, &instance, &soc);
    } else {
      auto max_pair = find_max_pair(instance);
      auto furthest = most_distanced_site(instance, instance[max_pair.first],
                                          instance[max_pair.second]);

      std::cout << "  picked pair (" << max_pair.first << ", "
                << max_pair.second << ") furthest " << furthest << std::endl;
      assert(max_pair.first < instance.size());
      assert(max_pair.second < instance.size());
      assert(furthest < instance.size() && furthest >= 0);
      return std::make_shared<Node>(
          std::vector<TourElement>{TourElement(instance, max_pair.first),
                                   TourElement(instance, furthest),
                                   TourElement(instance, max_pair.second)},
          &instance, &soc);
    }
  }
}

std::shared_ptr<Node> LongestTriple::get_root_node(Instance &instance,
                                                   SocSolver &soc) {
  /**
   * The longest triple strategy selects the three sites with the longest
   * cumulative distances between them. In path mode, it selects the site with
   * largest cumulative distance to origin and target.
   */
  std::cout << "using LongestTriple Root" << std::endl;
  if (instance.is_path()) {
    return path_furtest_site_root(instance, soc);
  } else if (instance.size() <= 3) { // trivial case
    std::vector<TourElement> seq;
    for (unsigned i = 0; i < instance.size(); i++) {
      seq.push_back(TourElement(instance, i));
    }
    return std::make_shared<Node>(seq, &instance, &soc);
  } else {
    std::vector<unsigned> best_sites = {0, 0, 0};
    double longest_distance = 0;
    for (unsigned a = 0; a < instance.size() - 2; a++) {
      const auto &site_a = instance.at(a).definition();
      for (unsigned b = a + 1; b < instance.size() - 1; b++) {
        const auto &site_b = instance.at(b).definition();
        double distance_ab = tspn::utils::distance(site_a, site_b);
        for (unsigned c = b + 1; c < instance.size() - 0; c++) {
          const auto &site_c = instance.at(c).definition();
          double distance_ac = tspn::utils::distance(site_a, site_c);
          double distance_bc = tspn::utils::distance(site_b, site_c);
          if (distance_ab + distance_ac + distance_bc >= longest_distance) {
            best_sites = {a, b, c};
            longest_distance = distance_ab + distance_ac + distance_bc;
          }
        }
      }
    }
    return std::make_shared<Node>(
        std::vector<TourElement>{TourElement(instance, best_sites[0]),
                                 TourElement(instance, best_sites[1]),
                                 TourElement(instance, best_sites[2])},
        &instance, &soc);
  }
}

std::shared_ptr<Node> LongestTripleWithPoint::get_root_node(Instance &instance,
                                                            SocSolver &soc) {
  // for convenience, the point shall be the very first site in the sequence.
  std::cout << "using LongestTripleWithPoint Root" << std::endl;

  // find all points
  std::vector<unsigned> point_positions;
  for (unsigned i = 0; i < instance.size(); i++) {
    if (std::holds_alternative<Point>(instance[i].definition())) {
      point_positions.push_back(i);
    }
  }
  if (instance.is_path()) {
    return path_furtest_site_root(instance, soc);
  } else if (point_positions.size() == 0) {
    throw std::logic_error("cannot build LongestTripleWithPoint root node for "
                           "instance without at least one point.");
  } else if (instance.size() <= 3) { // trivial case
    // may be one after end, but we caught that with none_of already
    std::vector<TourElement> seq;
    seq.push_back(TourElement(instance, point_positions[0]));
    for (unsigned i = 0; i < instance.size(); i++) {
      if (i != point_positions[0]) {
        seq.push_back(TourElement(instance, i));
      }
    }
    return std::make_shared<Node>(seq, &instance, &soc);
  } else {
    std::vector<unsigned> best_sites = {0, 0, 0};
    double longest_distance = 0;
    for (unsigned a : point_positions) {
      const auto &site_a = instance.at(a).definition();
      for (unsigned b = 0; b < instance.size() - 1; b++) {
        if (b != a) {
          const auto &site_b = instance.at(b).definition();
          double distance_ab = tspn::utils::distance(site_a, site_b);
          for (unsigned c = b + 1; c < instance.size() - 0; c++) {
            if (c != a) {
              const auto &site_c = instance.at(c).definition();
              double distance_ac = tspn::utils::distance(site_a, site_c);
              double distance_bc = tspn::utils::distance(site_b, site_c);
              if (distance_ab + distance_ac + distance_bc >= longest_distance) {
                best_sites = {a, b, c};
                longest_distance = distance_ab + distance_ac + distance_bc;
              }
            }
          }
        }
      }
    }
    return std::make_shared<Node>(
        std::vector<TourElement>{TourElement(instance, best_sites[0]),
                                 TourElement(instance, best_sites[1]),
                                 TourElement(instance, best_sites[2])},
        &instance, &soc);
  }
} // namespace tspn

std::shared_ptr<Node> LongestPointTriple::get_root_node(Instance &instance,
                                                        SocSolver &soc) {
  // for convenience, the point shall be the very first site in the sequence.
  std::cout << "using LongestPointTriple Root" << std::endl;

  // find all points, will be practial through everything
  std::vector<unsigned> point_positions;
  std::vector<unsigned> other_positions;
  for (unsigned i = 0; i < instance.size(); i++) {
    if (std::holds_alternative<Point>(instance[i].definition())) {
      point_positions.push_back(i);
    } else {
      other_positions.push_back(i);
    }
  }
  if (instance.is_path()) {
    return path_furtest_site_root(instance, soc);
  } else if (point_positions.size() == 0) {
    throw std::logic_error("cannot build LongestPointTriple root node for "
                           "instance without at least one point.");
  } else if (instance.size() <= 3) { // trivial case
    // may be one after end, but we caught that with none_of already
    std::vector<TourElement> seq;
    seq.push_back(TourElement(instance, point_positions[0]));
    for (unsigned i = 0; i < instance.size(); i++) {
      if (i != point_positions[0]) {
        seq.push_back(TourElement(instance, i));
      }
    }
    return std::make_shared<Node>(seq, &instance, &soc);
  } else {
    std::vector<unsigned> best_sites = {0, 0, 0};
    double longest_distance = 0;
    if (point_positions.size() == 1) {
      const auto &site_a = instance.at(point_positions[0]).definition();
      for (unsigned b = 0; b < instance.size() - 1; b++) {
        if (b != point_positions[0]) {
          const auto &site_b = instance.at(b).definition();
          double distance_ab = tspn::utils::distance(site_a, site_b);
          for (unsigned c = b + 1; c < instance.size() - 0; c++) {
            if (c != point_positions[0]) {
              const auto &site_c = instance.at(c).definition();
              double distance_ac = tspn::utils::distance(site_a, site_c);
              double distance_bc = tspn::utils::distance(site_b, site_c);
              if (distance_ab + distance_ac + distance_bc >= longest_distance) {
                best_sites = {point_positions[0], b, c};
                longest_distance = distance_ab + distance_ac + distance_bc;
              }
            }
          }
        }
      }
    } else if (point_positions.size() == 2) {
      const auto &site_a = instance.at(point_positions[0]).definition();
      const auto &site_b = instance.at(point_positions[1]).definition();
      double distance_ab = tspn::utils::distance(site_a, site_b);
      for (unsigned c = 0; c < instance.size() - 0; c++) {
        if (c != point_positions[0] && c != point_positions[1]) {
          const auto &site_c = instance.at(c).definition();
          double distance_ac = tspn::utils::distance(site_a, site_c);
          double distance_bc = tspn::utils::distance(site_b, site_c);
          if (distance_ab + distance_ac + distance_bc >= longest_distance) {
            best_sites = {point_positions[0], point_positions[1], c};
            longest_distance = distance_ab + distance_ac + distance_bc;
          }
        }
      }
    } else {
      for (unsigned a = 0; a < point_positions.size() - 2; a++) {
        const auto &site_a = instance.at(point_positions[a]).definition();
        for (unsigned b = a + 1; b < point_positions.size() - 1; b++) {
          const auto &site_b = instance.at(point_positions[b]).definition();
          double distance_ab = tspn::utils::distance(site_a, site_b);
          for (unsigned c = b + 1; c < point_positions.size() - 0; c++) {
            const auto &site_c = instance.at(point_positions[c]).definition();
            double distance_ac = tspn::utils::distance(site_a, site_c);
            double distance_bc = tspn::utils::distance(site_b, site_c);
            if (distance_ab + distance_ac + distance_bc >= longest_distance) {
              best_sites = {a, b, c};
              longest_distance = distance_ab + distance_ac + distance_bc;
            }
          }
        }
      }
    }
    return std::make_shared<Node>(
        std::vector<TourElement>{TourElement(instance, best_sites[0]),
                                 TourElement(instance, best_sites[1]),
                                 TourElement(instance, best_sites[2])},
        &instance, &soc);
  }
}

std::shared_ptr<Node> LongestPair::get_root_node(Instance &instance,
                                                 SocSolver &soc) {
  std::cout << "using LongestPair Root" << std::endl;
  if (instance.is_path()) {
    // path requires origin/target at ends
    std::vector<TourElement> seq;
    seq.emplace_back(instance, 0);
    if (instance.size() >= 2) {
      seq.emplace_back(instance, 1);
    }
    std::cout << "  path root -> [" << 0 << ", "
              << (instance.size() >= 2 ? 1 : 0) << "]" << std::endl;
    return std::make_shared<Node>(seq, &instance, &soc);
  }
  if (instance.size() <= 2) {
    std::vector<TourElement> seq;
    for (unsigned i = 0; i < instance.size(); ++i) {
      seq.emplace_back(instance, i);
    }
    std::cout << "  tour root trivial size <=2, seq size " << seq.size()
              << std::endl;
    return std::make_shared<Node>(seq, &instance, &soc);
  }
  auto max_pair = find_max_pair(instance);
  std::cout << "  tour root max pair (" << max_pair.first << ", "
            << max_pair.second << ")" << std::endl;
  return std::make_shared<Node>(
      std::vector<TourElement>{TourElement(instance, max_pair.first),
                               TourElement(instance, max_pair.second)},
      &instance, &soc);
}

std::shared_ptr<Node> RandomPair::get_root_node(Instance &instance,
                                                SocSolver &soc) {
  std::cout << "using RandomPair Root" << std::endl;
  if (instance.is_path()) {
    std::vector<TourElement> seq;
    seq.emplace_back(instance, 0);
    if (instance.size() >= 2) {
      seq.emplace_back(instance, 1);
    }
    std::cout << "  path root -> [" << 0 << ", "
              << (instance.size() >= 2 ? 1 : 0) << "]" << std::endl;
    return std::make_shared<Node>(seq, &instance, &soc);
  }
  if (instance.size() <= 2) {
    std::vector<TourElement> seq;
    for (unsigned i = 0; i < instance.size(); ++i) {
      seq.emplace_back(instance, i);
    }
    std::cout << "  tour root trivial size <=2, seq size " << seq.size()
              << std::endl;
    return std::make_shared<Node>(seq, &instance, &soc);
  }
  std::vector<unsigned> indices(instance.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::shuffle(indices.begin(), indices.end(),
               std::mt19937{std::random_device{}()});
  std::vector<TourElement> seq{TourElement(instance, indices[0]),
                               TourElement(instance, indices[1])};
  std::cout << "  tour root random pair (" << indices[0] << ", " << indices[1]
            << ")" << std::endl;
  return std::make_shared<Node>(seq, &instance, &soc);
}

std::shared_ptr<Node> OrderRootStrategy::get_root_node(Instance &instance,
                                                       SocSolver &soc) {
  std::cout << "using OrderRootStrategy Root" << std::endl;
  // For paths, keep origin/target in place.
  std::vector<TourElement> seq;
  std::vector<std::pair<unsigned, unsigned>> annotated; // (order, geo_idx)

  for (unsigned idx = 0; idx < instance.size(); ++idx) {
    const auto &geo = instance[idx];
    if (geo.annotations.order_index.has_value()) {
      annotated.emplace_back(*geo.annotations.order_index, idx);
    }
  }

  std::sort(annotated.begin(), annotated.end(),
            [](const auto &a, const auto &b) {
              if (a.first == b.first) {
                return a.second < b.second;
              }
              return a.first < b.first;
            });

  std::unordered_set<unsigned> excluded;
  std::vector<unsigned> selected;

  for (const auto &[order, geo_idx] : annotated) {
    (void)order;
    if (excluded.contains(geo_idx)) {
      continue;
    }
    selected.push_back(geo_idx);
    const auto &overlaps =
        instance[geo_idx].annotations.overlapping_order_geo_indices;
    excluded.insert(overlaps.begin(), overlaps.end());
  }

  std::cout << "  annotated count: " << annotated.size()
            << " selected: " << selected.size() << std::endl;
  if (!selected.empty()) {
    std::cout << "  selected indices:";
    for (auto idx : selected) {
      std::cout << " " << idx;
    }
    std::cout << std::endl;
  }

  if (instance.is_path()) {
    if (instance.size() >= 1) {
      seq.emplace_back(instance, 0); // origin
    }
    for (auto idx : selected) {
      if (idx == 0 || idx == 1) {
        continue;
      }
      seq.emplace_back(instance, idx);
    }
    if (instance.size() >= 2) {
      seq.emplace_back(instance, 1); // target
    }
    if (seq.empty()) {
      throw std::logic_error("Cannot build path root without any sites.");
    }
    std::cout << "  path seq size " << seq.size() << std::endl;
    return std::make_shared<Node>(seq, &instance, &soc);
  }

  if (selected.empty()) {
    // fallback to farthest pair heuristic
    auto max_pair = find_max_pair(instance);
    seq.emplace_back(instance, max_pair.first);
    if (instance.size() > 1) {
      seq.emplace_back(instance, max_pair.second);
    }
    std::cout << "  fallback max pair ("
              << (seq.empty() ? 0 : seq[0].geo_index()) << ", "
              << (seq.size() > 1 ? seq[1].geo_index() : 0) << ")" << std::endl;
  } else {
    for (auto idx : selected) {
      seq.emplace_back(instance, idx);
    }
    std::cout << "  tour seq size " << seq.size() << std::endl;
  }
  auto node = std::make_unique<Node>(seq, &instance, &soc);
  std::cout << "Original root node has size " << seq.size() << std::endl;
  auto simplified_sequence = node->get_spanning_sequence();
  std::cout << "  simplified sequence:";
  for (auto &entry : simplified_sequence) {
    std::cout << " " << entry.geo_index();
  }
  std::cout << std::endl;

  return std::make_shared<Node>(simplified_sequence, &instance, &soc);
}
} // namespace tspn
