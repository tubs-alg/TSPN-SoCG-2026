#pragma once

#include "doctest/doctest.h"
#include "tspn/common.h"
#include "tspn/node.h"
#include "tspn/soc.h"
#include "tspn/strategies/order_filtering.h"

namespace {

tspn::Polygon square(double x0, double y0, double size) {
  return tspn::Polygon{
      {{x0, y0}, {x0 + size, y0}, {x0 + size, y0 + size}, {x0, y0 + size}}};
}

std::vector<tspn::SiteVariant> make_sites() {
  return {square(0, 0, 1), square(2, 0, 1), square(4, 0, 1)};
}

TEST_CASE("OrderFiltering cyclic between check allows correct wrap") {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 10;
  instance[1].annotations.order_index = 16; // j
  instance[2].annotations.order_index = 2;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  // parent has i (0) then k (2); child inserts j (1) between them
  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 0),
                                            tspn::TourElement(instance, 2)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 1),
                                           tspn::TourElement(instance, 2)};

  CHECK(rule.is_ok(child_seq, parent) == true);
}

TEST_CASE("OrderFiltering rejects wrong cyclic order") {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 2;
  instance[1].annotations.order_index = 16; // j
  instance[2].annotations.order_index = 10;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 0),
                                            tspn::TourElement(instance, 2)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 1),
                                           tspn::TourElement(instance, 2)};

  CHECK(rule.is_ok(child_seq, parent) == false);
}

TEST_CASE("OrderFiltering allows ties with j") {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 5;
  instance[1].annotations.order_index = 5; // j, same as i
  instance[2].annotations.order_index = 10;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 0),
                                            tspn::TourElement(instance, 2)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 1),
                                           tspn::TourElement(instance, 2)};

  CHECK(rule.is_ok(child_seq, parent) == true); // tie should not prune
}

TEST_CASE("OrderFiltering skips pairs when j is not between i and k") {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;
  instance[1].annotations.order_index = 5; // j
  instance[2].annotations.order_index = 3;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  // Parent sequence has j at start; adding i and k after means j is not
  // between i and k for the only annotated triple, so no pruning.
  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 1)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 1),
                                           tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 2)};

  CHECK(rule.is_ok(child_seq, parent) == true);
}

TEST_CASE("OrderFiltering multiple annotated neighbors") {
  auto sites = make_sites();
  sites.push_back(square(6, 0, 1));
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;
  instance[1].annotations.order_index = 3; // j
  instance[2].annotations.order_index = 5;
  instance[3].annotations.order_index = 7;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 0),
                                            tspn::TourElement(instance, 2),
                                            tspn::TourElement(instance, 3)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{
      tspn::TourElement(instance, 0), tspn::TourElement(instance, 1),
      tspn::TourElement(instance, 2), tspn::TourElement(instance, 3)};

  CHECK(rule.is_ok(child_seq, parent) == true);

  // Violate cyclic order: j (order 3) placed between geo 2 (order 5) and
  // geo 3 (order 7). Cyclically, 3 is NOT between 5 and 7, so this must
  // be rejected.
  std::vector<tspn::TourElement> bad_seq{
      tspn::TourElement(instance, 0), tspn::TourElement(instance, 2),
      tspn::TourElement(instance, 1), tspn::TourElement(instance, 3)};
  tspn::Node bad_parent(parent_seq, &instance, &solver, root.get());
  CHECK(rule.is_ok(bad_seq, bad_parent) == false);
}
TEST_CASE("OrderFiltering ignores unannotated geometries") {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;
  // others unannotated

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 0)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 1)};
  CHECK(rule.is_ok(child_seq, parent) == true);
}

} // namespace
