// Tests for Node class from tspn_core/node.h
//
// Covers: construction, lower bounds, pruning, branching, depth tracking,
// parent–child relationships, and child reevaluation.

#include <gtest/gtest.h>

#include "tspn_core/node.h"

namespace tspn {

// --- Helpers ----------------------------------------------------------------

// Simple triangle polygon at a given position.
static Polygon triangle(double x, double y) {
  return Polygon{{{x, y}, {x + 1, y}, {x, y + 1}}};
}

// Build a small tour instance with well-separated polygons so that
// the SOCP relaxation yields a non-trivial, predictable tour.
static std::vector<SiteVariant> make_triangle_row(unsigned n,
                                                  double spacing = 3.0) {
  std::vector<SiteVariant> sites;
  sites.reserve(n);
  for (unsigned i = 0; i < n; ++i) {
    sites.push_back(triangle(i * spacing, 0));
  }
  return sites;
}

// Shorthand for building a full sequence of TourElements for an instance.
static std::vector<TourElement> full_sequence(Instance &instance) {
  std::vector<TourElement> seq;
  seq.reserve(instance.size());
  for (unsigned i = 0; i < instance.size(); ++i) {
    seq.push_back(TourElement(instance, i));
  }
  return seq;
}

// --- Tests ------------------------------------------------------------------

// The original basic test: two triangles produce a feasible tour.
TEST(Node, BasicTwoPolygonTour) {
  // Two triangles separated by 1 unit vertically — tour length is 2.0.
  auto sites = std::vector<SiteVariant>{
      Polygon{{{1, 0}, {0, 0}, {0, 1}}},
      Polygon{{{1, 2}, {0, 2}, {0, 3}}},
  };
  Instance instance(sites);
  EXPECT_TRUE(instance.is_tour());
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_NEAR(node.get_lower_bound(), 2.0, 1e-6);
  EXPECT_TRUE(node.is_feasible());
}

// Root node has depth 0, no parent.
TEST(Node, RootNodeDepthAndParent) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node root(full_sequence(instance), &instance, &soc);

  EXPECT_EQ(root.depth(), 0);
  EXPECT_EQ(root.get_parent(), nullptr);
}

// A newly constructed node is not pruned.
TEST(Node, InitiallyNotPruned) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_FALSE(node.is_pruned());
}

// Pruning a node marks it as pruned.
TEST(Node, PruneMarksPruned) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  node.prune();
  EXPECT_TRUE(node.is_pruned());
}

// Pruning with infeasible=true sets the lower bound to infinity.
TEST(Node, PruneInfeasibleSetsInfinityBound) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  // Force evaluation of lower bound first.
  double lb_before = node.get_lower_bound();
  EXPECT_TRUE(std::isfinite(lb_before));

  node.prune(/*infeasible=*/true);
  EXPECT_EQ(node.get_lower_bound(), std::numeric_limits<double>::infinity());
}

// Pruning is idempotent: calling prune twice doesn't crash.
TEST(Node, PruneIsIdempotent) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  node.prune();
  node.prune(); // Should not crash or change state.
  EXPECT_TRUE(node.is_pruned());
}

// add_lower_bound only increases the bound, never decreases it.
TEST(Node, AddLowerBoundOnlyIncreases) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  double original = node.get_lower_bound();

  // Trying to set a lower value should have no effect.
  node.add_lower_bound(original - 1.0);
  EXPECT_NEAR(node.get_lower_bound(), original, 1e-9);

  // Setting a higher value should update the bound.
  node.add_lower_bound(original + 5.0);
  EXPECT_NEAR(node.get_lower_bound(), original + 5.0, 1e-9);
}

// make_child produces a child with correct depth and parent pointer.
TEST(Node, MakeChildDepthAndParent) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);

  // Root with partial sequence (not feasible, so branching makes sense).
  std::vector<TourElement> root_seq{TourElement(instance, 0),
                                    TourElement(instance, 2)};
  Node root(root_seq, &instance, &soc);
  EXPECT_EQ(root.depth(), 0);

  auto child = root.make_child(full_sequence(instance));
  EXPECT_EQ(child->depth(), 1);
  EXPECT_EQ(child->get_parent(), &root);
}

// get_instance and get_soc return the pointers passed at construction.
TEST(Node, AccessorsReturnConstructionPointers) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_EQ(node.get_instance(), &instance);
  EXPECT_EQ(node.get_soc(), &soc);
}

// A node with no children reports num_children() == 0.
TEST(Node, NoChildrenInitially) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_EQ(node.num_children(), 0u);
  EXPECT_TRUE(node.get_children().empty());
}

