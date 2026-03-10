/**
 * convert boost::geometry <-> cgal (basic types!)
 */

#ifndef CGAL_CONVERSION
#define CGAL_CONVERSION

#include "tspn_core/types.h"
#include <CGAL/Circle_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

namespace tspn::details {

using K = CGAL::Exact_predicates_exact_constructions_kernel;
using CGAL_Point = CGAL::Point_2<K>;
using CGAL_Circle = CGAL::Circle_2<K>;
using CGAL_Polygon = CGAL::Polygon_2<K>;
using CGAL_Segment = CGAL::Segment_2<K>;
using CGAL_Polygon_wh = CGAL::Polygon_with_holes_2<K>;
using CGAL_FT = K::FT;

using CGAL_SiteVariant =
    std::variant<CGAL_Point, CGAL_Circle, CGAL_Polygon, CGAL_Polygon_wh>;

Ring cgalpoly_to_ring(const CGAL_Polygon &cpoly);

CGAL_Polygon ring_to_cgalpoly(const Ring &ring);

Polygon cgalpwh_to_poly(const CGAL_Polygon_wh &cpwh);

CGAL_SiteVariant site_to_cgal(const SiteVariant &site);
SiteVariant cgal_to_site(const CGAL_SiteVariant &site);

void print_details(const CGAL_Polygon_wh &poly_wh);

void print_details(const CGAL_Polygon &poly);
}; // namespace tspn::details

#endif // CGAL_CONVERSION
