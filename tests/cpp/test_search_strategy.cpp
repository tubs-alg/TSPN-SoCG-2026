// Tests for search strategies from tspn/strategies/search_strategy.h
//
// Verifies the actual exploration order contracts:
// - CheapestChildDepthFirst: DFS with cheapest child explored first
// - DfsBfs: DFS initially, switches to BFS (cheapest-first) after feasible
// - CheapestBreadthFirst: always picks globally cheapest LB next
// - RandomNextNode: shuffled order
// Also tests pruned-node skipping and full-solve correctness.

#include <gtest/gtest.h>

#include "tspn_core/bnb.h"
#include "tspn_core/strategies/branching_strategy.h"
#include "tspn_core/strategies/root_node_strategy.h"
#include "tspn_core/strategies/search_strategy.h"

namespace tspn {

// --- Helpers ---

// A 6-point instance where the root has 3 sites and 3 remain to branch on.
// Sites are spread enough that children have meaningfully different LBs.
struct StrategyTestFixture {
  std::vector<SiteVariant> sites;
  Instance instance;
  SocSolver soc;
  std::shared_ptr<Node> root;
  FarthestPoly branching;

  StrategyTestFixture()
      : sites{
            Point(0, 0),   Point(20, 0),  Point(40, 0),
            Point(10, 30), Point(30, 30), Point(20, 50),
        },
        instance(sites), soc(false),
        root(std::make_shared<Node>(
            std::vector<TourElement>{TourElement(instance, 0),
                                     TourElement(instance, 2),
                                     TourElement(instance, 5)},
            &instance, &soc)),
        branching(false) {
    branching.setup(&instance, root, nullptr);
  }
};

// =========================================================================
// CheapestChildDepthFirst: DFS that explores cheapest child first
// =========================================================================

TEST(SearchStrategy, CheapestChildDFExploresCheapestChildFirst) {
  // After branching, the first node popped should be the child with
  // the lowest LB (cheapest child). This is the defining property.
  StrategyTestFixture f;
  CheapestChildDepthFirst ss;
  ss.init(f.root);
  auto node = ss.next();
  ASSERT_TRUE(f.branching.branch(*node));
  ss.notify_of_branch(*node);

  auto &children = node->get_children();
  ASSERT_GE(children.size(), 2u);

  // Find the child with minimum LB
  double min_lb = std::numeric_limits<double>::max();
  for (auto &c : children) {
    min_lb = std::min(min_lb, c->get_lower_bound());
  }

  // First popped should be the cheapest
  auto first = ss.next();
  ASSERT_NE(first, nullptr);
  EXPECT_NEAR(first->get_lower_bound(), min_lb, 0.01)
      << "CheapestChildDepthFirst should pop the cheapest child first";
}

TEST(SearchStrategy, CheapestChildDFIsDFS) {
  // DFS property: after branching node A and getting children,
  // then branching a child B, B's children should be explored before
  // A's remaining children.
  StrategyTestFixture f;
  CheapestChildDepthFirst ss;
  ss.init(f.root);
  auto root_node = ss.next();
  ASSERT_TRUE(f.branching.branch(*root_node));
  ss.notify_of_branch(*root_node);

  // Get first child (cheapest) and branch it
  auto child = ss.next();
  ASSERT_NE(child, nullptr);
  int child_depth = child->depth();

  if (f.branching.branch(*child)) {
    ss.notify_of_branch(*child);
    // Next node should be a grandchild (depth = child_depth + 1),
    // NOT a sibling (depth = child_depth).
    auto next = ss.next();
    ASSERT_NE(next, nullptr);
    EXPECT_EQ(next->depth(), child_depth + 1)
        << "DFS: grandchildren should be explored before siblings";
  }
}

// =========================================================================
// DfsBfs: DFS before first feasible, BFS (cheapest-first) after
// =========================================================================

TEST(SearchStrategy, DfsBfsIsDFSBeforeFeasible) {
  // Before notify_of_feasible, DfsBfs behaves like DFS: children are
  // appended to back and popped from back (LIFO).
  StrategyTestFixture f;
  DfsBfs ss;
  ss.init(f.root);
  auto root_node = ss.next();
  ASSERT_TRUE(f.branching.branch(*root_node));
  ss.notify_of_branch(*root_node);

  auto child = ss.next();
  ASSERT_NE(child, nullptr);
  int child_depth = child->depth();

  if (f.branching.branch(*child)) {
    ss.notify_of_branch(*child);
    auto next = ss.next();
    ASSERT_NE(next, nullptr);
    // DFS: grandchild before sibling
    EXPECT_EQ(next->depth(), child_depth + 1)
        << "Before feasible, DfsBfs should be DFS (grandchild before sibling)";
  }
}

TEST(SearchStrategy, DfsBfsSwitchesToCheapestAfterFeasible) {
  // After notify_of_feasible, the queue is sorted by LB. The next
  // node popped should be the one with globally lowest LB.
  StrategyTestFixture f;
  DfsBfs ss;
  ss.init(f.root);
  auto root_node = ss.next();
  ASSERT_TRUE(f.branching.branch(*root_node));
  ss.notify_of_branch(*root_node);

  // Branch a child to create a deeper tree with varied LBs
  auto child1 = ss.next();
  ASSERT_NE(child1, nullptr);
  if (f.branching.branch(*child1)) {
    ss.notify_of_branch(*child1);
  }

  // Trigger BFS mode
  ss.notify_of_feasible(*child1);

  // After sort, popping should yield nodes in non-decreasing LB order
  double prev_lb = 0;
  int count = 0;
  while (ss.has_next() && count < 10) {
    auto n = ss.next();
    EXPECT_GE(n->get_lower_bound(), prev_lb - 0.01)
        << "After feasible, DfsBfs should pop in non-decreasing LB order";
    prev_lb = n->get_lower_bound();
    ++count;
  }
  EXPECT_GE(count, 2) << "Should have multiple nodes to verify ordering";
}

// =========================================================================
// CheapestBreadthFirst: always picks globally cheapest LB
// =========================================================================

TEST(SearchStrategy, CheapestBFAlwaysPicksCheapest) {
  // After branching, CheapestBreadthFirst sorts the entire queue.
  // Each popped node should have the lowest LB among remaining nodes.
  StrategyTestFixture f;
  CheapestBreadthFirst ss;
  ss.init(f.root);
  auto root_node = ss.next();
  ASSERT_TRUE(f.branching.branch(*root_node));
  ss.notify_of_branch(*root_node);

  // Pop all children and verify non-decreasing LB order
  double prev_lb = 0;
  int count = 0;
  while (ss.has_next()) {
    auto n = ss.next();
    EXPECT_GE(n->get_lower_bound(), prev_lb - 0.01)
        << "CheapestBreadthFirst should always pop cheapest node";
    prev_lb = n->get_lower_bound();
    ++count;
  }
  EXPECT_GE(count, 2) << "Should have multiple children to verify ordering";
}

TEST(SearchStrategy, CheapestBFIsBFS) {
  // BFS property: after branching root and child, a sibling (shallower)
  // with lower LB should be explored before a grandchild with higher LB.
  StrategyTestFixture f;
  CheapestBreadthFirst ss;
  ss.init(f.root);
  auto root_node = ss.next();
  ASSERT_TRUE(f.branching.branch(*root_node));
  ss.notify_of_branch(*root_node);

  // Branch the cheapest child
  auto child = ss.next();
  ASSERT_NE(child, nullptr);
  if (f.branching.branch(*child)) {
    ss.notify_of_branch(*child);

    // Now queue has: remaining siblings + grandchildren, sorted by LB.
    // Next should be the globally cheapest, regardless of depth.
    auto next1 = ss.next();
    auto next2 = ss.next();
    if (next1 && next2) {
      EXPECT_LE(next1->get_lower_bound(), next2->get_lower_bound() + 0.01)
          << "CheapestBreadthFirst: globally cheapest should come first";
    }
  }
}

// =========================================================================
// Pruning: all strategies must skip pruned nodes
// =========================================================================

TEST(SearchStrategy, AllStrategiesSkipPrunedNodes) {
  auto test_pruning = [](SearchStrategy &ss, const std::string &name) {
    std::vector<SiteVariant> sites = {
        Point(0, 0),   Point(20, 0),  Point(40, 0),
        Point(10, 30), Point(30, 30), Point(20, 50),
    };
    Instance instance(sites);
    SocSolver soc(false);
    auto root = std::make_shared<Node>(
        std::vector<TourElement>{TourElement(instance, 0),
                                 TourElement(instance, 2),
                                 TourElement(instance, 5)},
        &instance, &soc);
    FarthestPoly branching(false);
    branching.setup(&instance, root, nullptr);

    ss.init(root);
    auto node = ss.next();
    branching.branch(*node);
    ss.notify_of_branch(*node);

    // Prune all children
    for (auto &c : node->get_children()) {
      c->prune(false);
    }

    EXPECT_FALSE(ss.has_next())
        << name << ": should report empty after all children pruned";
  };

  CheapestChildDepthFirst s1;
  test_pruning(s1, "CheapestChildDepthFirst");
  CheapestBreadthFirst s2;
  test_pruning(s2, "CheapestBreadthFirst");
  RandomNextNode s3;
  test_pruning(s3, "RandomNextNode");
  DfsBfs s4;
  test_pruning(s4, "DfsBfs");
}

// =========================================================================
// All strategies solve to the same optimum on a small instance
// =========================================================================

TEST(SearchStrategy, AllStrategiesConvergeToSameOptimum) {
  // On a small instance, all search strategies paired with FarthestPoly
  // should find the same optimal tour length.
  std::vector<SiteVariant> sites = {
      Point(0, 0),
      Point(10, 0),
      Point(10, 10),
      Point(0, 10),
  };
  Instance instance(sites);
  SocSolver soc(false);

  auto solve_with = [&](SearchStrategy &ss) -> double {
    LongestEdgePlusFurthestSite rns;
    auto root = rns.get_root_node(instance, soc);
    FarthestPoly bs(false);
    BranchAndBoundAlgorithm bnb(&instance, root, bs, ss);
    bnb.optimize(30, 0.001, false);
    return bnb.get_upper_bound();
  };

  DfsBfs s1;
  double ub1 = solve_with(s1);
  CheapestChildDepthFirst s2;
  double ub2 = solve_with(s2);
  CheapestBreadthFirst s3;
  double ub3 = solve_with(s3);
  RandomNextNode s4;
  double ub4 = solve_with(s4);

  // All should find the same optimal tour (perimeter of a 10x10 square = 40)
  EXPECT_NEAR(ub1, 40.0, 0.5);
  EXPECT_NEAR(ub2, 40.0, 0.5);
  EXPECT_NEAR(ub3, 40.0, 0.5);
  EXPECT_NEAR(ub4, 40.0, 0.5);
}

// =========================================================================
// FarthestPoly: branches on the poly most distanced from relaxed solution
// =========================================================================

TEST(BranchingStrategy, FarthestPolyBranchesOnMostDistantPoly) {
  // Create a node where one uncovered poly is clearly farther from the
  // relaxed tour than another.
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // idx 0 - in root
      Point(10, 0),  // idx 1 - in root
      Point(5, 5),   // idx 2 - in root
      Point(5, 2),   // idx 3 - close to tour, should NOT be branched first
      Point(50, 50), // idx 4 - far from tour, should be branched first
  };
  Instance instance(sites);
  SocSolver soc(false);
  auto root =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2)},
                             &instance, &soc);
  FarthestPoly bs(false);
  bs.setup(&instance, root, nullptr);

  ASSERT_TRUE(bs.branch(*root));

  // Check that children include idx 4 in their sequences (branched on it)
  auto &children = root->get_children();
  bool all_have_idx4 = true;
  for (auto &child : children) {
    auto &seq = child->get_fixed_sequence();
    bool has_4 = false;
    for (auto &te : seq) {
      if (te.geo_index() == 4)
        has_4 = true;
    }
    if (!has_4)
      all_have_idx4 = false;
  }
  EXPECT_TRUE(all_have_idx4)
      << "FarthestPoly should branch on idx 4 (most distanced from tour), "
         "so all children should contain idx 4";
}

