#include "tspn/soc.h"
#include "tspn/utils/distance.h"
#include "tspn/utils/geometry.h"
#include <boost/geometry.hpp>
#include <format>
#include <gurobi_c++.h>
#include <iostream>
#include <variant>
#include <vector>

namespace bg = boost::geometry;

namespace tspn {

void model_site_indicator(GRBModel &model, const GRBVar &var_x,
                          const GRBVar &var_y, const SiteVariant &site,
                          GRBLinExpr &sum_chosen) {
  GRBVar part_chosen = model.addVar(0, 1, 0, GRB_BINARY);
  sum_chosen += part_chosen;

  if (const Ring *ring = std::get_if<Ring>(&site)) {
    // Polygons with Area have a specific closing point (repetition of 1st
    // point)-- can be achieved inplace with bg.correct(poly)
    // => POLYGON((1 0,0 0,0 1,1 0))
    auto it_origin = ring->begin();
    auto it_target = ring->begin() + 1;
    while (it_target < ring->end()) {
      double dx = it_target->get<0>() - it_origin->get<0>();
      double dy = it_target->get<1>() - it_origin->get<1>();
      model.addGenConstrIndicator(part_chosen, 1,
                                  dy * (var_x - it_origin->get<0>()) >=
                                      dx * (var_y - it_origin->get<1>()));
      it_origin++;
      it_target++;
    }
  } else if (const Point *point = std::get_if<Point>(&site)) {
    model.addGenConstrIndicator(part_chosen, 1, var_x == point->get<0>());
    model.addGenConstrIndicator(part_chosen, 1, var_y == point->get<1>());
  } else if (std::holds_alternative<Circle>(site)) {
    throw std::logic_error(
        "model_site_indicator does not implement the circle site variant.");
  } else {
    throw std::logic_error(
        "model_site_indicator does not implement this SiteVariant type.");
  }
}

void model_convex_site(GRBModel &model, const GRBVar &var_x,
                       const GRBVar &var_y, const SiteVariant &site) {
  if (const Ring *ring = std::get_if<Ring>(&site)) {
    // Polygons with Area have a specific closing point (repetition of 1st
    // point)-- can be achieved inplace with bg.correct(poly)
    // => POLYGON((1 0,0 0,0 1,1 0))
    auto it_origin = ring->begin();
    auto it_target = ring->begin() + 1;
    while (it_target < ring->end()) {
      double dx = it_target->get<0>() - it_origin->get<0>();
      double dy = it_target->get<1>() - it_origin->get<1>();
      model.addConstr(dy * (var_x - it_origin->get<0>()) >=
                      dx * (var_y - it_origin->get<1>()));
      it_origin++;
      it_target++;
    }

  } else if (const Point *point = std::get_if<Point>(&site)) {
    model.addConstr(var_x == point->get<0>());
    model.addConstr(var_y == point->get<1>());
  } else if (const Circle *circle = std::get_if<Circle>(&site)) {
    auto dx = circle->center().get<0>() - var_x;
    auto dy = circle->center().get<1>() - var_y;
    model.addQConstr(dx * dx + dy * dy <= circle->radius() * circle->radius());
  } else {
    throw std::logic_error(
        "model_convex_site does not implement this SiteVariant type.");
  }
}

void model_tour_element(GRBModel &model, const GRBVar &var_x,
                        const GRBVar &var_y, const TourElement &te) {
  model_convex_site(model, var_x, var_y, te.active_convex_region());
  if (te.is_exact()) {
    GRBLinExpr sum_chosen;
    for (const SiteVariant &site : te.geometry()->decomposition()) {
      model_site_indicator(model, var_x, var_y, site, sum_chosen);
    }
    model.addConstr(sum_chosen >= 1);
  }
}

SocSolver::SocSolver(bool use_cutoff) : use_cutoff{use_cutoff} {};

void SocSolver::update_cutoff(double new_cutoff) {
  if (use_cutoff && new_cutoff < cutoff_value) {
    cutoff_value = new_cutoff;
  }
}

void setup_vars(GRBModel &model, const std::vector<TourElement> &sequence,
                std::vector<GRBVar> &abs_x, std::vector<GRBVar> &abs_y,
                std::vector<GRBVar> &f, std::vector<GRBVar> &rel_x,
                std::vector<GRBVar> &rel_y) {
  const auto n = std::count_if(sequence.begin(), sequence.end(),
                               [](const auto &te) { return te.is_active(); });
  abs_x.resize(n);
  abs_y.resize(n);
  f.resize(n);
  rel_x.resize(n);
  rel_y.resize(n);

  unsigned i = 0;
  for (const auto &te : sequence) {
    if (te.is_active()) {
      const Box bbox = te.bbox();
      abs_x[i] = model.addVar(/*lb=*/bbox.min_corner().get<0>(),
                              /*ub=*/bbox.max_corner().get<0>(),
                              /*obj=*/0.0, /*type=*/GRB_CONTINUOUS,
                              /*name=*/std::format("abs_x[{}]", i));
      abs_y[i] = model.addVar(/*lb=*/bbox.min_corner().get<1>(),
                              /*ub=*/bbox.max_corner().get<1>(),
                              /*obj=*/0.0, /*type=*/GRB_CONTINUOUS,
                              /*name=*/std::format("abs_y[{}]", i));
      f[i] = model.addVar(/*lb=*/0, /*ub=*/GRB_INFINITY,
                          /*obj=*/0.0, /*type=*/GRB_CONTINUOUS,
                          /*name=*/std::format("f[{}]", i));
      rel_x[i] = model.addVar(/*lb=*/-GRB_INFINITY, /*ub=*/GRB_INFINITY,
                              /*obj=*/0.0, /*type=*/GRB_CONTINUOUS,
                              /*name=*/std::format("rel_x[{}]", i));
      rel_y[i] = model.addVar(/*lb=*/-GRB_INFINITY, /*ub=*/GRB_INFINITY,
                              /*obj=*/0.0, /*type=*/GRB_CONTINUOUS,
                              /*name=*/std::format("rel_y[{}]", i));
      model_tour_element(model, abs_x[i], abs_y[i], te);
      i++;
    } else {
      throw std::logic_error(
          "found disabled TE! a TE should not be able to be disabled anymore.");
    }
  }
}

GRBEnv makeenv() {
  // env setup is set up in this function to only set parameters on env
  // creation.
  GRBEnv env;
  env.set(GRB_IntParam_OutputFlag, 0);
  env.set(GRB_IntParam_Presolve, 0);
  env.set(GRB_IntParam_SimplexPricing, 3);
  env.set(GRB_IntParam_Threads, 1);
  std::cout << "Initialized GRB env" << std::endl;
  return env;
}

Trajectory
SocSolver::compute_trajectory(const std::vector<TourElement> &sequence,
                              bool path) const {
  auto res = compute_trajectory_with_information(sequence, path);
  return std::get<2>(res);
}

std::tuple<double, double, Trajectory>
SocSolver::compute_trajectory_with_information(
    const std::vector<TourElement> &sequence, const bool path) const {

  const auto n = std::count_if(sequence.begin(), sequence.end(),
                               [](const auto &te) { return te.is_active(); });
  std::vector<Point> points;
  bool within_cutoff = false;
  if (path) {
    points.reserve(n);
  } else {
    points.reserve(n + 1);
  }

  // setup env & model properties
  static GRBEnv env = makeenv();

  GRBModel model(&env);
  if (use_cutoff) {
    model.set(GRB_DoubleParam_Cutoff, cutoff_value);
  }
  std::vector<GRBVar> abs_x;
  std::vector<GRBVar> abs_y;
  std::vector<GRBVar> f;
  std::vector<GRBVar> rel_x;
  std::vector<GRBVar> rel_y;

  setup_vars(model, sequence, abs_x, abs_y, f, rel_x, rel_y);

  GRBLinExpr obj = 0;
  for (unsigned i = 0; i < n; i++) {
    obj += f[i];
  }
  model.setObjective(obj);

  // define distances
  for (unsigned i = 0; i < n; ++i) {
    if (path && i == 0) {
      model.addConstr(rel_x[i] == 0);
      model.addConstr(rel_y[i] == 0);
    } else {
      const auto prev_c = (i == 0 ? n - 1 : i - 1);
      assert(prev_c >= 0);
      model.addConstr(rel_x[i] == abs_x[prev_c] - abs_x[i]);
      model.addConstr(rel_y[i] == abs_y[prev_c] - abs_y[i]);
    }
  }
  for (unsigned i = 0; i < n; i++) {
    model.addQConstr(f[i] * f[i] >= rel_x[i] * rel_x[i] + rel_y[i] * rel_y[i]);
  }
  model.update();
  model.optimize();

  int status = model.get(GRB_IntAttr_Status);
  if (status == GRB_OPTIMAL || status == GRB_SUBOPTIMAL) {
    // if there is a solution, we can extract points.
    within_cutoff = true;
    for (unsigned i = 0; i < n; i++) {
      points.emplace_back(Point(abs_x[i].get(GRB_DoubleAttr_X),
                                abs_y[i].get(GRB_DoubleAttr_X)));
    }
    if (!path) {
      points.push_back(points[0]);
    }
    /* // Debug prints, left here for availability
    {
      std::cout << "Model objective:   " << model.get(GRB_DoubleAttr_ObjVal)
                << std::endl;
      Trajectory traj(points, within_cutoff);
      std::cout << "Trajectory length: " << traj.length() << std::endl;
      std::cout << "Traj. WKT: " << bg::wkt(traj.points) << std::endl;
    }
    */

  } else if (status == GRB_CUTOFF) {
    // if cutoff, LB proof might've been done before the first sol
    // => perhaps no solution, but that solution is irrelevant anyways, as we
    // can work with LB (which is worse than cutoff)
    within_cutoff = false;
  } else {
    throw std::logic_error(
        std::format("invalid model status: {}.  \n Check "
                    "https://www.gurobi.com/documentation/current/refman/"
                    "optimization_status_codes.html#sec:StatusCodes for info.",
                    status));
  }
  if (status == GRB_SUBOPTIMAL) {
    std::cout << "  found SUBOPTIMAL model status:" << std::endl;
    std::cout << "    ObjBound: " << model.get(GRB_DoubleAttr_ObjBound)
              << std::endl;
    std::cout << "    ObjValue: " << model.get(GRB_DoubleAttr_ObjVal)
              << std::endl;
  }
  return {model.get(GRB_DoubleAttr_ObjBound), model.get(GRB_DoubleAttr_ObjVal),
          Trajectory(points, within_cutoff)};
}
} // namespace tspn
