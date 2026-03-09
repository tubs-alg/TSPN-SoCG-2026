#include "tspn/details/cgal_operations.h"
#include "tspn/details/cgal_conversion.h"
#include "tspn/utils/distance.h"
#include "tspn/utils/geometry.h"
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/IO/WKT.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/tags.h>
#include <boost/geometry.hpp>
#include <ranges>

namespace tspn::details {

CGAL_FT area(const CGAL_Point &csite) { return CGAL_FT(0); }
CGAL_FT area(const CGAL_Circle &csite) { return csite.squared_radius() * PI; }
CGAL_FT area(const CGAL_Polygon_wh &csite) {
  CGAL_FT running_area = csite.outer_boundary().area();
  for (const auto &hole : csite.holes()) {
    running_area -= hole.area();
  }
  return running_area;
}
CGAL_FT area(const CGAL_Polygon &csite) { return csite.area(); }
CGAL_FT area(const CGAL_SiteVariant &csite) {
  return std::visit([](const auto &cs) { return area(cs); }, csite);
}

CGAL_Point interior_point(const CGAL_Point &p) { return p; }
CGAL_Point interior_point(const CGAL_Circle &c) { return c.center(); }
CGAL_Point interior_point(const CGAL_Polygon &pg) {
  assert(!pg.is_empty());      // CGAL guarantees non‑empty for a valid polygon
  return *pg.vertices_begin(); // any vertex belongs to the interior/boundary
}
CGAL_Point interior_point(const CGAL_Polygon_wh &pwh) {
  assert(!pwh.outer_boundary().is_empty());
  return *pwh.outer_boundary().vertices_begin();
}

bool point_inside_polygon(const CGAL_Point &q, const CGAL_Polygon &poly) {
  return CGAL::oriented_side(q, poly) != CGAL::NEGATIVE;
}

bool point_inside_polygon_wh(const CGAL_Point &q, const CGAL_Polygon_wh &pwh) {
  return CGAL::oriented_side(q, pwh) != CGAL::NEGATIVE;
}

bool point_inside_circle(const CGAL_Point &q, const CGAL_Circle &c) {
  return CGAL::squared_distance(q, c.center()) <= c.squared_radius() + EPS;
}

// -------------------------------------------------------------------
//  Main containment tests (inner in outer)
// -------------------------------------------------------------------

// Point
bool contains(const CGAL_Point &inner, const CGAL_Point &outer) {
  return inner == outer;
}
bool contains(const CGAL_Point &inner, const CGAL_Circle &outer) {
  return point_inside_circle(inner, outer);
}
bool contains(const CGAL_Point &inner, const CGAL_Polygon &outer) {
  return point_inside_polygon(inner, outer);
}
bool contains(const CGAL_Point &inner, const CGAL_Polygon_wh &outer) {
  return point_inside_polygon_wh(inner, outer);
}

// Circle
bool contains(const CGAL_Circle &inner, const CGAL_Point &outer) {
  return false;
}
bool contains(const CGAL_Circle &inner, const CGAL_Circle &outer) {
  if (inner.squared_radius() > outer.squared_radius()) {
    return false;
  }
  double d2 =
      CGAL::to_double(CGAL::squared_distance(inner.center(), outer.center()));
  double rDiff =
      CGAL::to_double(outer.squared_radius() - inner.squared_radius());
  // we need sqrt(d2) + r_in <= r_out  ->  d2 <= (r_out - r_in)²
  return std::sqrt(d2) <= std::sqrt(rDiff) + EPS;
}
bool contains(const CGAL_Circle &inner, const CGAL_Polygon &outer) {
  // 1) center must be inside polygon
  // 2) distance from center to every edge >= radius
  if (!point_inside_polygon(inner.center(), outer)) {
    return false;
  }

  // Minimal distance from center to the polygon’s edges:
  for (auto eit = outer.edges_begin(); eit != outer.edges_end(); ++eit) {
    const double dist =
        CGAL::to_double(CGAL::squared_distance(inner.center(), *eit));
    if (dist < inner.squared_radius() - EPS) {
      return false; // an edge cuts the circle
    }
  }
  return true;
}
bool contains(const CGAL_Circle &inner, const CGAL_Polygon_wh &outer) {
  // Must be fully inside the outer boundary **and** away from every hole.
  if (!point_inside_polygon(inner.center(), outer.outer_boundary())) {
    return false;
  }

  // distance to outer boundary edges
  for (auto eit = outer.outer_boundary().edges_begin();
       eit != outer.outer_boundary().edges_end(); ++eit) {
    if (CGAL::squared_distance(inner.center(), *eit) <
        inner.squared_radius() - EPS) {
      return false;
    }
  }

  // also stay away from every hole
  for (auto hit = outer.holes_begin(); hit != outer.holes_end(); ++hit) {
    // the center may be inside a hole only if the whole circle is outside it,
    // which is impossible for a non‑zero radius.
    if (point_inside_polygon(inner.center(), *hit)) {
      return false;
    }
    for (auto eit = (*hit).edges_begin(); eit != (*hit).edges_end(); ++eit) {
      if (CGAL::squared_distance(inner.center(), *eit) <
          inner.squared_radius() - EPS) {
        return false;
      }
    }
  }
  return true;
}

// Polygon
bool contains(const CGAL_Polygon &inner, const CGAL_Point &outer) {
  return false;
}
bool contains(const CGAL_Polygon &inner, const CGAL_Circle &outer) {
  // All vertices must lie inside (or on) the circle.
  for (auto vit = inner.vertices_begin(); vit != inner.vertices_end(); ++vit) {
    if (!point_inside_circle(*vit, outer)) {
      return false;
    }
  }
  return true;
}
bool contains(const CGAL_Polygon &inner, const CGAL_Polygon &outer) {
  // 1) representative interior point of the inner polygon must be inside outer,
  // 2) the two polygons must not intersect (touching is allowed).
  if (!point_inside_polygon(interior_point(inner), outer)) {
    return false;
  }
  if (area(inner) > area(outer)) {
    return false;
  }
  // A safe way: compute the intersection and compare areas.
  std::vector<CGAL_Polygon_wh> inter;
  CGAL::intersection(inner, outer, std::back_inserter(inter));
  if (inter.size() != 1) {
    return false;
  }
  return area(inter[0]) == area(inner);
}
bool contains(const CGAL_Polygon &inner, const CGAL_Polygon_wh &outer) {
  // Same logic as polygon‑vs‑polygon, but also need to be outside every hole.
  if (!point_inside_polygon_wh(interior_point(inner), outer)) {
    return false;
  }
  if (area(inner) > area(outer)) {
    return false;
  }
  std::vector<CGAL_Polygon_wh> inter;
  CGAL::intersection(inner, outer.outer_boundary(), std::back_inserter(inter));
  if (inter.size() != 1) {
    return false;
  }
  if (area(inter[0]) != area(inner)) {
    return false; // not fully within outer
  }
  // Must not intersect any hole.
  for (auto hit = outer.holes_begin(); hit != outer.holes_end(); ++hit) {
    if (CGAL::do_intersect(inner, *hit)) {
      return false;
    }
  }
  return true;
}

// Polygon with holes
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Point &outer) {
  return false;
}
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Circle &outer) {
  // outer boundary has to be contained, holes are irrelevant (simple)
  return contains(inner.outer_boundary(), outer);
}
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Polygon &outer) {
  // outer boundary has to be contained, holes are irrelevant (simple)
  return contains(inner.outer_boundary(), outer);
}
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Polygon_wh &outer) {
  if (!point_inside_polygon_wh(interior_point(inner), outer))
    return false;
  if (area(inner) > area(outer)) {
    return false;
  }
  // intersection-area trick
  std::vector<CGAL_Polygon_wh> inter;
  CGAL::intersection(inner, outer.outer_boundary(), std::back_inserter(inter));
  if (inter.size() != 1) {
    return false;
  }
  if (area(inter[0]) == area(inner)) {
    return true;
  }
  return false; // not fully within outer
}

bool contains(const CGAL_SiteVariant &inner, const CGAL_SiteVariant &outer) {
  return std::visit([](const auto &i, const auto &o) { return contains(i, o); },
                    inner, outer);
}
} // namespace tspn::details
