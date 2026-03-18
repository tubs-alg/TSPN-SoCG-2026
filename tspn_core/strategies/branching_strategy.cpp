//
// Created by Dominik Krupke on 21.12.22.
//
#include "tspn_core/strategies/branching_strategy.h"
#include <boost/atomic/atomic.hpp>
#include <boost/thread/thread.hpp>
// #include <execution>
namespace tspn {

std::optional<unsigned> seq_contains(const std::vector<TourElement> &seq,
                                     const TourElement &te) {
  for (unsigned i = 0; i < seq.size(); i++) {
    if (seq[i].geo_index() == te.geo_index()) {
      assert(seq[i].is_lazy() == true);
      assert(seq[i].is_exact() == false);
      assert(seq[i].geometry()->is_convex() == false);
      return {i};
    }
  }
  return {};
}
/**
 * Find the circle that is most distanced to the current (relaxed) solution.
 * This is a good circle to branch upon. If no circle is uncovered, it returns
 * nothing.
 * @param solution The relaxed solution.
 * @param instance The instance with the circles.
 * @return The index of the most distanced circle in the solution or nothing
 *          if all circles are included.
 */
std::optional<int>
get_index_of_most_distanced_poly(const PartialSequenceSolution &solution,
                                 const Instance &instance) {
  const int n = static_cast<int>(instance.size());
  auto &distances = solution.distances();
  auto max_dist = std::max_element(distances.begin(), distances.end());
  if (*max_dist < tspn::constants::FEASIBILITY_TOLERANCE) {
    return {};
  }
  const int c = static_cast<int>(std::distance(distances.begin(), max_dist));
  return {c};
}

void evaluate_child(std::shared_ptr<Node> &child, const bool simplify) {
  child->trigger_lazy_evaluation();
  if (simplify && !child->is_pruned()) {
    child->simplify();
  }
}

void distributed_child_evaluation(std::vector<std::shared_ptr<Node>> &children,
                                  const bool simplify,
                                  const unsigned num_threads) {
  // Parallelize the computation of the relaxed solutions for the children
  // using a simple modulo on the number of threads. This is fine as we write
  // on separate heap memory for all children.
  size_t con_threads = std::min((size_t)num_threads, children.size());
  if (con_threads == 1) { // Without threading overhead.
    for (unsigned i = 0; i < children.size(); i += 1) {
      evaluate_child(children[i], simplify);
    }
  } else {
    boost::thread_group tg;
    boost::atomic<unsigned> next_child(0);
    for (unsigned offset = 0; offset < con_threads; ++offset) {
      tg.create_thread([simplify, &next_child, &children]() {
        // instead of traditional stripmining, this is more dynamic => faster
        // for wide branching trees with large variance in child evaluation
        // times
        while (true) {
          unsigned i = next_child.fetch_add(1, boost::memory_order_relaxed);
          if (i >= children.size()) {
            break;
          }
          evaluate_child(children[i], simplify);
        }
      });
    }
    tg.join_all(); // Wait for all threads to finish
  }
}

bool PolyBranching::branch(Node &node) {
  const auto c = get_branching_poly(node);
  if (!c.has_value()) {
    return false;
  }
  const unsigned branching_geo_index = c.value().geo_index();
  std::vector<std::shared_ptr<Node>> children;
  std::vector<TourElement> seq = node.get_fixed_sequence();
  std::optional<unsigned> contains = seq_contains(seq, *c);
  auto &pss = node.get_relaxed_solution();
  if (contains.has_value()) { // poly lazy
    unsigned index = contains.value();
    TourElement &selected = seq[index];
    if (selected.is_lazy()) {
      // un-lazify a covered poly
      if (decomposition_branch == false) {
        // tell the modelling to model the decomposition
        const TourElement mdelement = seq[index].set_exact();
        seq[index] = mdelement;
        if (is_sequence_ok(seq, node)) {
          children.push_back(node.make_child(seq));
        } else {
          ++num_discarded_sequences;
        }
      } else {
        // branch on decomposition
        std::vector<TourElement> decomposition_elements = seq[index].branch();
        for (const TourElement candidate : decomposition_elements) {
          seq[index] = candidate;
          if (is_sequence_ok(seq, node)) {
            children.push_back(node.make_child(seq));
          } else {
            ++num_discarded_sequences;
          }
        }
      }
    }
  } else if (pss.is_stashed(branching_geo_index)) {
    StashedTourElement ste = *pss.get_from_stash(branching_geo_index);
    std::tuple<size_t, size_t> reinsert_positions =
        ste.get_reinsert_positions(seq);
    size_t earliest = std::get<0>(reinsert_positions);
    size_t latest = std::get<1>(reinsert_positions);
    if (earliest > latest) {
      std::cout << "WARNING! restoring stashed geometry " << branching_geo_index
                << ": impossible position between " << earliest << " and "
                << latest << std::endl;
    }
    // std::cout << "restoring stashed geometry " << branching_geo_index
    //           << " between " << earliest << " and " << latest << "\n";
    if (earliest <= latest) {
      TourElement reinserted = ste.tour_element;
      reinserted.enable();
      pss.remove_from_stash(branching_geo_index);
      seq.insert(seq.begin() + earliest, reinserted);
      if (is_sequence_ok(seq, node)) {
        children.push_back(node.make_child(seq));
      }
      while (++earliest <= latest) {
        // bubble one back
        std::iter_swap(seq.begin() + (earliest - 1), seq.begin() + earliest);
        // worst case it's now on latest => ok
        if (is_sequence_ok(seq, node)) {
          children.push_back(node.make_child(seq));
        }
      }
    }

  } else {
    // cover a new poly
    if (skip_convex_hull && !c.value().geometry()->is_convex()) {
      // skip convex hull approximation: add with exact indicator modeling
      seq.push_back(
          TourElement(c.value().geo_index(), instance, /*is_exact=*/true));
    } else {
      seq.push_back(c.value());
    }
    // in non-path-mode, sequence is a ring, so:
    //   "abc" => ["abdc", "adbc", "dabc" (="abcd")]
    // in path mode, we don't have a ring, and dont want stuff between origin
    // and target:
    //   "abc" => ["abdc", "adbc"]
    if (instance->is_tour()) {
      // for tours, this position is required.
      if (is_sequence_ok(seq, node)) {
        children.push_back(node.make_child(seq));
      } else {
        ++num_discarded_sequences;
      }
    }
    for (int i = seq.size() - 1; i >= 2; i--) {
      seq[i] = seq[i - 1];
      seq[i - 1] = *c;
      if (is_sequence_ok(seq, node)) {
        children.push_back(node.make_child(seq));
      } else {
        ++num_discarded_sequences;
      }
    }
  }
  distributed_child_evaluation(children, simplify, num_threads);

  /* remove children above the UB cutoff, as they have no trajectory. */
  // TODO: i (RK) suspect that this causes the UB cutoff bug, as these children
  // are not actually pruned => fewer sorting with DFSBFS strat => slower LB
  // improvement initially.
  children.erase(std::remove_if(children.begin(), children.end(),
                                [](std::shared_ptr<Node> child) {
                                  return child->is_pruned();
                                }),
                 children.end());

  node.branch(children);
  return true;
}

std::optional<TourElement> FarthestPoly::get_branching_poly(Node &node) {
  const auto c =
      get_index_of_most_distanced_poly(node.get_relaxed_solution(), *instance);
  if (c) {
    return {TourElement(*instance, *c)};
  } else {
    return {};
  }
}

std::optional<TourElement> RandomPoly::get_branching_poly(Node &node) {
  std::vector<int> uncovered_polys;
  for (unsigned i = 0; i < instance->size(); ++i) {
    if (!node.get_relaxed_solution().covers(i)) {
      uncovered_polys.push_back(i);
    }
  }
  if (uncovered_polys.empty()) {
    return {};
  } else {
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0,
                                                    uncovered_polys.size() - 1);
    int uncovered = uncovered_polys[distribution(generator)];
    return {TourElement(*instance, uncovered)};
  }
}
} // namespace tspn
