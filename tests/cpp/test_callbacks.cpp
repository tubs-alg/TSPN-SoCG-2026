// Tests for BnB callback system (callbacks.h) and BnB upper/lower bound
// injection.
//
// The existing test_lazy_callback.cpp only tests add_lazy_site via
// add_lazy_constraints. This file covers:
// - on_entering_node with lower bound injection
// - Heuristic solution injection via add_solution
// - on_leaving_node for observation
// - Multiple chained callbacks
// - EventContext accessor correctness
// - add_upper_bound / add_lower_bound on BnB directly
// - set_ub_callback notification

#include <gtest/gtest.h>

#include "tspn_core/bnb.h"
#include "tspn_core/callbacks.h"

namespace {

// Helper: build a grid instance that requires some branching
tspn::Instance make_grid_instance(int n, double spacing, double size) {
  tspn::Instance instance;
  for (int ix = 0; ix < n; ++ix) {
    for (int iy = 0; iy < n; ++iy) {
      double x = ix * spacing;
      double y = iy * spacing;
      tspn::Polygon poly{{{x - size, y - size},
                          {x - size, y + size},
                          {x + size, y + size},
                          {x + size, y - size}}};
      bg::correct(poly);
      instance.add_site(poly);
    }
  }
  return instance;
}

// Helper: build a small 4-polygon instance
tspn::Instance make_small_instance() {
  auto polygons = std::vector<tspn::SiteVariant>{
      tspn::Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      tspn::Polygon{{{5, 0}, {6, 0}, {5, 1}}},
      tspn::Polygon{{{5, 5}, {6, 5}, {5, 6}}},
      tspn::Polygon{{{0, 5}, {1, 5}, {0, 6}}},
  };
  return tspn::Instance(polygons);
}

// ===================================================================
// Callback: on_entering_node with lower bound injection
// ===================================================================

class LowerBoundCallback : public tspn::B2BNodeCallback {
public:
  double injected_lb;
  int times_called = 0;

  LowerBoundCallback(double lb) : injected_lb(lb) {}

  void on_entering_node(tspn::EventContext &e) override {
    times_called++;
    e.add_lower_bound(injected_lb);
  }
};

TEST(Callbacks, OnEnteringNodeCalled) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<LowerBoundCallback>(0.0);
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(5, 0.001, false);
  EXPECT_GT(cb_ptr->times_called, 0);
}

TEST(Callbacks, LowerBoundInjectionTightens) {
  auto instance = make_grid_instance(3, 3.0, 0.5);
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  // Inject a known lower bound that's higher than 0
  auto cb = std::make_unique<LowerBoundCallback>(5.0);
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(5, 0.001, false);
  EXPECT_GE(bnb.get_lower_bound(), 5.0);
}

// ===================================================================
// Callback: on_leaving_node for observation
// ===================================================================

class ObservationCallback : public tspn::B2BNodeCallback {
public:
  int enter_count = 0;
  int leave_count = 0;
  int lazy_count = 0;
  std::vector<double> lower_bounds_seen;

  void on_entering_node(tspn::EventContext &e) override {
    enter_count++;
    lower_bounds_seen.push_back(e.get_lower_bound());
  }

  void add_lazy_constraints(tspn::EventContext &) override { lazy_count++; }

  void on_leaving_node(tspn::EventContext &) override { leave_count++; }
};

TEST(Callbacks, OnLeavingNodeCalledForEveryEntry) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<ObservationCallback>();
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(5, 0.001, false);
  // on_leaving_node is called exactly once per on_entering_node
  EXPECT_EQ(cb_ptr->enter_count, cb_ptr->leave_count);
  EXPECT_GT(cb_ptr->enter_count, 0);
}

TEST(Callbacks, ObservationSeesValidLowerBounds) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<ObservationCallback>();
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(5, 0.001, false);
  // All lower bounds seen should be non-negative
  for (double lb : cb_ptr->lower_bounds_seen) {
    EXPECT_GE(lb, 0.0);
  }
}

// ===================================================================
// Callback: injecting heuristic solutions
// ===================================================================

class HeuristicSolutionCallback : public tspn::B2BNodeCallback {
public:
  bool injected = false;

