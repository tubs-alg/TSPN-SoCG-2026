//
// just for type-aliasing / typedef-ing
//
#pragma once

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>
#include <boost/geometry/geometries/segment.hpp>
#include <variant>

namespace bg = boost::geometry;

namespace tspn {

// added
typedef bg::model::point<double, 2, bg::cs::cartesian> Point;
typedef bg::model::segment<Point> Segment;
typedef bg::model::polygon<Point> Polygon;
typedef bg::model::multi_polygon<Polygon> MultiPolygon;
typedef bg::model::linestring<Point>
    Linestring; // Curve with linear interpolation between Points (OGC)
typedef bg::ring_type<Polygon>::type Ring;
typedef bg::model::box<Point> Box;

class Circle {
public:
  Circle(const Point &center, double radius)
      : _center(center), _radius(radius) {};
  Point center() const { return _center; };
  double radius() const { return _radius; };
  Circle(const std::string &wkt) {
    if (!wkt.starts_with("CIRCLE")) {
      throw std::invalid_argument("wkt not for circle");
    }
    // mostly stolen from boost wkt/read.hpp
    using separator = boost::char_separator<char>;
    using tokenizer = boost::tokenizer<separator>;
    const tokenizer tokens(wkt, separator(" \n\t\r", ",()"));
    auto it = tokens.begin();
    const auto end = tokens.end();
    it++; // "CIRCLE"
    bg::detail::wkt::handle_open_parenthesis(it, end, wkt);
    if (*it == "POINT") {
      it++;
    }
    bg::detail::wkt::handle_open_parenthesis(it, end, wkt);
    bg::detail::wkt::parsing_assigner<Point>::apply(it, end, _center, wkt);
    bg::detail::wkt::handle_close_parenthesis(it, end, wkt);
    if (*(it++) != ",") {
      throw std::invalid_argument(
          "malformed circle wkt. expected comma after center.");
    }
    _radius = bg::detail::coordinate_cast<double>::apply(*(it++));
    bg::detail::wkt::handle_close_parenthesis(it, end, wkt);
  };

private:
  Point _center;
  double _radius;
};

// variant = more modern union, these types are okay to use as a site.
// depending on the context, only a subset of these might actually make sense.
using SiteVariant = std::variant<Point,   // point depots
                                 Polygon, // polygons
                                 Circle,  // circles
                                 Ring>;   // exterior ring of a polygon

// overload for visit and lambdas
template <class... Ts> struct overload : Ts... {
  using Ts::operator()...;
};

}; // namespace tspn
