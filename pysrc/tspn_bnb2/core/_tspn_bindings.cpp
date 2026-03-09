/**
 * This file defines the python bindings.
 */
#include "tspn/bnb.h"
#include "tspn/common.h"
#include "tspn/details/pre_simplify.h"
#include "tspn/node.h"
#include "tspn/sampling/dp.h"
#include "tspn/sampling/voronoi.h"
#include "tspn/strategies/branching_strategy.h"
#include "tspn/strategies/order_filtering.h"
#include "tspn/strategies/root_node_strategy.h"
#include "tspn/strategies/search_strategy.h"
#include "tspn/types.h"
#include "tspn/utils/distance.h"
#include "tspn/utils/drawer.h"
#include "tspn/utils/geometry.h"
#include <boost/geometry.hpp>
#include <fmt/core.h>
#include <gurobi_c++.h>
#include <iostream>
#include <pybind11/functional.h>
#include <pybind11/operators.h> // to define operator overloading
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> // automatic conversion of vectors

namespace bg = boost::geometry;
namespace py = pybind11;
using namespace tspn;
using namespace tspn::details;

class PythonCallback : public B2BNodeCallback {
  /*
   * Allowing user-callbacks to influence/improve the branch
   * and bound algorithm.
   */
public:
  PythonCallback(std::function<void(EventContext)> *callback,
                 std::function<void(EventContext)> lazy_callback)
      : callback(callback), lazy_callback(std::move(lazy_callback)) {}

  void on_entering_node(EventContext &e) {
    if (callback == nullptr) {
      return;
    }
    EventContext e_ = e;
    (*callback)(e_);
  }

  void add_lazy_constraints(EventContext &e) {
    if (!lazy_callback) {
      return;
    }
    EventContext e_ = e;
    lazy_callback(e_);
  }

private:
  std::function<void(EventContext)> *callback;
  std::function<void(EventContext)> lazy_callback;
};

std::unique_ptr<Polygon> create_poly(std::vector<std::vector<Point>> &points) {
  auto poly = std::make_unique<Polygon>();
  auto outer = points.front();
  for (const auto &point : outer) {
    bg::append(poly->outer(), point);
  }
  points.erase(points.begin());
  poly->inners().resize(points.size());
  for (unsigned i = 0; i < points.size(); i++) {
    for (const auto &point : points[i]) {
      bg::append(poly->inners()[i], point);
    }
  }
  bg::correct(*poly);
  return poly;
}

std::unique_ptr<TourElement> create_tour_element(Instance &instance,
                                                 unsigned geo_index) {
  return std::make_unique<TourElement>(instance, geo_index);
}
/**
 * Explicit function for the binding  of calling the BnB algorithm.
 * @param instance Instance to be solved.
 * @param py_callback Callback function in Python.
 * @param initial_solution Initial solution to get started.
 * @param timelimit Timelimit in seconds for the BnB algorithm.
 * @return Best solution found within timelimit or nullptr.
 */
std::tuple<std::unique_ptr<Solution>, double,
           std::unordered_map<std::string, std::string>>
