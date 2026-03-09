#pragma once
#include "tspn/types.h"
#include "tspn/utils/geometry.h"
#include <boost/geometry.hpp>

/** Distance Calls:
 * Point - Point
 * Segment - Point
 * Linestring - Point
 * Circle - Point
 * Ring - Point
 * Polygon - Point
 * Point - Segment
 * Segment - Segment
 * Linestring - Segment
 * Circle - Segment
 * Ring - Segment
 * Polygon - Segment
 * Point - Circle
 * Segment - Circle
 * Linestring - Circle
 * Circle - Circle
 * Ring - Circle
 * Polygon - Circle
 * Point - Ring
 * Segment - Ring
 * Linestring - Ring
 * Circle - Ring
 * Ring - Ring
 * Polygon - Ring
 * Point - Polygon
 * Segment - Polygon
 * Linestring - Polygon
 * Circle - Polygon
 * Ring - Polygon
 * Polygon - Polygon
 * SiteVariant - SiteVariant
 * Point - SiteVariant
 * Segment - SiteVariant
 * Linestring - SiteVariant
 * Circle - SiteVariant
 * Ring - SiteVariant
 * Polygon - SiteVariant
 */

namespace tspn::utils {

// forward declaration
// template <typename G1, typename G2> double distance(G1 const &g1, G2 const
// &g2);

// primary: pass into boost
template <typename G1, typename G2>
inline double distance(G1 const &g1, G2 const &g2) {
  return bg::distance(g1, g2);
}

// Geom-Circle and Circle-Geom
template <>
inline double distance<Circle, Point>(Circle const &c, Point const &p) {
  return std::max(bg::distance(c.center(), p) - c.radius(), 0.0);
}
template <>
inline double distance<Point, Circle>(Point const &p, Circle const &c) {
  return std::max(bg::distance(p, c.center()) - c.radius(), 0.0);
}
template <>
inline double distance<Circle, Segment>(Circle const &c, Segment const &s) {
  return std::max(bg::distance(c.center(), s) - c.radius(), 0.0);
}
template <>
inline double distance<Segment, Circle>(Segment const &s, Circle const &c) {
  return std::max(bg::distance(s, c.center()) - c.radius(), 0.0);
}
template <>
inline double distance<Circle, Linestring>(Circle const &c,
                                           Linestring const &ls) {
  return std::max(bg::distance(c.center(), ls) - c.radius(), 0.0);
}
template <>
inline double distance<Linestring, Circle>(Linestring const &ls,
                                           Circle const &c) {
  return std::max(bg::distance(ls, c.center()) - c.radius(), 0.0);
}
template <>
inline double distance<Circle, Ring>(Circle const &c, Ring const &r) {
  return std::max(bg::distance(c.center(), r) - c.radius(), 0.0);
}
template <>
inline double distance<Ring, Circle>(Ring const &r, Circle const &c) {
  return std::max(bg::distance(r, c.center()) - c.radius(), 0.0);
}
template <>
inline double distance<Circle, Polygon>(Circle const &c, Polygon const &poly) {
  return std::max(bg::distance(c.center(), poly) - c.radius(), 0.0);
}
template <>
inline double distance<Polygon, Circle>(Polygon const &poly, Circle const &c) {
  return std::max(bg::distance(poly, c.center()) - c.radius(), 0.0);
}
template <>
inline double distance<Circle, Circle>(Circle const &c1, Circle const &c2) {
  return std::max(
      bg::distance(c1.center(), c2.center()) - c1.radius() - c2.radius(), 0.0);
}

// SiteVariant - SiteVariant: dispatch
template <>
inline double distance<SiteVariant, SiteVariant>(SiteVariant const &v1,
                                                 SiteVariant const &v2) {
  return std::visit([](auto const &a, auto const &b) { return distance(a, b); },
                    v1, v2);
}

// Geom -> SiteVariant: dispatch
template <>
inline double distance<Point, SiteVariant>(Point const &g,
                                           SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}
template <>
inline double distance<Segment, SiteVariant>(Segment const &g,
                                             SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}
template <>
inline double distance<Linestring, SiteVariant>(Linestring const &g,
                                                SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}
template <>
inline double distance<Circle, SiteVariant>(Circle const &g,
                                            SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}
template <>
inline double distance<Ring, SiteVariant>(Ring const &g, SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}
template <>
inline double distance<Polygon, SiteVariant>(Polygon const &g,
                                             SiteVariant const &v) {
  return std::visit([&](auto const &h) { return distance(g, h); }, v);
}

// SiteVariant -> Geom: dispatch
template <>
inline double distance<SiteVariant, Point>(SiteVariant const &v,
                                           Point const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}
template <>
inline double distance<SiteVariant, Segment>(SiteVariant const &v,
                                             Segment const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}
template <>
inline double distance<SiteVariant, Linestring>(SiteVariant const &v,
                                                Linestring const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}
template <>
inline double distance<SiteVariant, Circle>(SiteVariant const &v,
                                            Circle const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}
template <>
inline double distance<SiteVariant, Ring>(SiteVariant const &v, Ring const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}
template <>
inline double distance<SiteVariant, Polygon>(SiteVariant const &v,
                                             Polygon const &g) {
  return std::visit([&](auto const &h) { return distance(h, g); }, v);
}

} // namespace tspn::utils
