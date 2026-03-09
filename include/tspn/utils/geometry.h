//
// Created by Dominik Krupke on 15.01.23.
//

#ifndef TSPN_GEOMETRY_H
#define TSPN_GEOMETRY_H
#include "doctest/doctest.h"
#include "tspn/types.h"
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

TEST_CASE("Distance to Segment") {
  CHECK(distance_to_segment({0, 0}, {10, 0}, {0, 0}) == doctest::Approx(0.0));
  CHECK(distance_to_segment({0, 0}, {10, 0}, {0, 1}) == doctest::Approx(1.0));
  CHECK(distance_to_segment({0, 0}, {10, 0}, {0, -1}) == doctest::Approx(1.0));
  CHECK(distance_to_segment({0, 0}, {10, 0}, {-1, 0}) == doctest::Approx(1.0));
  CHECK(distance_to_segment({0, 0}, {10, 0}, {11, 0}) == doctest::Approx(1.0));
}
} // namespace tspn::utils

#endif // TSPN_GEOMETRY_H