branch_and_bound(Instance instance,
                 std::function<void(EventContext)> *py_callback,
                 std::function<void(EventContext)> py_lazy_callback,
                 Solution *initial_solution, int timelimit,
                 std::string branching, std::string search, std::string root,
                 bool node_simplification, std::vector<std::string> rules,
                 bool use_cutoff, unsigned num_threads,
                 bool decomposition_branch, double eps) {

  std::unique_ptr<RootNodeStrategy> rns;
  { // root node strat
    if (root == "LongestEdgePlusFurthestSite") {
      rns = std::make_unique<LongestEdgePlusFurthestSite>();
    } else if (root == "RandomRoot") {
      rns = std::make_unique<RandomRoot>();
    } else if (root == "LongestTriple") {
      rns = std::make_unique<LongestTriple>();
    } else if (root == "LongestTripleWithPoint") {
      rns = std::make_unique<LongestTripleWithPoint>();
    } else if (root == "LongestPointTriple") {
      rns = std::make_unique<LongestPointTriple>();
    } else if (root == "LongestPair") {
      rns = std::make_unique<LongestPair>();
    } else if (root == "RandomPair") {
      rns = std::make_unique<RandomPair>();
    } else if (root == "OrderRoot") {
      rns = std::make_unique<OrderRootStrategy>();
    } else {
      throw std::invalid_argument("Invalid root node strategy");
    }
    std::cout << "[Python] Selected root strategy: " << root << std::endl;
  }

  std::unique_ptr<PolyBranching> branching_strategy;
  { // branching strat
    if (branching == "FarthestPoly") {
      branching_strategy = std::make_unique<FarthestPoly>(
          decomposition_branch, /*simplify*/ node_simplification, num_threads);
    } else if (branching == "Random") {
      branching_strategy = std::make_unique<RandomPoly>(
          decomposition_branch, /*simplify*/ node_simplification, num_threads);
    } else {
      throw std::invalid_argument("Invalid branching strategy.");
    }
  }

  std::unique_ptr<SearchStrategy> search_strategy;
  { // search strat
    std::cout << "[Python] Selected search strategy: " << search << std::endl;
    if (search == "DfsBfs") {
      search_strategy = std::make_unique<DfsBfs>();
    } else if (search == "CheapestChildDepthFirst") {
      search_strategy = std::make_unique<CheapestChildDepthFirst>();
    } else if (search == "CheapestBreadthFirst") {
      search_strategy = std::make_unique<CheapestBreadthFirst>();
    } else if (search == "Random") {
      search_strategy = std::make_unique<RandomNextNode>();
    } else {
      throw std::invalid_argument("Invalid search strategy.");
    }
  }

  std::unordered_set<std::string> rules_(rules.begin(), rules.end());
  { // rules
    for (const auto &rule_name : rules_) {
      if (rule_name == "None") {
        // branching_strategy->add_rule(std::make_unique<NoneRule>());
      } else if (rule_name == "OrderFiltering") {
        branching_strategy->add_rule(std::make_unique<OrderFiltering>());
      } else {
        throw std::invalid_argument("Invalid rule.");
      }
    }
    if (!rules_.empty()) {
      std::cout << "[Python] Enabled rules:";
      for (const auto &r : rules_) {
        std::cout << " " << r;
      }
      std::cout << std::endl;
    }
  }

  SocSolver soc(use_cutoff);
  BranchAndBoundAlgorithm baba(&instance, rns->get_root_node(instance, soc),
                               *branching_strategy, *search_strategy);
  std::function<void(double)> ub_cb =
      std::bind_front(&SocSolver::update_cutoff, &soc);
  baba.set_ub_callback(ub_cb);
  { // Callbacks
    if (py_callback || py_lazy_callback) {
      baba.add_node_callback(
          std::make_unique<PythonCallback>(py_callback,
                                           std::move(py_lazy_callback)));
    }
  }
  if (initial_solution != nullptr) {
    baba.add_upper_bound(*initial_solution);
  }
  baba.optimize(timelimit, eps);

  return {baba.get_solution(), baba.get_lower_bound(), baba.get_statistics()};
}