TEST(BranchingStrategy, FarthestPolyDoesNotBranchWhenAllCovered) {
  std::vector<SiteVariant> sites = {
      Point(0, 0),
      Point(10, 0),
      Point(5, 5),
  };
  Instance instance(sites);
  SocSolver soc(false);
  auto root =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2)},
                             &instance, &soc);
  FarthestPoly bs(false);
  bs.setup(&instance, root, nullptr);

  EXPECT_FALSE(bs.branch(*root))
      << "Should not branch when all polys are covered";
}

// =========================================================================
// RandomPoly: branches on an uncovered poly (any of them)
// =========================================================================

TEST(BranchingStrategy, RandomPolyBranchesOnUncoveredPoly) {
  std::vector<SiteVariant> sites = {
      Point(0, 0),   // idx 0 - in root
      Point(10, 0),  // idx 1 - in root
      Point(5, 5),   // idx 2 - in root
      Point(5, 2),   // idx 3 - uncovered
      Point(50, 50), // idx 4 - uncovered
  };
  Instance instance(sites);
  SocSolver soc(false);
  auto root =
      std::make_shared<Node>(std::vector<TourElement>{TourElement(instance, 0),
                                                      TourElement(instance, 1),
                                                      TourElement(instance, 2)},
                             &instance, &soc);
  RandomPoly bs(false);
  bs.setup(&instance, root, nullptr);

  ASSERT_TRUE(bs.branch(*root));

  // All children should contain the same newly branched poly (either 3 or 4)
  auto &children = root->get_children();
  ASSERT_FALSE(children.empty());
  // Find which new index was added
  std::set<unsigned> root_indices = {0, 1, 2};
  unsigned branched_idx = 999;
  for (auto &te : children[0]->get_fixed_sequence()) {
    if (root_indices.find(te.geo_index()) == root_indices.end()) {
      branched_idx = te.geo_index();
      break;
    }
  }
  EXPECT_TRUE(branched_idx == 3 || branched_idx == 4)
      << "RandomPoly should branch on one of the uncovered polys (3 or 4)";
}

} // namespace tspn
