#include "tspn_core/common.h"
#include "tspn_core/types.h"
#include "tspn_core/utils/distance.h"
#include "tspn_core/utils/geometry.h"
#include <stdexcept>
#include <vector>

namespace tspn {

// Parameter storage and default values
namespace constants {
double FEASIBILITY_TOLERANCE = 0.001;
double SPANNING_TOLERANCE = 0.0009;
unsigned MAX_TE_TOGGLE_COUNT = 1;
unsigned MAX_GEOM_TOGGLE_COUNT = 1;

// Default values for reset
namespace defaults {
constexpr double FEASIBILITY_TOLERANCE = 0.001;
constexpr double SPANNING_TOLERANCE = 0.0009;
constexpr unsigned MAX_TE_TOGGLE_COUNT = 1;
constexpr unsigned MAX_GEOM_TOGGLE_COUNT = 1;
} // namespace defaults

void set_feasibility_tolerance(double value) {
  if (value <= 0.0) {
    throw std::invalid_argument(
        "FEASIBILITY_TOLERANCE must be positive, got: " +
        std::to_string(value));
  }
  FEASIBILITY_TOLERANCE = value;
}

void set_spanning_tolerance(double value) {
  if (value <= 0.0) {
    throw std::invalid_argument("SPANNING_TOLERANCE must be positive, got: " +
                                std::to_string(value));
  }
  SPANNING_TOLERANCE = value;
}

void set_max_te_toggle_count(unsigned value) { MAX_TE_TOGGLE_COUNT = value; }

void set_max_geom_toggle_count(unsigned value) {
  MAX_GEOM_TOGGLE_COUNT = value;
}

void reset_parameters() {
  FEASIBILITY_TOLERANCE = defaults::FEASIBILITY_TOLERANCE;
  SPANNING_TOLERANCE = defaults::SPANNING_TOLERANCE;
  MAX_TE_TOGGLE_COUNT = defaults::MAX_TE_TOGGLE_COUNT;
  MAX_GEOM_TOGGLE_COUNT = defaults::MAX_GEOM_TOGGLE_COUNT;
}

void set_float_parameter(const std::string &name, double value) {
  if (name == "FEASIBILITY_TOLERANCE" || name == "feasibility_tolerance") {
    set_feasibility_tolerance(value);
  } else if (name == "SPANNING_TOLERANCE" || name == "spanning_tolerance") {
    set_spanning_tolerance(value);
  } else {
    throw std::invalid_argument("Unknown float parameter: " + name +
                                ". Valid options: FEASIBILITY_TOLERANCE, "
                                "SPANNING_TOLERANCE");
  }
}

void set_int_parameter(const std::string &name, unsigned value) {
  if (name == "MAX_TE_TOGGLE_COUNT" || name == "max_te_toggle_count") {
    set_max_te_toggle_count(value);
  } else if (name == "MAX_GEOM_TOGGLE_COUNT" ||
             name == "max_geom_toggle_count") {
    set_max_geom_toggle_count(value);
  } else {
    throw std::invalid_argument(
        "Unknown int parameter: " + name +
        ". Valid options: MAX_TE_TOGGLE_COUNT, MAX_GEOM_TOGGLE_COUNT");
  }
}

double get_float_parameter(const std::string &name) {
  if (name == "FEASIBILITY_TOLERANCE" || name == "feasibility_tolerance") {
    return FEASIBILITY_TOLERANCE;
  } else if (name == "SPANNING_TOLERANCE" || name == "spanning_tolerance") {
    return SPANNING_TOLERANCE;
  } else {
    throw std::invalid_argument("Unknown float parameter: " + name +
                                ". Valid options: FEASIBILITY_TOLERANCE, "
                                "SPANNING_TOLERANCE");
  }
}

unsigned get_int_parameter(const std::string &name) {
  if (name == "MAX_TE_TOGGLE_COUNT" || name == "max_te_toggle_count") {
    return MAX_TE_TOGGLE_COUNT;
  } else if (name == "MAX_GEOM_TOGGLE_COUNT" ||
             name == "max_geom_toggle_count") {
    return MAX_GEOM_TOGGLE_COUNT;
  } else {
    throw std::invalid_argument(
        "Unknown int parameter: " + name +
        ". Valid options: MAX_TE_TOGGLE_COUNT, MAX_GEOM_TOGGLE_COUNT");
  }
}

} // namespace constants

bool has_holes(const SiteVariant *site) {
  if (const Polygon *poly = std::get_if<Polygon>(site)) {
    return (poly->inners().size() > 0);
  } else {
    return false;
  }
}

Geometry::Geometry(unsigned geo_index, const SiteVariant &definition)
    : _is_convex(tspn::utils::is_convex(definition)),
      _has_holes(tspn::has_holes(&definition)), _definition(definition),
      _geo_index(geo_index), _convex_hull(tspn::utils::convex_hull(definition)),
      _decomposition(tspn::details::partition(definition)),
      geom_toggle_count(0) {}

Instance::Instance() {}
Instance::Instance(std::vector<SiteVariant> &sites, bool path) : path(path) {
  reserve(sites.size());
  for (SiteVariant &site : sites) {
    tspn::utils::correct(site);
    push_back(Geometry(revision++, site));
  }
}

bool Instance::is_path() const { return path; }
bool Instance::is_tour() const { return !path; }

void Instance::add_site(SiteVariant &site) {
  tspn::utils::correct(site);
  push_back(Geometry(revision++, site));
}
void Instance::add_site(Circle &geom) {
  SiteVariant site = SiteVariant(geom);
  return add_site(site);
}
void Instance::add_site(Polygon &geom) {
  SiteVariant site = SiteVariant(geom);
  return add_site(site);
}
void Instance::add_site(Ring &geom) {
  SiteVariant site = SiteVariant(geom);
  return add_site(site);
}
void Instance::add_site(Point &geom) {
  SiteVariant site = SiteVariant(geom);
  return add_site(site);
}

Instance Instance::hull_instance() const {
  std::vector<SiteVariant> hulls;
  for (const Geometry &geo : *this) {
    hulls.push_back(geo.convex_hull());
  }
  return Instance(hulls);
}

TourElement::TourElement(unsigned geo_index, unsigned decomposition_index,
                         const Instance *instance, const Box &bbox,
                         bool is_lazy, bool is_exact)
    : _geo_index(geo_index), _decomposition_index(decomposition_index),
      _instance(instance), _bbox(bbox), _is_lazy(is_lazy), _is_exact(is_exact),
      _is_active(true), te_toggle_count(0) {}
TourElement::TourElement(unsigned geo_index, const Instance *instance,
                         bool is_exact)
    : TourElement(
          /*geo_index=*/geo_index, /*decomposition_index=*/0,
          /*instance=*/instance,
          /*bbox=*/tspn::utils::bbox((*instance)[geo_index].convex_hull()),
          /*is_lazy=*/(!((*instance)[geo_index].is_convex()) && !(is_exact)),
          /*is_exact=*/is_exact) {}
TourElement::TourElement(const Instance &instance, unsigned geo_index)
    : TourElement(
          /*geo_index=*/geo_index, /*decomposition_index=*/0,
          /*instance=*/&instance,
          /*bbox=*/tspn::utils::bbox(instance[geo_index].convex_hull()),
          /*is_lazy=*/!(instance[geo_index].is_convex()),
          /*is_exact=*/false) {}
bool TourElement::listorder(const TourElement &rhs) const {
  return geo_index() < rhs.geo_index() ||
         (geo_index() == rhs.geo_index() &&
          (decomposition_index() < rhs.decomposition_index() ||
           (decomposition_index() == rhs.decomposition_index() &&
            (is_exact() < rhs.is_exact() ||
             (is_exact() == rhs.is_exact() && (is_lazy() < rhs.is_lazy()))))));
}
bool TourElement::operator==(const TourElement &rhs) const {
  return geo_index() == rhs.geo_index() &&
         decomposition_index() == rhs.decomposition_index() &&
         is_exact() == rhs.is_exact() && is_lazy() == is_lazy();
}
bool TourElement::operator!=(const TourElement &rhs) const {
  return !(operator==(rhs));
}
const SiteVariant &TourElement::active_convex_region() const {
  /* active convex region = the convex hull in most scenarios,
  except when all of the following hold true:
  1. geom not convex (obv)
  2. not lazy (obv)
  3. not set to exact modeling (in that case, decomposition is modeled)
  In that case, the tour element is just a convex fragment => return that.
  */
  if (geometry()->is_convex() || is_lazy() || is_exact()) {
    return geometry()->convex_hull_ref();
  } else {
    return geometry()->decomposition(decomposition_index());
  }
}
const std::vector<TourElement> TourElement::branch() const {
  assert(is_lazy() == true);
  assert(geometry()->is_convex() == false);
  assert(is_exact() == false);
  std::vector<TourElement> branches;
  if (is_lazy() == true && geometry()->is_convex() == false) {
    for (unsigned i = 0; i < geometry()->decomposition().size(); i++) {
      branches.push_back(TourElement(
          /*geo_index=*/geo_index(), /*decomposition_index=*/i,
          /*instance=*/_instance,
          /*bbox=*/tspn::utils::bbox(geometry()->decomposition(i)),
          /*is_lazy=*/false,
          /*is_exact=*/false));
    }
  }
  return branches;
}
const TourElement TourElement::set_exact() const {
  assert(is_lazy() == true);
  assert(geometry()->is_convex() == false);
  assert(is_exact() == false);
  return TourElement(/*geo_index=*/geo_index(), /*decomposition_index=*/0,
                     /*instance=*/_instance, /*bbox=*/bbox(),
                     /*is_lazy=*/false, /*is_exact=*/true);
}

const SiteVariant &TourElement::modeled_site() const {
  // modeled site is definition, if exact, otherwise active convex region.
  if (is_exact()) {
    return geometry()->definition();
  } else {
    return active_convex_region();
  }
}

std::atomic<size_t> TourElement::enable_count = 0;
std::atomic<size_t> TourElement::disable_count = 0;
void TourElement::enable() {
  if (!_is_active) {
    _is_active = true;
    te_toggle_count++;
    geometry()->geom_toggle_count++;
    enable_count++;
  }
}
void TourElement::disable() {
  if (_is_active) {
    _is_active = false;
    disable_count++;
  }
}
bool TourElement::can_be_disabled() const {
  return (
      _is_active && te_toggle_count < tspn::constants::MAX_TE_TOGGLE_COUNT &&
      geometry()->geom_toggle_count < tspn::constants::MAX_GEOM_TOGGLE_COUNT);
}

Trajectory::Trajectory(Linestring points, bool valid)
    : points{std::move(points)}, _is_valid{valid} {}
Trajectory::Trajectory(std::vector<Point> pointvector, bool valid)
    : points(pointvector.begin(), pointvector.end()), _is_valid{valid} {}
bool Trajectory::is_path() const {
  return !bg::equals(points[0], points[bg::num_points(points) - 1]);
}
bool Trajectory::is_tour() const {
  return bg::equals(points[0], points[bg::num_points(points) - 1]);
}
bool Trajectory::is_valid() const { return _is_valid; }
double Trajectory::distance_site(const SiteVariant &site) const {
  return tspn::utils::distance(points, site);
}
double Trajectory::distance_geometry(const Geometry &geo) const {
  return tspn::utils::distance(points, geo.definition());
}
double Trajectory::distance_geometry_sequence(
    const Geometry &geo, const std::vector<TourElement> &sequence) const {
  auto found_te =
      std::find_if(sequence.begin(), sequence.end(), [&geo](auto &te) {
        return te.geo_index() == geo.geo_index();
      });
  if (found_te != sequence.end() && found_te->can_bypass_distance()) {
    return 0;
  } else {
    return distance_geometry(geo);
  }
}

double Trajectory::length() const {
  if (!_length) {
    _length = bg::length(points);
  }
  return *_length;
}
size_t Trajectory::num_points() const { return bg::num_points(points); }

bool Trajectory::is_simple() const { return bg::is_simple(points); }
bool Trajectory::covers(const Geometry &geo, double tolerance) const {
  return distance_geometry(geo) <= tolerance;
}
bool Trajectory::covers(const Geometry &geo,
                        const std::vector<TourElement> &sequence,
                        double tolerance) const {
  return distance_geometry_sequence(geo, sequence) <= tolerance;
}

Point Trajectory::get(unsigned i) const { return points[i]; }
Point Trajectory::front() const { return points.front(); }
Point Trajectory::back() const { return points.back(); }
Linestring Trajectory::get_simplified_points(double tolerance) const {
  Linestring simplified;
  std::vector<bool> spanning_info = get_spanning_info(tolerance);
  for (unsigned i = 0; i < spanning_info.size(); i++) {
    if (spanning_info[i]) {
      simplified.push_back(points[i]);
    }
  }
  return simplified;
}
std::vector<bool> Trajectory::get_spanning_info(double tolerance) const {
  std::vector<bool> spanning_info = {};
  if (is_path()) {
    spanning_info.push_back(true);
  } else {
    const Point &prev = get(num_points() - 2); //-1 is closing point for tour
    const Point &curr = get(0);
    const Point &next = get(1);
    Segment s = Segment(prev, next);
    double dist = tspn::utils::distance(s, curr);
    spanning_info.push_back(dist >= tspn::constants::SPANNING_TOLERANCE);
  }
  for (unsigned center = 1; center < num_points() - 1; center++) {
    const Point &prev = get(center - 1);
    const Point &curr = get(center);
    const Point &next = get(center + 1);
    Segment s = Segment(prev, next);
    double dist = tspn::utils::distance(s, curr);
    spanning_info.push_back(dist >= tspn::constants::SPANNING_TOLERANCE);
  }
  if (is_path()) {
    spanning_info.push_back(true);
  } else {
    spanning_info.push_back(spanning_info[0]);
  }
  return spanning_info;
};

SiteVariant parse_site_wkt(const std::string &s) {
  // types: Point, Polygon, Circle, Ring (via polygon)
  // find the applicable and use that.
  if (s.starts_with("POINT")) {
    Point p;
    bg::read_wkt(s, p);
    return {p};
  } else if (s.starts_with("POLYGON")) {
    Polygon p;
    bg::read_wkt(s, p);
    bg::correct(p);
    if (size(p.inners()) == 0) {
      return {p.outer()};
    } else {
      return {p};
    }
  } else if (s.starts_with("CIRCLE")) {
    // NOTE: wkt does not do circle -> need to do our own spec!
    // CIRCLE ((x, y), r)
    return {Circle(s)};
  } else {
    throw std::invalid_argument("unrecogized wkt type.");
  }
}

}; // namespace tspn
