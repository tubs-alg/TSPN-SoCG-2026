//
// Provides some common classes on instances and solutions that are
// independent of the concrete algorithms.
//
#pragma once

#include "tspn_core/details/convex_decomposition.h"
#include "tspn_core/details/convex_partition.h"
#include "tspn_core/types.h"
#include <any>
#include <atomic>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

// added
#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/is_convex.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/segment.hpp>

namespace bg = boost::geometry;

namespace tspn {

namespace constants {
/// Tolerance for checking if a trajectory covers a geometry (in distance
/// units). A geometry is considered covered if the trajectory comes within this
/// distance. Used in feasibility checks to determine if a solution is valid.
/// NOTE: Parameters should only be changed between algorithm runs, not during
/// execution.
extern double FEASIBILITY_TOLERANCE;

/// Tolerance for determining if a trajectory point is "spanning" (i.e., a
/// turning point). A point is considered spanning if removing it would change
/// the trajectory by at least this distance. Points below this threshold can be
/// simplified away. Set slightly lower than FEASIBILITY_TOLERANCE to avoid
/// numerical issues. NOTE: Parameters should only be changed between algorithm
/// runs, not during execution.
extern double SPANNING_TOLERANCE;

/// Maximum number of times a TourElement can be toggled (enabled/disabled)
/// during node simplification and reactivation. Prevents infinite toggling
/// loops in the branch-and-bound algorithm. Set to 1 by default (one disable
/// allowed). Can be increased to allow more aggressive simplification, but may
/// increase runtime. NOTE: Parameters should only be changed between algorithm
/// runs, not during execution.
extern unsigned MAX_TE_TOGGLE_COUNT;

/// Maximum number of times a geometry can have its TourElements toggled across
/// all its decomposition elements. Works in conjunction with
/// MAX_TE_TOGGLE_COUNT to prevent pathological toggling behavior during
/// simplification/reactivation. NOTE: Parameters should only be changed between
/// algorithm runs, not during execution.
extern unsigned MAX_GEOM_TOGGLE_COUNT;

// Parameter management functions

/// Set FEASIBILITY_TOLERANCE parameter. Must be positive.
void set_feasibility_tolerance(double value);

/// Set SPANNING_TOLERANCE parameter. Must be positive.
void set_spanning_tolerance(double value);

/// Set MAX_TE_TOGGLE_COUNT parameter.
void set_max_te_toggle_count(unsigned value);

/// Set MAX_GEOM_TOGGLE_COUNT parameter.
void set_max_geom_toggle_count(unsigned value);

/// Reset all parameters to their default values.
void reset_parameters();

/// Generic setter for float parameters by name. Throws std::invalid_argument if
/// name is unknown.
void set_float_parameter(const std::string &name, double value);

/// Generic setter for int parameters by name. Throws std::invalid_argument if
/// name is unknown.
void set_int_parameter(const std::string &name, unsigned value);

/// Generic getter for float parameters by name. Throws std::invalid_argument if
/// name is unknown.
double get_float_parameter(const std::string &name);

/// Generic getter for int parameters by name. Throws std::invalid_argument if
/// name is unknown.
unsigned get_int_parameter(const std::string &name);

} // namespace constants

class GeometryAnnotations {
  // should hold additional info about the geometry passed in.
public:
  GeometryAnnotations() {};
  std::optional<unsigned> order_index;
  std::vector<unsigned> overlapping_order_geo_indices;
};

class Geometry {
public:
  explicit Geometry(unsigned geo_index, const SiteVariant &definition);
  GeometryAnnotations annotations;

private:
  bool _is_convex;
  bool _has_holes;
  SiteVariant _definition;
  unsigned _geo_index;
  SiteVariant _convex_hull;
  std::vector<SiteVariant> _decomposition;

public:
  bool is_convex() const { return _is_convex; };
  bool has_holes() const { return _has_holes; };
  const SiteVariant &definition() const { return _definition; };
  unsigned geo_index() const { return _geo_index; };
  SiteVariant convex_hull() const { return _convex_hull; };
  const SiteVariant &convex_hull_ref() const { return _convex_hull; };
  const std::vector<SiteVariant> &decomposition() const {
    return _decomposition;
  };
  const SiteVariant &decomposition(unsigned pos) const {
    return _decomposition.at(pos);
  };

