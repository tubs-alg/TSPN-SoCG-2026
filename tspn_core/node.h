/**
 * Implements a node in the BnB tree.
 */
#ifndef TSPN_NODE_H
#define TSPN_NODE_H
#include "relaxed_solution.h"
#include "tspn_core/common.h"
#include "tspn_core/soc.h"
#include <memory>
#include <numeric>
#include <optional>

namespace tspn {
/**
 * Represent an intersection in a specific trajectory.
 * The intersection is between two edges of the polygons pol1,pol2; pol3,pol4
 * which have the exact coordinates pt1,pt2; pt3,pt4
 */
struct TrajectoryIntersection {
public:
  const Point pt1, pt2, pt3, pt4;
  const Geometry pol1, pol2, pol3, pol4;

  TrajectoryIntersection(const Point &pt1, const Point &pt2,
                         const Geometry &pol1, const Geometry &pol2,
                         const Point &pt3, const Point &pt4,
                         const Geometry &pol3, const Geometry &pol4)
      : pt1(pt1), pt2(pt2), pt3(pt3), pt4(pt4), pol1(pol1), pol2(pol2),
        pol3(pol3), pol4(pol4) {}
};

class Node {
public:
  Node(Node &node) = delete;

  Node(Node &&node) = default;

  Node(std::vector<TourElement> branch_sequence_,
       std::list<StashedTourElement> stash, Instance *instance, SocSolver *soc,
       Node *parent = nullptr)
      : _relaxed_solution(instance, soc, std::move(branch_sequence_),
                          std::move(stash)),
        parent{parent}, instance{instance}, soc{soc} {
    if (parent != nullptr) {
      _depth = parent->depth() + 1;
    }
  }
  Node(std::vector<TourElement> branch_sequence_, Instance *instance,
       SocSolver *soc, Node *parent = nullptr)
      : Node(branch_sequence_, {}, instance, soc, parent) {}

  std::shared_ptr<Node> make_child(std::vector<TourElement> branch_sequence_) {
    return std::make_shared<Node>(branch_sequence_, _relaxed_solution.stash,
                                  instance, soc, this);
  }

  void trigger_lazy_evaluation() {
    _relaxed_solution.trigger_lazy_computation(true);
    /*
    while we *could* prune the node away immediately, this would mess
    up the search strategy. not pruning upon finding CUTOFF nodes
    improves bound perf.

    if (!_relaxed_solution.get_trajectory().is_valid()) {
      prune();
    }
    */
  }

  void set_ub_cutoff(double cutoff) { _relaxed_solution.set_ub_cutoff(cutoff); }

  void add_lower_bound(double lb);

  auto get_lower_bound() -> double;

  bool is_feasible();

  void branch(std::vector<std::shared_ptr<Node>> &children_);

  [[nodiscard]] const std::vector<std::shared_ptr<Node>> &get_children() const {
    return children;
  }

  [[nodiscard]] std::vector<std::shared_ptr<Node>> &get_children() {
    return children;
  }

  size_t num_children() const { return children.size(); }
  [[nodiscard]] Node *get_parent() { return parent; }
  [[nodiscard]] const Node *get_parent() const { return parent; }

  auto get_relaxed_solution() -> PartialSequenceSolution &;

  /**
   * Will prune the node, i.e., mark it as not leading to an optimal
   * solution and thus stopping at it. Pruned nodes are allowed to be
   * deleted from memory.
   */
  void prune(bool infeasible = true);

  [[nodiscard]] const std::vector<TourElement> &get_fixed_sequence() const {
    return _relaxed_solution.get_sequence();
  }

  /**
   * The spanning sequence is a subset of the fixed sequence, but with
   * all indices belonging to circles that to not span/define the trajectroy
   * removed.
   * @return The orded list of indices of the circles spanning the current
   * trajectory.
   */
  [[nodiscard]] std::vector<TourElement> get_spanning_sequence() {
    std::vector<TourElement> spanning_sequence;
    unsigned n = _relaxed_solution.get_sequence().size();
    spanning_sequence.reserve(n);
    for (unsigned i = 0; i < n; ++i) {
      if (_relaxed_solution.is_sequence_index_spanning(i)) {
        spanning_sequence.push_back(_relaxed_solution.get_sequence()[i]);
      }
    }
    return spanning_sequence;
  }

  void simplify() { _relaxed_solution.simplify(); }

  [[nodiscard]] auto is_pruned() const -> bool { return pruned; }

  [[nodiscard]] Instance *get_instance() { return instance; }

  [[nodiscard]] SocSolver *get_soc() { return soc; }

  [[nodiscard]] int depth() const { return _depth; }

  std::vector<TrajectoryIntersection> get_intersections();

private:
  // Check if the children allow to improve the lower bound.
  void reevaluate_children();

  PartialSequenceSolution _relaxed_solution;
  std::optional<double> lazy_lower_bound_value{};
  std::vector<std::shared_ptr<Node>> children;
  Node *parent;

  int _depth = 0;
  bool pruned = false;
  Instance *instance;
  SocSolver *soc;
};

} // namespace tspn
#endif // TSPN_NODE_H
