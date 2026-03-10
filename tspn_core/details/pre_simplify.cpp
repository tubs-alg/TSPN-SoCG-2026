#include "tspn_core/details/pre_simplify.h"
#include "tspn_core/details/cgal_conversion.h"
#include "tspn_core/details/cgal_operations.h"
#include "tspn_core/utils/distance.h"
#include "tspn_core/utils/geometry.h"
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/IO/WKT.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/tags.h>
#include <boost/geometry.hpp>
#include <ranges>

#include <CGAL/number_type_config.h>
#include <ranges>

namespace bg = boost::geometry;

typedef CGAL::Arr_segment_traits_2<tspn::details::K> Arr_Traits;
typedef CGAL::Arrangement_2<Arr_Traits> Arrangement;
typedef Arrangement::Vertex_handle Vertex_handle;
typedef Arrangement::Halfedge_handle Halfedge_handle;
typedef Arrangement::Face_handle Face_handle;
typedef Arrangement::Vertex_const_handle Vertex_const_handle;
typedef Arrangement::Halfedge_const_handle Halfedge_const_handle;
typedef Arrangement::Face_const_handle Face_const_handle;

namespace tspn::details {

std::vector<CGAL_SiteVariant> convert(const std::vector<SiteVariant> &sites) {
  std::vector<CGAL_SiteVariant> csites;
  for (const auto &site : sites) {
    csites.push_back(site_to_cgal(site));
  }
  return csites;
}

bool contains_with_prio(const CGAL_SiteVariant &inner,
                        const CGAL_SiteVariant &outer,
                        bool priority /* inner < outer for index/adress */) {
  if (contains(inner, outer) && (priority || !contains(outer, inner))) {
    return true;
  }
  return false;
}

void remove_holes(std::vector<SiteVariant> &sites) {
  for (unsigned i = 0; i < sites.size(); i++) {
    if (const Polygon *poly = std::get_if<Polygon>(&sites[i])) {
      std::vector<Ring> kept_holes;
      Polygon result_poly;
      bg::assign(result_poly, poly->outer());
      for (const auto &hole : poly->inners()) {
        Ring inner_reversed = hole;
        // holes would include volume on outside -> invert/correct to inside
        bg::correct(inner_reversed);
        // for each hole: check all sites distances.
        // if site with positive distance exists => hole unnecessary
        bool hole_required = true;
        for (unsigned j = 0; j < sites.size(); j++) {
          if (i != j && tspn::utils::distance(inner_reversed, sites[j]) > 0) {
            hole_required = false;
            break;
          }
        }
        if (hole_required) {
          auto &inners = result_poly.inners();
          inners.resize(inners.size() + 1);
          bg::append(inners[inners.size() - 1], hole);
        }
      }
      if (result_poly.inners().size() == 0) {
        sites[i] = result_poly.outer();
      } else {
        sites[i] = result_poly;
      }
    }
  }
}

Ring convex_hull(std::vector<SiteVariant> &sites) {
  // step 1: collect all points.
  bg::model::multi_point<Point> points;
  for (const auto &site : sites) {
    if (const Point *point = std::get_if<Point>(&site)) {
      bg::append(points, *point);
    } else if (const Circle *circle = std::get_if<Circle>(&site)) {
      // can't, *but* i fake with points (bbox)
      double radius = circle->radius();
      double x = circle->center().get<0>();
      double y = circle->center().get<1>();
      bg::append(points, Point(x + radius, y + radius));
      bg::append(points, Point(x + radius, y - radius));
      bg::append(points, Point(y - radius, y + radius));
      bg::append(points, Point(y - radius, y - radius));
    } else if (const Ring *ring = std::get_if<Ring>(&site)) {
      for (const auto &p : *ring) {
        bg::append(points, p);
      }
    } else if (const Polygon *poly = std::get_if<Polygon>(&site)) {
      for (const auto &p : poly->outer()) {
        bg::append(points, p);
      }
    } else {
      throw std::logic_error(
          "model_convex_site does not implement this SiteVariant type.");
    }
  }
  // step 2: convex hull them
  Ring hull;
  bg::convex_hull(points, hull);
  return hull;
}

void convex_hull_fill(std::vector<SiteVariant> &sites) {
  Ring hull = convex_hull(sites);
  size_t hull_points = hull.size() - 1; // -1 for closed / repetition
  // for each site: if poly, find positions on CH
  for (unsigned site_pos = 0; site_pos < sites.size(); site_pos++) {
    Ring ring;
    Polygon base;
    if (const Ring *sr = std::get_if<Ring>(&sites[site_pos])) {
      bg::assign(ring, *sr);
      bg::assign(base, *sr);
    } else if (const Polygon *sp = std::get_if<Polygon>(&sites[site_pos])) {
      bg::assign(ring, sp->outer());
      bg::assign(base, *sp);
    } else {
      continue;
    }
    size_t ring_points = ring.size() - 1; // -1 for closed / repetition
    std::vector<std::tuple<unsigned, unsigned>> matches = {};
    for (unsigned r = 0; r < ring_points; r++) {
      for (unsigned h = 0; h < hull_points; h++) {
        if (bg::equals(hull[h], ring[r])) {
          matches.push_back({r, h});
        }
      }
    }
    if (matches.size() >= 2) {
      for (unsigned match_idx = 0; match_idx < matches.size(); match_idx++) {
        // for each match-pair, build the area in between
        unsigned ring_origin = std::get<0>(matches[match_idx]);
        unsigned ring_target =
            std::get<0>(matches[(match_idx + 1) % matches.size()]);
        unsigned hull_origin = std::get<1>(matches[match_idx]);
        unsigned hull_target =
            std::get<1>(matches[(match_idx + 1) % matches.size()]);
        if ((ring_origin + 1) % ring_points == ring_target) {
          continue; // irrelevant, as consecutive points are always consecutive
                    // on hull and not fillable
        }
        Ring between;
        // push all points from hull
        for (unsigned hull_pos = hull_origin; hull_pos != hull_target;
             hull_pos = (hull_pos + 1) % hull_points) {
          bg::append(between, hull[hull_pos]);
        }
        // ring in reverse
        for (unsigned ring_pos = ring_target; ring_pos != ring_origin;
             ring_pos = (ring_pos + ring_points - 1) % ring_points) {
          bg::append(between, ring[ring_pos]);
        }
        bg::append(between, ring[ring_origin]);
        bg::correct(between);

        // if at least one site with distance -> can fill
        if (std::any_of(sites.begin(), sites.end(),
                        [&between](const auto &site) {
                          return tspn::utils::distance(between, site) > 0;
                        })) {
          std::vector<Polygon> out;
          bg::union_(base, between, out);
          if (out.size() != 1) {
            throw std::logic_error("hull filled geometry is not connected.");
          }
          base = out[0];
          if (base.inners().size() == 0) {
            sites[site_pos] = base.outer();
          } else {
            sites[site_pos] = base;
          }
        }
      }
    }
  }
}

void remove_supersites(std::vector<SiteVariant> &sites) {
  std::vector<CGAL_SiteVariant> csites = convert(sites);
  std::vector<unsigned> removed_sites;
  // iterate downwards for deletion lateron
  for (unsigned outer_idx = csites.size(); outer_idx > 0; outer_idx--) {
    const auto &outer = csites[outer_idx - 1];
    for (unsigned inner_idx = csites.size(); inner_idx > 0; inner_idx--) {
      const auto &inner = csites[inner_idx - 1];
      if (inner_idx != outer_idx &&
          contains_with_prio(inner, outer, inner_idx < outer_idx)) {
        // found a smaller inner => outer is not required
        sites.erase(sites.begin() + outer_idx - 1);
        break;
      }
    }
  }
}

std::vector<SiteVariant> simplify(std::vector<SiteVariant> &sites) {
  /* runs all steps */
  std::vector<SiteVariant> results(sites);
  remove_holes(results);
  convex_hull_fill(results);
  remove_supersites(results);
  return results;
}

} // namespace tspn::details
