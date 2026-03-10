/**
 * This file implements branching strategies for the branch and bound algorithm,
 * i.e., deciding how to split the solution space. The primary decision here
 * is to decide for the poly to integrate next. However, we can also do
 * some filtering here and only create branches that are promising.
 *
 * The simplest branching strategy is to use always the farthest circle to
 * the current relaxed solution.
 *
 * If you want to improve the behaviour, the easiest way is probably to add
 * a rule, as for example done in ChFarhestCircle. A rule defines which
 * sequences are promising without actually evaluating them. If you need
 * to argue on the actual trajectory, use a callback directly added to the
 * branch and bound algorithm.
 *
 * 2023, Dominik Krupke, Tel Aviv
 *
 * modded by Rouven Kniep on 02.06.23
 */
#pragma once

#include "rule.h"
#include "tspn_core/common.h"
#include "tspn_core/details/solution_pool.h"
#include "tspn_core/node.h"
#include <CGAL/Convex_hull_traits_adapter_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/property_map.h>
#include <boost/thread.hpp>
#include <random>
#include <vector>

namespace tspn {
/**
 * The branching strategy decides how to split the solution space further.
 */
class BranchingStrategy {
public:
  /**
   * Allows you to setup some things for the rule at the beginning
   * of the branch and bound algorithm. For example change the mode
   * for tours or paths.
   * @param instance The instance to be solved.
   * @param root The root of the branch and bound tree.
   * @param solution_pool A pool of all solutions that will be found during BnB.
   */
  virtual void setup(Instance *instance, std::shared_ptr<Node> &root,
                     SolutionPool *solution_pool) {
    (void)instance;
    (void)root;
    (void)solution_pool;
  };
  /**
   * Branches the solution space of a node
   * @param node The node to be branched.
   * @return True iff the node has children.
   */
  virtual bool branch(Node &node) = 0;

  virtual ~BranchingStrategy() = default;
};

/**
 * The idea of Poly Branching is to add an still uncovered poly to the
 * sequence. This should be the most sensible branching strategy for most
 * cases, but it is not the only one. For example in case of intersections,
 * we know that the intersection has to be resolved and we could branch on
 * all ways to add a poly to the two edges in the .intersection.
 *
 * This is an abstract class and you need to specify how to choose the poly.
 * The most sensible approach should be to add the poly most distanced to
 * the current trajectory.
 */
class PolyBranching : public BranchingStrategy {
public:
  explicit PolyBranching(bool decomposition_branch, bool simplify,
                         unsigned num_threads_, bool verbose = true)
      : decomposition_branch{decomposition_branch}, simplify{simplify},
        num_threads{(num_threads_ == 0) ? boost::thread::hardware_concurrency()
                                        : num_threads_},
        verbose_{verbose} {
    if (verbose_) {
      std::cout << std::endl;
      if (simplify) {
        std::cout << "Using node simplification." << std::endl;
      }
      if (decomposition_branch) {
        std::cout << "Branching on polygon decompositions" << std::endl;
      } else {
        std::cout << "Indicator-modelling polygon decompositions" << std::endl;
      }
      std::cout << "Exploring on " << num_threads << " of "
                << boost::thread::hardware_concurrency() << " threads";
      if (num_threads_ == 0) {
        std::cout << " (auto)";
      }
      std::cout << std::endl;
    }
  }

  void setup(Instance *instance_, std::shared_ptr<Node> &root,
             SolutionPool *solution_pool) override {
    instance = instance_;
    for (auto &rule : rules) {
      rule->setup(instance, root, solution_pool);
    }
  }

  void add_rule(std::unique_ptr<SequenceRule> &&rule) {
    rules.push_back(std::move(rule));
  }

  bool branch(Node &node) override;

  unsigned get_num_discarded_sequences() const {
    return num_discarded_sequences;
  }

protected:
  /**
   * Override this method to filter the branching in advance.
   * @param sequence Sequence to be checked for a potential branch.
   * @return True if branch should be created.
   */
  virtual bool is_sequence_ok(const std::vector<TourElement> &sequence,
                              const Node &parent) {
    return std::all_of(rules.begin(), rules.end(),
                       [&sequence, &parent](auto &rule) {
                         return rule->is_ok(sequence, parent);
                       });
  }

  /**
   * Return the poly to branch on. This allows to easily create different
   * strategies.
   * @param node The node to be branched.
   * @return Index to the poly, or None if no option for branchinng.
   */
  virtual std::optional<TourElement> get_branching_poly(Node &node) = 0;

  Instance *instance = nullptr;
  bool decomposition_branch;
  bool simplify;
  unsigned num_threads;
  bool verbose_;
  std::vector<std::unique_ptr<SequenceRule>> rules;
  unsigned num_discarded_sequences = 0;
};

/**
 * This strategy tries to branch on the poly that is most distanced
 * to the relaxed solution.
 */
class FarthestPoly : public PolyBranching {
public:
  explicit FarthestPoly(bool decomposition_branch = true, bool simplify = false,
                        unsigned num_threads = 0, bool verbose = true)
      : PolyBranching{decomposition_branch, simplify, num_threads, verbose} {
    if (verbose_) {
      std::cout << "Branching on farthest poly." << std::endl;
    }
  }

protected:
  std::optional<TourElement> get_branching_poly(Node &node) override;
};

/**
 * Just a random branching strategy as comparison. It will branch on a random
 * not yet covered poly.
 */
class RandomPoly : public PolyBranching {
public:
  explicit RandomPoly(bool decomposition_branch = true, bool simplify = false,
                      unsigned num_threads = 0, bool verbose = true)
      : PolyBranching{decomposition_branch, simplify, num_threads, verbose} {
    if (verbose_) {
      std::cout << "Branching on random poly" << std::endl;
    }
  }

protected:
  std::optional<TourElement> get_branching_poly(Node &node) override;
};

} // namespace tspn