// get_fixed_sequence returns the sequence passed at construction.
TEST(Node, FixedSequenceMatchesConstruction) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);
  auto seq = full_sequence(instance);
  Node node(seq, &instance, &soc);

  const auto &fixed = node.get_fixed_sequence();
  ASSERT_EQ(fixed.size(), 3u);
  for (unsigned i = 0; i < 3; ++i) {
    EXPECT_EQ(fixed[i].geo_index(), i);
  }
}

// Three well-separated polygons should produce a feasible tour.
TEST(Node, ThreePolygonFeasibility) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_TRUE(node.is_feasible());
  EXPECT_TRUE(std::isfinite(node.get_lower_bound()));
  EXPECT_GT(node.get_lower_bound(), 0.0);
}

// Lower bound of a child is at least the parent's lower bound.
TEST(Node, ChildLowerBoundAtLeastParent) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);

  // Root with two sites — partial, so the relaxation is weaker.
  std::vector<TourElement> root_seq{TourElement(instance, 0),
                                    TourElement(instance, 2)};
  Node root(root_seq, &instance, &soc);
  double parent_lb = root.get_lower_bound();

  // Child with all three sites — tighter relaxation.
  auto child = root.make_child(full_sequence(instance));
  double child_lb = child->get_lower_bound();

  EXPECT_GE(child_lb, parent_lb - 1e-6);
}

// Pruning a parent cascades to its children.
TEST(Node, PruneCascadesToChildren) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);

  std::vector<TourElement> root_seq{TourElement(instance, 0),
                                    TourElement(instance, 2)};
  auto root = std::make_shared<Node>(root_seq, &instance, &soc);

  auto child = root->make_child(full_sequence(instance));

  // branch() requires the node to not be feasible — use the partial root.
  if (!root->is_feasible()) {
    std::vector<std::shared_ptr<Node>> children_vec{child};
    root->branch(children_vec);

    EXPECT_FALSE(child->is_pruned());
    root->prune();
    EXPECT_TRUE(root->is_pruned());
    EXPECT_TRUE(child->is_pruned());
  }
}

// Path instance: a node with all sites in a path should be feasible.
TEST(Node, PathInstanceFeasible) {
  auto sites = make_triangle_row(3);
  Instance instance(sites, /*path=*/true);
  EXPECT_TRUE(instance.is_path());
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_TRUE(node.is_feasible());
  EXPECT_GT(node.get_lower_bound(), 0.0);
}

// Branching with empty children prunes the node.
TEST(Node, BranchWithEmptyChildrenPrunes) {
  auto sites = make_triangle_row(3);
  Instance instance(sites);
  SocSolver soc(false);

  // Partial sequence — likely not feasible.
  std::vector<TourElement> partial{TourElement(instance, 0),
                                   TourElement(instance, 2)};
  Node node(partial, &instance, &soc);

  if (!node.is_feasible()) {
    std::vector<std::shared_ptr<Node>> empty_children;
    node.branch(empty_children);
    EXPECT_TRUE(node.is_pruned());
    EXPECT_EQ(node.num_children(), 0u);
  }
}

// Branching on a pruned node throws.
TEST(Node, BranchOnPrunedThrows) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);

  // Use a partial sequence so the node is not feasible.
  std::vector<TourElement> partial{TourElement(instance, 0)};
  Node node(partial, &instance, &soc);
  node.prune();

  std::vector<std::shared_ptr<Node>> children;
  EXPECT_THROW(node.branch(children), std::invalid_argument);
}

// set_ub_cutoff doesn't crash and can be called before evaluation.
TEST(Node, SetUbCutoffDoesNotCrash) {
  auto sites = make_triangle_row(2);
  Instance instance(sites);
  SocSolver soc(false);
  Node node(full_sequence(instance), &instance, &soc);

  EXPECT_NO_THROW(node.set_ub_cutoff(100.0));
}

// Grandchild has depth 2 and correct parent chain.
TEST(Node, GrandchildDepthAndParentChain) {
  auto sites = make_triangle_row(4, 4.0);
  Instance instance(sites);
  SocSolver soc(false);

  // Root with 2 of 4 sites.
  std::vector<TourElement> root_seq{TourElement(instance, 0),
                                    TourElement(instance, 3)};
  Node root(root_seq, &instance, &soc);

  // Child with 3 of 4 sites.
  std::vector<TourElement> child_seq{TourElement(instance, 0),
                                     TourElement(instance, 1),
                                     TourElement(instance, 3)};
  auto child = root.make_child(child_seq);
  EXPECT_EQ(child->depth(), 1);
  EXPECT_EQ(child->get_parent(), &root);

  // Grandchild with all 4 sites.
  auto grandchild = child->make_child(full_sequence(instance));
  EXPECT_EQ(grandchild->depth(), 2);
  EXPECT_EQ(grandchild->get_parent(), child.get());
  EXPECT_EQ(grandchild->get_parent()->get_parent(), &root);
}

} // namespace tspn