  void on_entering_node(tspn::EventContext &e) override {
    if (!injected && e.is_feasible()) {
      // The current node is already feasible, so we can grab its solution
      // and inject it. This tests the add_solution path.
      auto &relaxed = e.get_relaxed_solution();
      tspn::Solution sol(relaxed);
      e.add_solution(sol);
      injected = true;
    }
  }
};

TEST(Callbacks, InjectHeuristicSolution) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<HeuristicSolutionCallback>();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(10, 0.001, false);
  // Solution should have been found and injected
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_LT(bnb.get_upper_bound(), std::numeric_limits<double>::infinity());
}

// ===================================================================
// Callback: EventContext accessors
// ===================================================================

class ContextInspectionCallback : public tspn::B2BNodeCallback {
public:
  bool context_valid = true;
  bool saw_fixed_sequence = false;
  bool saw_relaxed_solution = false;
  int num_iterations_seen = 0;

  void on_entering_node(tspn::EventContext &e) override {
    // Verify context fields
    if (e.current_node == nullptr) {
      context_valid = false;
    }
    if (e.root_node == nullptr) {
      context_valid = false;
    }
    if (e.instance == nullptr) {
      context_valid = false;
    }
    if (e.solution_pool == nullptr) {
      context_valid = false;
    }
    if (e.num_iterations <= 0) {
      context_valid = false;
    }
    num_iterations_seen = e.num_iterations;

    // Check accessor methods
    const auto &seq = e.get_fixed_sequence();
    if (!seq.empty()) {
      saw_fixed_sequence = true;
    }

    const auto &relaxed = e.get_relaxed_solution();
    if (relaxed.get_trajectory().is_valid()) {
      saw_relaxed_solution = true;
    }

    // get_upper_bound and get_lower_bound should not throw
    double ub = e.get_upper_bound();
    double lb = e.get_lower_bound();
    (void)ub;
    (void)lb;
  }
};

TEST(Callbacks, EventContextFieldsValid) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<ContextInspectionCallback>();
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(5, 0.001, false);
  EXPECT_TRUE(cb_ptr->context_valid);
  EXPECT_TRUE(cb_ptr->saw_fixed_sequence);
  EXPECT_TRUE(cb_ptr->saw_relaxed_solution);
  EXPECT_GT(cb_ptr->num_iterations_seen, 0);
}

// ===================================================================
// Multiple chained callbacks
// ===================================================================

TEST(Callbacks, MultipleChaineCallbacks) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb1 = std::make_unique<ObservationCallback>();
  auto cb2 = std::make_unique<ObservationCallback>();
  auto *cb1_ptr = cb1.get();
  auto *cb2_ptr = cb2.get();
  bnb.add_node_callback(std::move(cb1));
  bnb.add_node_callback(std::move(cb2));
  bnb.optimize(5, 0.001, false);
  // Both should be called the same number of times
  EXPECT_EQ(cb1_ptr->enter_count, cb2_ptr->enter_count);
  EXPECT_EQ(cb1_ptr->leave_count, cb2_ptr->leave_count);
  EXPECT_GT(cb1_ptr->enter_count, 0);
}

// ===================================================================
// BnB: add_upper_bound directly
// ===================================================================

TEST(Callbacks, BnBAddUpperBoundDirectly) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  // Compute a feasible solution first by running briefly
  bnb.optimize(3, 0.001, false);
  auto sol = bnb.get_solution();
  if (sol) {
    double original_ub = bnb.get_upper_bound();
    // Re-adding the same solution shouldn't change the UB
    bnb.add_upper_bound(*sol);
    EXPECT_DOUBLE_EQ(bnb.get_upper_bound(), original_ub);
  }
}

// ===================================================================
// BnB: add_lower_bound directly
// ===================================================================

TEST(Callbacks, BnBAddLowerBoundDirectly) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  bnb.add_lower_bound(1.0);
  EXPECT_GE(bnb.get_lower_bound(), 1.0);
}

// ===================================================================
// BnB: set_ub_callback notification
// ===================================================================

