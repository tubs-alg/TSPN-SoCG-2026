//
// Created by Dominik Krupke on 15.01.23.
//

#ifndef TSPN_GEOMETRY_H
#define TSPN_GEOMETRY_H
#include "tspn_core/types.h"
#include <cmath>
#include <utility>
namespace tspn::utils {

Box bbox(const SiteVariant &site);

void correct(SiteVariant &site);

template <class GeomType> bool is_convex(const GeomType &geo);

SiteVariant convex_hull(const SiteVariant &geo);

double distance_to_segment(std::pair<double, double> s0,
                           std::pair<double, double> s1,
                           std::pair<double, double> p);

bool equals(const Polygon &a, const Polygon &b);
bool equals(const Ring &a, const Ring &b);

std::vector<Segment> to_segments(Ring &ring);

Ring to_ring(const std::vector<Segment> &segments);

} // namespace tspn::utils

#endif // TSPN_GEOMETRY_H
