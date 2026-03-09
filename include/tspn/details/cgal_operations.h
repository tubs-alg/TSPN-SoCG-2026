#pragma once

#include "tspn/details/cgal_conversion.h"
#include "tspn/details/cgal_operations.h"
#include "tspn/utils/distance.h"
#include "tspn/utils/geometry.h"
#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/IO/WKT.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/tags.h>
#include <boost/geometry.hpp>
#include <ranges>

#define EPS 1e-12
#define PI 3.14159265358979323846

namespace tspn::details {

CGAL_FT area(const CGAL_Point &csite);
CGAL_FT area(const CGAL_Circle &csite);
CGAL_FT area(const CGAL_Polygon_wh &csite);
CGAL_FT area(const CGAL_Polygon &csite);
CGAL_FT area(const CGAL_SiteVariant &csite);

bool contains(const CGAL_Point &inner, const CGAL_Point &outer);
bool contains(const CGAL_Point &inner, const CGAL_Circle &outer);
bool contains(const CGAL_Point &inner, const CGAL_Polygon &outer);
bool contains(const CGAL_Point &inner, const CGAL_Polygon_wh &outer);
bool contains(const CGAL_Circle &inner, const CGAL_Point &outer);
bool contains(const CGAL_Circle &inner, const CGAL_Circle &outer);
bool contains(const CGAL_Circle &inner, const CGAL_Polygon &outer);
bool contains(const CGAL_Circle &inner, const CGAL_Polygon_wh &outer);
bool contains(const CGAL_Polygon &inner, const CGAL_Point &outer);
bool contains(const CGAL_Polygon &inner, const CGAL_Circle &outer);
bool contains(const CGAL_Polygon &inner, const CGAL_Polygon &outer);
bool contains(const CGAL_Polygon &inner, const CGAL_Polygon_wh &outer);
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Point &outer);
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Circle &outer);
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Polygon &outer);
bool contains(const CGAL_Polygon_wh &inner, const CGAL_Polygon_wh &outer);
bool contains(const CGAL_SiteVariant &inner, const CGAL_SiteVariant &outer);
} // namespace tspn::details