PYBIND11_MODULE(_tspn_bindings, m) {
  // Translate Gurobi C++ exceptions into informative Python RuntimeErrors
  // instead of the default "Caught an unknown exception!" message.
  py::register_exception_translator([](std::exception_ptr p) {
    try {
      if (p) std::rethrow_exception(p);
    } catch (const GRBException &e) {
      PyErr_SetString(
          PyExc_RuntimeError,
          fmt::format("Gurobi error (code {}): {}", e.getErrorCode(),
                      e.getMessage())
              .c_str());
    }
  });

  py::class_<CroppedVoronoiDiagram>(m, "CroppedVoronoiDiagram")
      .def(py::init<const Polygon &>())
      .def("insert",
           (void (CroppedVoronoiDiagram::*)(
               Point &, bool))&CroppedVoronoiDiagram::insert,
           py::arg("p"), py::arg("recompute") = true,
           "Insert a single point into the diagram.")
      .def(
          "insert",
          [](CroppedVoronoiDiagram &self, double x, double y,
             bool recompute = true) {
            Point p(x, y);
            self.insert(p, recompute);
          },
          py::arg("x"), py::arg("y"), py::arg("recompute") = true,
          "Insert a single point into the diagram.")
      .def("error_for_point", &CroppedVoronoiDiagram::error_for_point)
      .def("get_point",
           (Point (CroppedVoronoiDiagram::*)(unsigned int i) const) &
               CroppedVoronoiDiagram::get_point,
           py::arg("i"), "Get the point at index i in the sampling points.")
      .def_property_readonly("n_sampling_points",
                             &CroppedVoronoiDiagram::n_sampling_points)
      .def("arrangement_edges", &CroppedVoronoiDiagram::arrangement_edges)
      .def("recompute", &CroppedVoronoiDiagram::recompute);

  py::class_<tspn::dp::DPSolver>(m, "DPSolver",
                                 "Dynamic Programming Solver for TSPN")
      .def(py::init<const std::vector<CroppedVoronoiDiagram> &>())
      .def("compute_trajectory", &tspn::dp::DPSolver::compute_trajectory,
           py::arg("start_point"), py::arg("tour_elements"),
           "Compute a trajectory for the given tour elements starting at "
           "start_point.");

  py::class_<tspn::dp::Cost>(m, "DPCost", "DP cost")
      .def_readonly("previous_index", &tspn::dp::Cost::previous_index)
      .def_readonly("value", &tspn::dp::Cost::value)
      .def("__repr__", [](const tspn::dp::Cost &cost) {
        return fmt::format("DPCost(previous_index={}, value={})",
                           cost.previous_index, cost.value);
      });

  py::class_<tspn::dp::DPSolution>(m, "DPSolution",
                                   "DP Trajectory of a solution")
      .def(py::init<Trajectory &>())
      .def_readonly("trajectory", &tspn::dp::DPSolution::trajectory)
      .def_readonly("cost", &tspn::dp::DPSolution::cost)
      .def_readonly("backward_cost", &tspn::dp::DPSolution::backward_cost)
      .def_readonly("lower_bound", &tspn::dp::DPSolution::lower_bound);

  m.def("parse_site_wkt", &parse_site_wkt,
        "Parse wkt of a site and return SiteVariant.", py::arg("wkt"));

  py::class_<Point>(m, "Point", "Simple position")
      .def(py::init<double, double>())
      .def_property("x", &Point::get<0>, &Point::set<0>)
      .def_property("y", &Point::get<1>, &Point::set<1>)
      .def("__repr__", [](const Point &point) { return bg::to_wkt(point, 17); })
      .def("__sub__",
           [](const Point &a, const Point &b) {
             Point res(a);
             bg::subtract_point(res, b);
             return res;
           })
      .def("__add__",
           [](const Point &a, const Point &b) {
             Point res(a);
             bg::add_point(res, b);
             return res;
           })
      .def("bbox", [](const Point &self) { return tspn::utils::bbox(self); })
      .def("scale",
           [](const Point &a, double b) {
             Point res(a);
             res.set<0>(res.get<0>() * b);
             res.set<1>(res.get<1>() * b);
             return res;
           })
      .def("dist", [](const Point &a, const Point &b) {
        return tspn::utils::distance(a, b);
      });

  py::class_<Circle>(m, "Circle", "Circle with Point and Radius")
      .def(py::init<const Point &, double>())
      .def("center", &Circle::center)
      .def("radius", &Circle::radius)

      .def("__repr__",
           [](const Circle &circle) {
             return std::format("CIRCLE({}, {})",
                                bg::to_wkt(circle.center(), 17),
                                circle.radius());
           })
      .def("bbox", [](const Circle &self) { return tspn::utils::bbox(self); });

  py::class_<Ring>(m, "Ring", "Simple Closed Linestrign as pointlist")
      .def("within",
           [](const Ring &self, const Point &p) { return bg::within(p, self); })
      .def("area", [](const Ring &self) { return bg::area(self); })
      .def("__repr__", [](const Ring &self) { return bg::to_wkt(self, 17); })
      .def(
          "__iter__",
          [](const Ring &self) {
            return py::make_iterator(self.begin(), self.end());
          },
          py::keep_alive<0, 1>())
      .def("__len__", [](const Ring &self) { return self.size(); })
      .def("__getitem__",
           [](const Ring &self, unsigned i) { return self.at(i); })
      .def("as_poly",
           [](const Ring &self) {
             Polygon poly;
             for (const auto &p : self) {
               bg::append(poly.outer(), p);
             }
             return poly;
           })
      .def("bbox", [](const Ring &self) { return tspn::utils::bbox(self); });

  py::class_<Polygon>(m, "Polygon", "Polygon as outer ring + inner rings")
      .def(py::init(&create_poly))
      .def_static("from_wkt",
                  [](const std::string &s) {
                    Polygon p;
                    bg::read_wkt(s, p);
                    bg::correct(p);
                    return p;
                  })
      .def("outer", [](const Polygon &p) { return p.outer(); })
      .def("inners", [](const Polygon &p) { return p.inners(); })
      .def("within", [](const Polygon &self,
                        const Point &p) { return bg::within(p, self); })
      .def("__repr__", [](const Polygon &self) { return bg::to_wkt(self, 17); })
      .def("area", [](const Polygon &self) { return bg::area(self); })
      .def("bbox", [](const Polygon &self) { return tspn::utils::bbox(self); });

  py::class_<Box>(m, "Box", "a directed bounding box")
      .def("min_corner", static_cast<Point &(Box::*)()>(&Box::min_corner))
      .def("max_corner", static_cast<Point &(Box::*)()>(&Box::max_corner))
      .def("__repr__", [](const Box &self) { return bg::to_wkt(self, 17); })
      .def("bbox", [](const Box &self) { return self; });

  py::class_<GeometryAnnotations>(m, "GeometryAnnotations",
                                  "additional info about instance geometries")
      .def(py::init<>())
      .def_readwrite("order_index", &GeometryAnnotations::order_index)
      .def_readwrite("overlapping_order_geo_indices",
                     &GeometryAnnotations::overlapping_order_geo_indices);

  py::class_<Geometry>(m, "Geometry", "a single geometry to be covered")
      .def("geo_index", &Geometry::geo_index)
      .def("definition", &Geometry::definition)
      .def("is_convex", &Geometry::is_convex)
      .def("hull", &Geometry::convex_hull)
      .def_readwrite("annotations", &Geometry::annotations)
      .def("decomposition",
           static_cast<const std::vector<SiteVariant> &(Geometry::*)() const>(
               &Geometry::decomposition))
      .def("dist", [](const Geometry &self, const Geometry &other) {
        return tspn::utils::distance(self.definition(), other.definition());
      });

  py::class_<TourElement>(m, "TourElement", "Element in the Sequence Order")
      .def(py::init(&create_tour_element))
      .def("geo_index", &TourElement::geo_index)
      .def("ring", &TourElement::active_convex_region,
           py::return_value_policy::reference);

  py::class_<Trajectory>(m, "Trajectory", "Trajectory of a solution")
      .def(py::init<std::vector<Point>>())
      .def_static("from_wkt",
                  [](const std::string &s) {
                    Linestring points;
                    bg::read_wkt(s, points);
                    return Trajectory(points);
                  })
      .def("length", &Trajectory::length)
      .def("is_tour", &Trajectory::is_tour)
      .def("__repr__",
           [](const Trajectory &self) { return bg::to_wkt(self.points, 17); })
      .def("__len__", [](const Trajectory &self) { return self.points.size(); })
      .def("__getitem__",
           [](const Trajectory &self, unsigned i) { return self.points.at(i); })
      .def(
          "__iter__",
          [](const Trajectory &self) {
            return py::make_iterator(self.points.begin(), self.points.end());
          },
          py::keep_alive<0, 1>())
      .def("begin", [](const Trajectory &self) { return self.points.begin(); })
      .def("end", [](const Trajectory &self) { return self.points.end(); })
      .def("distance_site", &Trajectory::distance_site)
      .def("distance_geometry", &Trajectory::distance_geometry)
      .def("is_simple", &Trajectory::is_simple);

  py::class_<Node, std::shared_ptr<Node>>(m, "Node", "Node in the BnB-tree.")
      .def("depth", &Node::depth)
      .def("get_lower_bound", &Node::get_lower_bound)
      .def("get_relaxed_solution", &Node::get_relaxed_solution)
      .def("add_lower_bound", &Node::add_lower_bound)
      .def("prune", &Node::prune)
      .def("is_pruned", &Node::is_pruned)
      .def("is_feasible", &Node::is_feasible)
      .def("get_fixed_sequence", &Node::get_fixed_sequence,
           py::return_value_policy::reference)
      .def("get_spanning_sequence", &Node::get_spanning_sequence)
      .def("num_children", &Node::num_children);

  py::class_<SolutionPool>(m, "SolutionPool")
      .def("add_solution", &SolutionPool::add_solution)
      .def("get_upper_bound", &SolutionPool::get_upper_bound)
      .def("empty", &SolutionPool::empty)
      .def("get_best_solution", &SolutionPool::get_best_solution);

  py::class_<Instance>(m, "Instance", "TSPN Instance")
      .def(py::init<std::vector<SiteVariant> &, bool>())
      .def("is_path", &Instance::is_path)
      .def("is_tour", &Instance::is_tour)
      .def("__len__", [](const Instance &self) { return self.size(); })
      .def("__getitem__",
           [](Instance &self, unsigned geo_index) { return self[geo_index]; })
      .def("add_annotation",
           [](Instance &self, unsigned geo_index,
              GeometryAnnotations annotations) {
             self[geo_index].annotations = annotations;
           })
      .def("sites",
           [](const Instance &self) {
             std::vector<SiteVariant> sites;
             for (const auto &c : self) {
               sites.push_back(c.definition());
             }
             return sites;
           })
      .def("hull_instance", &Instance::hull_instance,
           py::return_value_policy::reference,
           "The convex hull relaxation instance of the instance");
  /**
   * This is the probably most interesting class for the bindings.
   * It allows you to manipulate the BnB-process via a callback.
   */
  py::class_<EventContext>(m, "EventContext",
                           "Allows you to extract information of the BnB "
                           "process and influence it.")
      .def_readonly("current_node", &EventContext::current_node,
                    py::return_value_policy::reference_internal,
                    "The node currently investigated.")
      .def_readonly("root_node", &EventContext::root_node,
                    py::return_value_policy::reference,
                    "The root node of the BnB tree.")
      .def_readonly("instance", &EventContext::instance,
                    py::return_value_policy::reference_internal,
                    "The instance solved by the BnB tree.")
      .def_readonly("num_iterations", &EventContext::num_iterations,
                    "Number of iterations/nodes visited in the BnB tree.")
      .def("add_lazy_site", &EventContext::add_lazy_site<SiteVariant>,
           "Add a Poly to the instance as kind of a lazy constraint. "
           "Must be "
           "deterministic.")
      .def("add_solution", &EventContext::add_solution,
           "Add a new solution, that may help terminating earlier.")
      .def("get_lower_bound", &EventContext::get_lower_bound,
           "Return the currently proven lower bound.")
      .def("add_lower_bound", &EventContext::add_lower_bound,
           "add a lower bound to the current node.")
      .def("get_upper_bound", &EventContext::get_upper_bound,
           "Return  the value of the currently best known solution (or "
           "infinity).")
      .def("is_feasible", &EventContext::is_feasible,
           "Return true if the current node is feasible.")
      .def("get_relaxed_solution", &EventContext::get_relaxed_solution,
           "Return the relaxed solution of the current node.")
      .def("get_fixed_sequence", &EventContext::get_fixed_sequence,
           "Return the relaxed solution of the current node.")
      .def("get_best_solution", &EventContext::get_best_solution,
           "Return the best known feasible solution.");
  py::class_<SocSolver>(m, "SocSolver", "soc solving properties")
      .def(py::init<bool>(), py::arg("use_cutoff") = true)
      .def("compute_trajectory", &SocSolver::compute_trajectory,
           py::arg("sequence"), py::arg("path") = false,
           "Compute a trajectory from a sequence of TourElements.");
  py::class_<PartialSequenceSolution>(m, "PartialSequenceSolution")
      .def("get_trajectory", &PartialSequenceSolution::get_trajectory);
  py::class_<Solution>(m, "Solution")
      .def("get_trajectory", &Solution::get_trajectory);

  // functions
  m.def(
      "compute_tour_from_sequence",
      [](std::vector<TourElement> &sequence, bool path, bool use_cutoff) {
        SocSolver solver(use_cutoff);
        return solver.compute_trajectory(sequence, path);
      },
      "Computes a neighbourhood tour based on a given Polygon "
      "sequence.");

  m.def(
      "branch_and_bound", &branch_and_bound,
      "Computes an optimal solution based on Branch and Bound.\n\n"
      "Args:\n"
      "    instance: The TSPN instance to solve.\n"
      "    callback: User-defined callback function for monitoring/influencing "
      "the BnB process. "
      "Receives EventContext objects during node exploration.\n"
      "    lazy_callback: Callback for adding lazy constraints when a node is "
      "feasible. "
      "Use context.add_lazy_site() to add sites. Must be deterministic.\n"
      "    initial_solution: Optional initial solution to warm-start the "
      "algorithm (default: None).\n"
      "    timelimit: Maximum runtime in seconds (default: 60).\n"
      "    branching: Branching strategy to use. Options: 'FarthestPoly', "
      "'Random' (default: 'FarthestPoly').\n"
      "    search: Search strategy for node selection. Options: 'DfsBfs', "
      "'CheapestChildDepthFirst', "
      "'CheapestBreadthFirst', 'Random' (default: 'DfsBfs').\n"
      "    root: Root node strategy. Options: 'LongestEdgePlusFurthestSite', "
      "'RandomRoot', "
      "'LongestTriple', 'LongestTripleWithPoint', 'LongestPointTriple', "
      "'LongestPair', 'RandomPair', 'OrderRoot' (default: "
      "'LongestEdgePlusFurthestSite').\n"
      "    node_simplification: Enable node simplification during branching "
      "(default: False).\n"
      "    rules: List of additional branching rules to apply (default: []).\n"
      "    use_cutoff: Use upper bound cutoff for pruning (default: True).\n"
      "    num_threads: Number of parallel threads to use (default: 16).\n"
      "    decomposition_branch: Enable decomposition-based branching "
      "(default: True).\n"
      "    eps: Epsilon tolerance for optimality gap (default: 0.001).\n\n"
      "Returns:\n"
      "    tuple: A tuple containing:\n"
      "        - solution: The best solution found (or None)\n"
      "        - lower_bound: The proven lower bound\n"
      "        - statistics: Dictionary with algorithm statistics",
      py::arg("instance"), py::arg("callback"),
      py::arg("lazy_callback") = std::function<void(EventContext)>{},
      py::arg("initial_solution") = nullptr, py::arg("timelimit") = 60,
      py::arg("branching") = "FarthestPoly", py::arg("search") = "DfsBfs",
      py::arg("root") = "LongestEdgePlusFurthestSite",
      py::arg("node_simplification") = false,
      py::arg("rules") = std::vector<std::string>{},
      py::arg("use_cutoff") = true, py::arg("num_threads") = 0,
      py::arg("decomposition_branch") = true, py::arg("eps") = 0.001);

  m.def("draw_geometries", &tspn::utils::draw_geometries,
        "draws instance and trajectory", py::arg("filename"),
        py::arg("instance"), py::arg("trajectory"));

  m.def("pre_simplify", &tspn::details::simplify,
        "simplifies the set of site geometries", py::arg("polys"));

  // Wrapper functions that return new vectors instead of modifying in-place
  m.def(
      "remove_holes",
      [](std::vector<tspn::SiteVariant> polys) {
        tspn::details::remove_holes(polys);
        return polys;
      },
      "removes unnecessary holes from polygon site geometries",
      py::arg("polys"));

  m.def(
      "convex_hull_fill",
      [](std::vector<tspn::SiteVariant> polys) {
        tspn::details::convex_hull_fill(polys);
        return polys;
      },
      "fills areas between polygon boundaries and their convex hull",
      py::arg("polys"));

  m.def(
      "remove_supersites",
      [](std::vector<tspn::SiteVariant> polys) {
        tspn::details::remove_supersites(polys);
        return polys;
      },
      "removes polygons that are contained within other polygons",
      py::arg("polys"));

  // Parameter management
  m.def("set_float_parameter", &tspn::constants::set_float_parameter,
        "Set a float algorithm parameter by name.\n\n"
        "Args:\n"
        "    name: Parameter name ('FEASIBILITY_TOLERANCE' or "
        "'SPANNING_TOLERANCE')\n"
        "    value: New value (must be positive)\n\n"
        "Raises:\n"
        "    ValueError: If parameter name is unknown or value is invalid",
        py::arg("name"), py::arg("value"));

  m.def("set_int_parameter", &tspn::constants::set_int_parameter,
        "Set an integer algorithm parameter by name.\n\n"
        "Args:\n"
        "    name: Parameter name ('MAX_TE_TOGGLE_COUNT' or "
        "'MAX_GEOM_TOGGLE_COUNT')\n"
        "    value: New value (non-negative integer)\n\n"
        "Raises:\n"
        "    ValueError: If parameter name is unknown",
        py::arg("name"), py::arg("value"));

  m.def("get_float_parameter", &tspn::constants::get_float_parameter,
        "Get a float algorithm parameter by name.\n\n"
        "Args:\n"
        "    name: Parameter name ('FEASIBILITY_TOLERANCE' or "
        "'SPANNING_TOLERANCE')\n\n"
        "Returns:\n"
        "    Current parameter value\n\n"
        "Raises:\n"
        "    ValueError: If parameter name is unknown",
        py::arg("name"));

  m.def("get_int_parameter", &tspn::constants::get_int_parameter,
        "Get an integer algorithm parameter by name.\n\n"
        "Args:\n"
        "    name: Parameter name ('MAX_TE_TOGGLE_COUNT' or "
        "'MAX_GEOM_TOGGLE_COUNT')\n\n"
        "Returns:\n"
        "    Current parameter value\n\n"
        "Raises:\n"
        "    ValueError: If parameter name is unknown",
        py::arg("name"));

  m.def("reset_parameters", &tspn::constants::reset_parameters,
        "Reset all algorithm parameters to their default values.\n\n"
        "Default values:\n"
        "    FEASIBILITY_TOLERANCE = 0.001\n"
        "    SPANNING_TOLERANCE = 0.0009\n"
        "    MAX_TE_TOGGLE_COUNT = 1\n"
        "    MAX_GEOM_TOGGLE_COUNT = 1");
}
