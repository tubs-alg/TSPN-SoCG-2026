
#include "tspn_core/details/convex_partition.h"
#include "tspn_core/details/convex_decomposition.h"
#include "tspn_core/utils/geometry.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Partition_traits_2.h>
#include <CGAL/partition_2.h>
#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/is_convex.hpp>
#include <list>
#include <ranges>
#include <vector>

namespace bg = boost::geometry;

typedef CGAL::Exact_predicates_exact_constructions_kernel K;
typedef CGAL::Partition_traits_2<K> PTraits;
typedef PTraits::Point_2 CGAL_PTraits_Point;
typedef PTraits::Polygon_2 CGAL_PTraits_Polygon;

namespace tspn::details {

Ring cgalpoly_to_ring(const CGAL_PTraits_Polygon &cpoly) {
  /* special version for these traits - they require a list as a container */
  Ring ring;
  bg::append(ring, Point(CGAL::to_double(cpoly[0].x()),
                         CGAL::to_double(cpoly[0].y())));
  for (const auto &cpoint : std::ranges::views::reverse(cpoly)) {
    bg::append(ring,
               Point(CGAL::to_double(cpoint.x()), CGAL::to_double(cpoint.y())));
  }
  return ring;
}

CGAL_PTraits_Polygon ring_to_cgalpoly(Ring ring) {
  /* special version for these traits - they require a list as a container */
  CGAL_PTraits_Polygon cgalpoly;
  for (auto pt = ring.rbegin(); pt < ring.rend() - 1; pt++) {
    cgalpoly.push_back(CGAL_PTraits_Point(pt->get<0>(), pt->get<1>()));
  };
  return cgalpoly;
}

std::vector<SiteVariant> partition(const SiteVariant &site) {
  std::vector<SiteVariant> results;
  if (std::holds_alternative<Point>(site)) {
    return results;
  } else if (std::holds_alternative<Circle>(site)) {
    return results;
  } else if (const Ring *ring = std::get_if<Ring>(&site)) {
    if (bg::is_convex(*ring)) {
      return results;
    } else {
      CGAL_PTraits_Polygon cgalpoly = ring_to_cgalpoly(*ring);
      std::vector<CGAL_PTraits_Polygon> partition_output;
      CGAL::optimal_convex_partition_2(
          cgalpoly.vertices_begin(), cgalpoly.vertices_end(),
          std::back_insert_iterator(partition_output));
      for (auto &cpol_out : partition_output) {
        Ring partition_ring = cgalpoly_to_ring(cpol_out);
        results.push_back({partition_ring});
      }
      return results;
    }
  } else if (const Polygon *poly = std::get_if<Polygon>(&site)) {
    if (bg::is_convex(*poly)) {
      return results;
    } else if (poly->inners().size() == 0) {
      CGAL_PTraits_Polygon cgalpoly = ring_to_cgalpoly(poly->outer());
      std::vector<CGAL_PTraits_Polygon> partition_output;
      CGAL::optimal_convex_partition_2(
          cgalpoly.vertices_begin(), cgalpoly.vertices_end(),
          std::back_insert_iterator(partition_output));
      for (auto &cpol_out : partition_output) {
        Ring partition_ring = cgalpoly_to_ring(cpol_out);
        results.push_back({partition_ring});
      }
      return results;
    } else {
      /* fallback to decomposition, bcz:
      - optimal convex partition cant deal with PWH natively
      - connecting the holes to the outer makes the opt conv part just fail
      - behaviour in some other holy cases is undefined
      */
      results = decompose(*poly);
      return results;
    }
  } else {
    throw std::logic_error(
        "model_convex_site does not implement this SiteVariant type.");
  }
}
}; // namespace tspn::details
