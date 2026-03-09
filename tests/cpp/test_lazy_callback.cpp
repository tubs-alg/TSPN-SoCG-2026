// Tests for lazy callback capability

#include <gtest/gtest.h>

#include "tspn/bnb.h"
#include "tspn/callbacks.h"
#include <boost/geometry.hpp>

namespace bg = boost::geometry;

namespace {

class LazyCB : public tspn::B2BNodeCallback {
public:
  LazyCB(std::vector<tspn::SiteVariant> polys) : polys{std::move(polys)} {}

  virtual void add_lazy_constraints(tspn::EventContext &e) {
    for (auto &p : polys) {
      if (e.get_relaxed_solution().get_trajectory().distance_site(p) > 0.001) {
        e.add_lazy_site(p);
        return;
      }
    }
  }

private:
  std::vector<tspn::SiteVariant> polys;
};

} // namespace

TEST(LazyCallback, BasicLazyConstraints) {
  std::vector<tspn::SiteVariant> polys;
  for (double x = 0; x <= 10; x += 2.0) {
    for (double y = 0; y <= 10; y += 2.0) {
      tspn::Polygon pol = tspn::Polygon{{{x - 0.5, y - 0.5},
                                         {x - 0.5, y + 0.5},
                                         {x + 0.5, y + 0.5},
                                         {x + 0.5, y - 0.5}}};
      bg::correct(pol);
      polys.push_back(pol);
    }
  }
  auto polygons = std::vector<tspn::SiteVariant>{
      tspn::Polygon{{{-0.5, -0.5}, {-0.5, 0.5}, {0.5, 0.5}, {0.5, -0.5}}},
      tspn::Polygon{{{9.5, -0.5}, {9.5, 0.5}, {10.5, 0.5}, {10.5, -0.5}}},
      tspn::Polygon{{{9.5, 9.5}, {9.5, 10.5}, {10.5, 10.5}, {10.5, 9.5}}},
      tspn::Polygon{{{-0.5, 9.5}, {-0.5, 10.5}, {0.5, 10.5}, {0.5, 9.5}}},
  };
  tspn::Instance instance(polygons);
  tspn::LongestEdgePlusFurthestSite root_node_strategy{};
  tspn::FarthestPoly branching_strategy{true, false};
  tspn::DfsBfs search_strategy;
  tspn::SocSolver soc(false);
  tspn::BranchAndBoundAlgorithm bnb(
      &instance, root_node_strategy.get_root_node(instance, soc),
      branching_strategy, search_strategy);
  bnb.add_node_callback(std::make_unique<LazyCB>(polys));
  bnb.optimize(60, 0.01);
  EXPECT_TRUE(bnb.get_solution() != nullptr);
  EXPECT_LE(bnb.get_upper_bound(), 48.0);
  EXPECT_GE(bnb.get_lower_bound(), 36.0);
}