TEST(Callbacks, UbCallbackNotified) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  std::vector<double> ub_notifications;
  bnb.set_ub_callback([&](double ub) { ub_notifications.push_back(ub); });
  bnb.optimize(10, 0.001, false);
  // If a solution was found, the UB callback should have been called
  if (bnb.get_solution() != nullptr) {
    EXPECT_FALSE(ub_notifications.empty());
    // Each notification should be a finite, positive value
    for (double ub : ub_notifications) {
      EXPECT_GT(ub, 0.0);
      EXPECT_LT(ub, std::numeric_limits<double>::infinity());
    }
  }
}

// ===================================================================
// Callback: lazy constraints that add polygons with different types
// ===================================================================

class LazyCircleCallback : public tspn::B2BNodeCallback {
public:
  tspn::Circle extra_circle;
  bool added = false;

  LazyCircleCallback(tspn::Circle c) : extra_circle(c) {}

  void add_lazy_constraints(tspn::EventContext &e) override {
    if (!added) {
      auto dist = e.get_relaxed_solution().get_trajectory().distance_site(
          tspn::SiteVariant(extra_circle));
      if (dist > 0.001) {
        e.add_lazy_site(extra_circle);
        added = true;
      }
    }
  }
};

TEST(Callbacks, LazyConstraintWithCircle) {
  auto polygons = std::vector<tspn::SiteVariant>{
      tspn::Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      tspn::Polygon{{{8, 0}, {9, 0}, {8, 1}}},
  };
  tspn::Instance instance(polygons);
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  // Add a circle that's off the direct path
  auto cb = std::make_unique<LazyCircleCallback>(
      tspn::Circle(tspn::Point(4, 5), 1.0));
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(30, 0.01, false);
  // The circle should have been added as a lazy constraint
  EXPECT_TRUE(cb_ptr->added);
  // Instance should now have 3 sites
  EXPECT_EQ(instance.size(), 3u);
  // Solution should cover all including the lazy circle
  auto sol = bnb.get_solution();
  EXPECT_TRUE(sol != nullptr);
}

class LazyPointCallback : public tspn::B2BNodeCallback {
public:
  tspn::Point extra_point;
  bool added = false;

  LazyPointCallback(tspn::Point p) : extra_point(p) {}

  void add_lazy_constraints(tspn::EventContext &e) override {
    if (!added) {
      auto dist = e.get_relaxed_solution().get_trajectory().distance_site(
          tspn::SiteVariant(extra_point));
      if (dist > 0.001) {
        e.add_lazy_site(extra_point);
        added = true;
      }
    }
  }
};

TEST(Callbacks, LazyConstraintWithPoint) {
  auto polygons = std::vector<tspn::SiteVariant>{
      tspn::Polygon{{{0, 0}, {1, 0}, {0, 1}}},
      tspn::Polygon{{{8, 0}, {9, 0}, {8, 1}}},
  };
  tspn::Instance instance(polygons);
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  // Add a point that's off the direct path
  auto cb = std::make_unique<LazyPointCallback>(tspn::Point(4, 5));
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(30, 0.01, false);
  EXPECT_TRUE(cb_ptr->added);
  EXPECT_EQ(instance.size(), 3u);
}

// ===================================================================
// Callback: get_best_solution via context
// ===================================================================

class BestSolutionChecker : public tspn::B2BNodeCallback {
public:
  bool saw_solution = false;
  double best_length = std::numeric_limits<double>::infinity();

  void on_leaving_node(tspn::EventContext &e) override {
    auto sol = e.get_best_solution();
    if (sol != nullptr) {
      saw_solution = true;
      double len = sol->get_trajectory().length();
      if (len < best_length) {
        best_length = len;
      }
    }
  }
};

TEST(Callbacks, GetBestSolutionViaContext) {
  auto instance = make_small_instance();
  tspn::LongestEdgePlusFurthestSite root_strategy{};
  tspn::FarthestPoly branching_strategy;
  tspn::CheapestChildDepthFirst search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(&instance,
                                    root_strategy.get_root_node(instance, soc),
                                    branching_strategy, search_strategy);

  auto cb = std::make_unique<BestSolutionChecker>();
  auto *cb_ptr = cb.get();
  bnb.add_node_callback(std::move(cb));
  bnb.optimize(10, 0.001, false);
  if (bnb.get_solution() != nullptr) {
    EXPECT_TRUE(cb_ptr->saw_solution);
    EXPECT_NEAR(cb_ptr->best_length, bnb.get_upper_bound(), 0.01);
  }
}

} // namespace
