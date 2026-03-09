#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Constrained_triangulation_plus_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/is_convex.hpp>
#include <list>
#include <ranges>

#include "tspn/details/cgal_conversion.h"
#include "tspn/utils/geometry.h"

namespace bg = boost::geometry;

namespace tspn::details {

Ring cgalpoly_to_ring(const CGAL_Polygon &cpoly) {
  Ring ring;
  bg::append(ring, Point(CGAL::to_double(cpoly[0].x()),
                         CGAL::to_double(cpoly[0].y())));
  for (const auto &cpoint : std::ranges::views::reverse(cpoly)) {
    bg::append(ring,
               Point(CGAL::to_double(cpoint.x()), CGAL::to_double(cpoint.y())));
  }
  return ring;
}

CGAL_Polygon ring_to_cgalpoly(const Ring &ring) {
  CGAL_Polygon cgalpoly;
  for (auto pt = ring.rbegin(); pt < ring.rend() - 1; pt++) {
    cgalpoly.push_back(CGAL_Point(pt->get<0>(), pt->get<1>()));
  };
  return cgalpoly;
}

Polygon cgalpwh_to_poly(const CGAL_Polygon_wh &cpwh) {
  Polygon result;
  result.outer() = cgalpoly_to_ring(cpwh.outer_boundary());
  for (const auto &hole : cpwh.holes()) {
    Ring inner = cgalpoly_to_ring(hole);
    bg::reverse(inner); // CGAL: holes contain, BOOST: holes exclude
    result.inners().push_back(inner);
  }
  return result;
}

CGAL_SiteVariant site_to_cgal(const SiteVariant &site) {
  if (const Polygon *poly = std::get_if<Polygon>(&site)) {
    CGAL_Polygon boundary = ring_to_cgalpoly(poly->outer());
    CGAL_Polygon_wh result(boundary);
    for (const auto &inner : poly->inners()) {
      CGAL_Polygon hole = ring_to_cgalpoly(inner);
      hole.reverse_orientation(); // CGAL: holes contain, BOOST: holes exclude
      result.add_hole(hole);
    }
    return {result};
  } else if (const Ring *ring = std::get_if<Ring>(&site)) {
    CGAL_Polygon boundary = ring_to_cgalpoly(*ring);
    return {boundary};
  } else if (const Point *point = std::get_if<Point>(&site)) {
    CGAL_Point result = CGAL_Point(point->get<0>(), point->get<1>());
    return {result};
  } else if (const Circle *circle = std::get_if<Circle>(&site)) {
    Point pt = circle->center();
    double radius = circle->radius();
    CGAL_Circle result =
        CGAL_Circle(CGAL_Point(pt.get<0>(), pt.get<1>()), radius * radius);
    return {result};
  } else {
    throw std::logic_error(
        "site_to_cgal does not implement this SiteVariant type.");
  }
}

void print_orientation(const CGAL_Polygon &poly) {
  auto orientation = poly.orientation();
  std::cout << ((orientation == CGAL::CLOCKWISE)
                    ? "CLOCKWISE "
                    : ((orientation == CGAL::COUNTERCLOCKWISE)
                           ? "COUNTERCLOCKWISE "
                           : "unknown "));
}

void print_details(const CGAL_Polygon &poly) {
  std::cout << std::endl;
  std::cout << "Details of Polygon " << poly << std::endl;
  std::cout << "simple: " << ((poly.is_simple()) ? "True" : "False")
            << std::endl;
  std::cout << "orientation: ";
  print_orientation(poly);
  std::cout << std::endl;
  std::cout << "area: " << poly.area() << std::endl;
}

void print_details(const CGAL_Polygon_wh &poly_wh) {
  std::cout << std::endl;
  std::cout << "Details of Polygon with holes " << poly_wh << std::endl;
  std::cout << "outer simple: "
            << ((poly_wh.outer_boundary().is_simple()) ? "True" : "False")
            << std::endl;
  std::cout << "hole simple: ";
  for (const auto &hole : poly_wh.holes()) {
    std::cout << ((hole.is_simple()) ? "True " : "False ");
  }
  std::cout << std::endl;

  std::cout << "outer orientation: ";
  print_orientation(poly_wh.outer_boundary());
  std::cout << std::endl;
  std::cout << "hole orientation: ";
  for (const auto &hole : poly_wh.holes()) {
    print_orientation(hole);
  }
  std::cout << std::endl;
  std::cout << "area: " << poly_wh.outer_boundary().area() << std::endl;
  std::cout << "hole area: ";
  for (const auto &hole : poly_wh.holes()) {
    std::cout << hole.area();
  }
  std::cout << std::endl;
}

} // namespace tspn::details
