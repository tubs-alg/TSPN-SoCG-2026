#include "tspn_core/utils/drawer.h"
#include "tspn_core/common.h"
#include "tspn_core/types.h"
#include <boost/geometry.hpp>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

namespace bg = boost::geometry;

namespace tspn::utils {

struct HSVcolor {
  float H; // 0 to 360
  float S; // 0 to 1
  float V; // 0 to 1
};

struct RGBcolor {
  int R;
  int G;
  int B;
};

std::string rgbstring(struct RGBcolor &rgb) {
  return std::to_string(rgb.R) + std::string(",") + std::to_string(rgb.G) +
         std::string(",") + std::to_string(rgb.B);
}

struct RGBcolor hsvrgb(struct HSVcolor &hsv) {
  // logic from: https://www.codespeedy.com/hsv-to-rgb-in-cpp/
  float C = hsv.S * hsv.V;
  float X = C * (1 - abs(fmod(hsv.H / 60.0, 2) - 1));
  float m = hsv.V - C;
  float r, g, b;
  if (hsv.H >= 0 && hsv.H < 60) {
    r = C, g = X, b = 0;
  } else if (hsv.H >= 60 && hsv.H < 120) {
    r = X, g = C, b = 0;
  } else if (hsv.H >= 120 && hsv.H < 180) {
    r = 0, g = C, b = X;
  } else if (hsv.H >= 180 && hsv.H < 240) {
    r = 0, g = X, b = C;
  } else if (hsv.H >= 240 && hsv.H < 300) {
    r = X, g = 0, b = C;
  } else {
    r = C, g = 0, b = X;
  }
  struct RGBcolor rgb;
  rgb.R = (r + m) * 255;
  rgb.G = (g + m) * 255;
  rgb.B = (b + m) * 255;
  return rgb;
}

void draw_geometries(const std::string &filename,
                     const tspn::Instance &instance,
                     const tspn::Trajectory &trajectory) {
  std::ofstream svg(filename);
  bg::svg_mapper<tspn::Point> mapper(svg, 1000, 1000,
                                     "width=\"110%\" height=\"110%\"");

  // adjust the map size - seems to be a bit buggy on boost's side..
  // colorcoding polys with max distance
  float hue_step = 360.0f / instance.size();
  float current_hue = 0;
  for (const auto &geometry : instance) {
    struct HSVcolor hsv{current_hue, 1, 1};
    current_hue = current_hue + hue_step;
    struct RGBcolor rgb = hsvrgb(hsv);
    std::string color = rgbstring(rgb);
    std::string fmtstyle = std::format("fill-opacity:0.3;"
                                       "fill:rgb({});"
                                       "stroke:rgb({});"
                                       "stroke-width:2;",
                                       color, color);
    SiteVariant site = geometry.definition();
    if (const Circle *circle = std::get_if<Circle>(&site)) {
      MultiPolygon result;
      int points_per_circle = 64;
      bg::strategy::buffer::distance_symmetric<double> distance_strategy(
          circle->radius());
      bg::strategy::buffer::point_circle circle_strategy(points_per_circle);
      bg::strategy::buffer::join_round join_strategy(points_per_circle);
      bg::strategy::buffer::end_round end_strategy(points_per_circle);
      bg::strategy::buffer::side_straight side_strategy;
      bg::buffer(circle->center(), result, distance_strategy, side_strategy,
                 join_strategy, end_strategy, circle_strategy);
      mapper.add(result);
      mapper.map(result, fmtstyle);
    } else if (const Point *point = std::get_if<Point>(&site)) {
      mapper.add(*point);
      mapper.map(*point, fmtstyle);
    } else if (const Polygon *poly = std::get_if<Polygon>(&site)) {
      mapper.add(*poly);
      mapper.map(*poly, fmtstyle);
    } else if (const Ring *ring = std::get_if<Ring>(&site)) {
      mapper.add(*ring);
      mapper.map(*ring, fmtstyle);
    } else {
      throw std::logic_error(
          "draw_geometries does not implement this SiteVariant type.");
    }
    mapper.add(trajectory.points);
    mapper.map(trajectory.points, "opacity:0.9;"
                                  "fill:none;"
                                  "stroke:black;"
                                  "stroke-width:5;");
  }
}

} // namespace tspn::utils