  // note: multithreaded use **will** run into sync issues - not relevant for
  // correctness tho
  mutable unsigned geom_toggle_count;
};

class Instance : public std::vector<Geometry> {
public:
  Instance();
  explicit Instance(std::vector<SiteVariant> &sites, bool path = false);

  [[nodiscard]] bool is_path() const;

  [[nodiscard]] bool is_tour() const;

  void add_site(SiteVariant &site);
  void add_site(Circle &site);
  void add_site(Polygon &site);
  void add_site(Ring &site);
  void add_site(Point &site);
  Instance hull_instance() const;

  bool path; // path instance = first two sites are origin and target.
  unsigned revision = 0; // revision = size, as remove is not possible.
  double eps = 0.01;
};

class TourElement {
public:
  explicit TourElement(const Instance &instance, unsigned geo_index);
  explicit TourElement(unsigned geo_index, const Instance *instance,
                       bool is_exact);
  explicit TourElement(unsigned geo_index, unsigned decomposition_index,
                       const Instance *instance, const Box &bbox, bool is_lazy,
                       bool is_exact);
  bool listorder(const TourElement &rhs) const;
  bool operator==(const TourElement &rhs) const;
  bool operator!=(const TourElement &rhs) const;
  const SiteVariant &active_convex_region() const;
  const std::vector<TourElement> branch() const;
  const TourElement set_exact() const;
  const SiteVariant &modeled_site() const;
  void enable();
  void disable();
  bool can_be_disabled() const;
  static std::atomic<size_t> enable_count;
  static std::atomic<size_t> disable_count;
  static void reset_counters() {
    enable_count = 0;
    disable_count = 0;
  }

private:
  unsigned _geo_index;
  unsigned _decomposition_index;
  const Instance *_instance;
  Box _bbox;
  bool _is_lazy;
  bool _is_exact;
  bool _is_active;
  mutable unsigned te_toggle_count;

public:
  unsigned geo_index() const { return _geo_index; }
  unsigned decomposition_index() const { return _decomposition_index; }
  const Geometry *geometry() const { return &((*_instance)[_geo_index]); };
  Box bbox() const { return _bbox; };
  bool is_lazy() const { return _is_lazy; };
  bool is_exact() const { return _is_exact; };
  bool is_active() const { return _is_active; };
  bool can_bypass_distance() const { return _is_active & !_is_lazy; };
};

class Trajectory {
  /**
   * For representing the trajectory in a solution.
   */
public:
  Trajectory() = default;
  explicit Trajectory(Linestring points, bool valid = true);
  explicit Trajectory(std::vector<Point> pointvector, bool valid = true);
  bool is_tour() const;
  bool is_path() const;
  double distance_geometry(const Geometry &geo) const;
  double
  distance_geometry_sequence(const Geometry &geo,
                             const std::vector<TourElement> &sequence) const;
  double distance_site(const SiteVariant &site) const;

  double length() const;
  size_t num_points() const;

  bool is_simple() const;

  [[nodiscard]] bool
  covers(const Geometry &geo,
         double tolerance = tspn::constants::FEASIBILITY_TOLERANCE) const;
  [[nodiscard]] bool
  covers(const Geometry &geo, const std::vector<TourElement> &sequence,
         double tolerance = tspn::constants::FEASIBILITY_TOLERANCE) const;
  template <typename It>
  [[nodiscard]] bool
  covers(It begin, It end,
         double tolerance = tspn::constants::FEASIBILITY_TOLERANCE) const {
    return std::all_of(begin, end, [this, tolerance](const Geometry &geo) {
      return covers(geo, tolerance);
    });
  }

  bool is_valid() const;

  Point get(unsigned i) const;

  Point front() const;

  Point back() const;

  Linestring points;

  Linestring get_simplified_points(
      double tolerance = tspn::constants::SPANNING_TOLERANCE) const;

  std::vector<bool> get_spanning_info(
      double tolerance = tspn::constants::SPANNING_TOLERANCE) const;

private:
  mutable std::optional<double> _length;
  bool _is_valid;
};

SiteVariant parse_site_wkt(const std::string &s);

} // namespace tspn
