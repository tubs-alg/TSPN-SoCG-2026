//
// Created by Dominik Krupke on 15.01.23.
//
#include "tspn_core/utils/geometry.h"
#include "tspn_core/types.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace tspn::utils {

Box bbox(const SiteVariant &site) {
  return std::visit(
      overload{[](const Circle &circle) {
                 return Box({circle.center().get<0>() - circle.radius(),
                             circle.center().get<1>() - circle.radius()},
                            {circle.center().get<0>() + circle.radius(),
                             circle.center().get<1>() + circle.radius()});
               },
               [](const auto &geom) {
                 Box bbox;
                 bg::envelope(geom, bbox);
                 return bbox;
               }},
      site);
}

void correct(SiteVariant &site) {
  if (std::holds_alternative<Circle>(site)) {
    return;
  } else if (std::holds_alternative<Point>(site)) {
    return;
  } else if (Polygon *poly = std::get_if<Polygon>(&site)) {
    return bg::correct(*poly);
  } else if (Ring *ring = std::get_if<Ring>(&site)) {
    return bg::correct(*ring);
  } else {
    throw std::logic_error("correct does not implement this SiteVariant type.");
  }
}

template <class GeomType> bool is_convex(const GeomType &geo) {
  return bg::is_convex(geo);
}
template <> bool is_convex<Circle>(const Circle &geo) { return true; }
template <> bool is_convex<Point>(const Point &geo) { return true; }
template <> bool is_convex<SiteVariant>(const SiteVariant &geo) {
  return std::visit(
      overload{[](const auto &gt) { return tspn::utils::is_convex(gt); }}, geo);
}

SiteVariant convex_hull(const SiteVariant &site) {
  if (const Circle *circle = std::get_if<Circle>(&site)) {
    return {*circle};
  } else if (const Point *point = std::get_if<Point>(&site)) {
    return {*point};
  } else if (const Polygon *poly = std::get_if<Polygon>(&site)) {
    Ring hull;
    bg::convex_hull(*poly, hull);
    return hull;
  } else if (const Ring *ring = std::get_if<Ring>(&site)) {
    Ring hull;
    bg::convex_hull(*ring, hull);
    return hull;
  } else {
    throw std::logic_error(
        "convex_hull does not implement this SiteVariant type.");
  }
}

double distance_to_segment(std::pair<double, double> A,
                           std::pair<double, double> B,
                           std::pair<double, double> E) {
  // stolen from
  // https://www.geeksforgeeks.org/minimum-distance-from-a-point-to-the-line-segment-using-vectors/
  using namespace std;
  // vector AB
  pair<double, double> AB;
  AB.first = B.first - A.first;
  AB.second = B.second - A.second;

  // vector BP
  pair<double, double> BE;
  BE.first = E.first - B.first;
  BE.second = E.second - B.second;

  // vector AP
  pair<double, double> AE;
  AE.first = E.first - A.first, AE.second = E.second - A.second;

  // Variables to store dot product
  double AB_BE, AB_AE;

  // Calculating the dot product
  AB_BE = (AB.first * BE.first + AB.second * BE.second);
  AB_AE = (AB.first * AE.first + AB.second * AE.second);

  // Minimum distance from
  // point E to the line segment
  double reqAns = 0;

  // Case 1
  if (AB_BE > 0) {

    // Finding the magnitude
    double y = E.second - B.second;
    double x = E.first - B.first;
    reqAns = sqrt(x * x + y * y);
  }

  // Case 2
  else if (AB_AE < 0) {
    double y = E.second - A.second;
    double x = E.first - A.first;
    reqAns = sqrt(x * x + y * y);
  }

  // Case 3
  else {

    // Finding the perpendicular distance
    double x1 = AB.first;
    double y1 = AB.second;
    double x2 = AE.first;
    double y2 = AE.second;
    double mod = sqrt(x1 * x1 + y1 * y1);
    reqAns = abs(x1 * y2 - y1 * x2) / mod;
  }
  return reqAns;
};

bool equals(const Polygon &a, const Polygon &b) {
  Ring a_outer = a.outer();
  Ring b_outer = b.outer();
  if (!tspn::utils::equals(a_outer, b_outer)) {
    return false;
  }
  if (a.inners().size() != b.inners().size()) {
    return false;
  }
  std::vector<Ring> rings_a = {a.inners()};
  std::vector<Ring> rings_b = {b.inners()};
  while (!rings_a.empty()) {
    Ring ring_a = rings_a.front();
    bool found_equal = false;
    for (unsigned i = 0; i < rings_b.size(); i++) {
      Ring ring_b = rings_b[i];
      if (tspn::utils::equals(ring_a, ring_b)) {
        rings_a.erase(rings_a.begin());
        rings_b.erase(rings_b.begin() + i);
        found_equal = true;
        break;
      }
    }
    if (!found_equal) {
      return false;
    }
  }
  return true;
}

bool equals(const Ring &a, const Ring &b) {
  if (a.size() != b.size()) {
    return false;
  }
  std::vector<Point> pts_a = {a};
  std::vector<Point> pts_b = {b};
  if (bg::equals(pts_a.front(), pts_a.back())) {
    pts_a.pop_back();
  }
  if (bg::equals(pts_b.front(), pts_b.back())) {
    pts_b.pop_back();
  }
  if (pts_a.size() != pts_b.size()) {
    return false;
  }

  for (unsigned rotation = 0; rotation < b.size(); rotation++) {
    bool equal = true;
    for (unsigned pos = 0; pos < pts_a.size(); pos++) {
      if (!bg::equals(pts_b[pos], pts_a[pos])) {
        equal = false;
        break;
      }
    }
    if (equal) {
      return true;
    }
    std::rotate(pts_b.begin(), pts_b.begin() + 1, pts_b.end());
  }
  return false;
}

std::vector<Segment> to_segments(Ring &ring) {
  std::vector<Segment> segments;
  for (unsigned i = 0; i < bg::num_points(ring) - 1; i++) {
    segments.push_back(Segment(ring[i], ring[i + 1]));
  }
  return segments;
}

Ring to_ring(const std::vector<Segment> &segments) {
  Ring ring;
  for (const auto &segment : segments) {
    bg::append(ring, segment.first);
  }
  bg::append(ring, segments.back().second);
  return ring;
}

} // namespace tspn::utils
