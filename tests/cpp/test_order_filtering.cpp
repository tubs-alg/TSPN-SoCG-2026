// Tests for order filtering rules from tspn/strategies/order_filtering.h

#include <gtest/gtest.h>

#include "tspn_core/common.h"
#include "tspn_core/node.h"
#include "tspn_core/soc.h"
#include "tspn_core/strategies/order_filtering.h"

namespace {

tspn::Polygon square(double x0, double y0, double size) {
  return tspn::Polygon{
      {{x0, y0}, {x0 + size, y0}, {x0 + size, y0 + size}, {x0, y0 + size}}};
}

std::vector<tspn::SiteVariant> make_sites() {
  return {square(0, 0, 1), square(2, 0, 1), square(4, 0, 1)};
}

TEST(OrderFiltering, CyclicBetweenCheckAllowsCorrectWrap) {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 10;
  instance[1].annotations.order_index = 16;
  instance[2].annotations.order_index = 2;

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

  EXPECT_TRUE(rule.is_ok(child_seq, parent));
}

TEST(OrderFiltering, RejectsWrongCyclicOrder) {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 2;
  instance[1].annotations.order_index = 16;
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

  EXPECT_FALSE(rule.is_ok(child_seq, parent));
}

TEST(OrderFiltering, AllowsTies) {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 5;
  instance[1].annotations.order_index = 5;
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

  EXPECT_TRUE(rule.is_ok(child_seq, parent));
}

TEST(OrderFiltering, SkipsPairsWhenJNotBetween) {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;
  instance[1].annotations.order_index = 5;
  instance[2].annotations.order_index = 3;

  tspn::SocSolver solver(false);
  std::vector<tspn::TourElement> root_seq;
  auto root =
      std::make_shared<tspn::Node>(root_seq, &instance, &solver, nullptr);

  tspn::OrderFiltering rule;
  rule.setup(&instance, root, nullptr);

  std::vector<tspn::TourElement> parent_seq{tspn::TourElement(instance, 1)};
  tspn::Node parent(parent_seq, &instance, &solver, root.get());

  std::vector<tspn::TourElement> child_seq{tspn::TourElement(instance, 1),
                                           tspn::TourElement(instance, 0),
                                           tspn::TourElement(instance, 2)};

  EXPECT_TRUE(rule.is_ok(child_seq, parent));
}

TEST(OrderFiltering, MultipleAnnotatedNeighbors) {
  auto sites = make_sites();
  sites.push_back(square(6, 0, 1));
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;
  instance[1].annotations.order_index = 3;
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

  EXPECT_TRUE(rule.is_ok(child_seq, parent));

  std::vector<tspn::TourElement> bad_seq{
      tspn::TourElement(instance, 0), tspn::TourElement(instance, 2),
      tspn::TourElement(instance, 1), tspn::TourElement(instance, 3)};
  tspn::Node bad_parent(parent_seq, &instance, &solver, root.get());
  EXPECT_FALSE(rule.is_ok(bad_seq, bad_parent));
}

TEST(OrderFiltering, IgnoresUnannotatedGeometries) {
  auto sites = make_sites();
  tspn::Instance instance(sites, /*path=*/true);
  instance[0].annotations.order_index = 1;

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
  EXPECT_TRUE(rule.is_ok(child_seq, parent));
}

} // namespace
