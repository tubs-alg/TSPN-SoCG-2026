#ifndef TSPN_CROPPEDVORONOIDIAGRAM_H
#define TSPN_CROPPEDVORONOIDIAGRAM_H
#include <CGAL/Arr_linear_traits_2.h>
#include <CGAL/Arrangement_2.h>
#include <CGAL/Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <cassert>
#include <iterator>
#include <limits>

#include "tspn/types.h"

namespace tspn {
class CroppedVoronoiDiagram {
public:
  using K = CGAL::Exact_predicates_exact_constructions_kernel;
  using Point_2 = K::Point_2;
  using Segment_2 = K::Segment_2;
  using Ray_2 = K::Ray_2;
  using Line_2 = K::Line_2;
  using Polygon_2 = CGAL::Polygon_2<K>;
  using Delaunay_triangulation_2 = CGAL::Delaunay_triangulation_2<K>;

  typedef CGAL::Arr_linear_traits_2<K> Traits;
  typedef CGAL::Arrangement_2<Traits> Arrangement;
  typedef Arrangement::Vertex_handle Vertex_handle;
  typedef Arrangement::Halfedge_handle Halfedge_handle;
  typedef Arrangement::Face_handle Face_handle;

  std::vector<Segment_2> m_cropped_segments;

  Arrangement arrangement;

  explicit CroppedVoronoiDiagram(const Polygon &polygon) {
    m_polygon = Polygon_2();

    // remove duplicates
    auto points = polygon.outer();
    if (points.front().get<0>() == points.back().get<0>() &&
        points.front().get<1>() == points.back().get<1>()) {
      points.pop_back();
    }

    for (auto &p : points) {
      m_polygon.push_back(Point_2(p.get<0>(), p.get<1>()));
    }
  }

  void insert(Point &p, bool recompute_voronoi = true) {
    auto p_converted = Point_2(p.get<0>(), p.get<1>());

    m_orig_sampling_points.push_back(p);
    m_sampling_points.push_back(p_converted);

    this->m_delaunay_triangulation.insert(p_converted);

    if (recompute_voronoi)
      recompute();
  }

  template <typename It>
  void insert(It begin, It end, bool recompute_voronoi = true) {
    std::vector<Point_2> points;
    for (auto it = begin; it != end; it++) {
      points.push_back(Point_2(it->template get<0>(), it->template get<1>()));
    }

    m_orig_sampling_points.insert(m_orig_sampling_points.end(), begin, end);
    m_sampling_points.insert(m_sampling_points.end(), points.begin(),
                             points.end());

    this->m_delaunay_triangulation.insert(points.begin(), points.end());

    if (recompute_voronoi)
      this->recompute();
  }

  double error_for_point(unsigned int i) const {
    assert(m_error_for_point.find(i) != m_error_for_point.end());
    return m_error_for_point.at(i);
  }

  void recompute() {
    m_cropped_segments.clear();
    m_delaunay_triangulation.draw_dual(*this);

    arrangement.clear();
    CGAL::insert(arrangement, m_polygon.edges_begin(), m_polygon.edges_end());
    CGAL::insert(arrangement, m_cropped_segments.begin(),
                 m_cropped_segments.end());

    compute_errors();
  }

  std::vector<std::pair<Point, Point>> arrangement_edges() const {
    std::vector<std::pair<Point, Point>> edges;
    for (auto eit = arrangement.edges_begin(); eit != arrangement.edges_end();
         ++eit) {
      auto source = eit->source()->point();
      auto target = eit->target()->point();
      edges.emplace_back(
          Point{CGAL::to_double(source.x()), CGAL::to_double(source.y())},
          Point{CGAL::to_double(target.x()), CGAL::to_double(target.y())});
    }

    return edges;
  }

  auto get_point(auto i) const {
    if (i < 0 || i >= m_orig_sampling_points.size()) {
      throw std::out_of_range("Index out of range for sampling points.");
    }

    return m_orig_sampling_points[i];
  }

  auto n_sampling_points() const { return m_orig_sampling_points.size(); }

  void operator<<(const Ray_2 &ray) { crop_and_extract_segment(ray); }
  void operator<<(const Line_2 &line) { crop_and_extract_segment(line); }
  void operator<<(const Segment_2 &seg) { crop_and_extract_segment(seg); }

private:
  Delaunay_triangulation_2 m_delaunay_triangulation;
  Polygon_2 m_polygon;

  std::vector<Point> m_orig_sampling_points; // we use that to get a consistent
                                             // mapping from the input
  std::vector<Point_2> m_sampling_points;
  std::map<unsigned int, double> m_error_for_point;

  template <class RSL> void crop_and_extract_segment(const RSL &rsl) {
    std::vector<Point_2> points;

    for (auto edge_it = m_polygon.edges_begin();
         edge_it != m_polygon.edges_end(); ++edge_it) {
      CGAL::Object result = CGAL::intersection(rsl, *edge_it);
      if (!result)
        continue;

      if (const auto p = CGAL::object_cast<Point_2>(&result)) {
        points.push_back(*p);
      } else if (const auto s = CGAL::object_cast<Segment_2>(&result)) {
        points.push_back(s->source());
        points.push_back(s->target());
      }
    }

    if constexpr (std::is_same<RSL, Segment_2>::value) {
      if (m_polygon.bounded_side(rsl.source()) != CGAL::ON_UNBOUNDED_SIDE)
        points.push_back(rsl.source());
      if (m_polygon.bounded_side(rsl.target()) != CGAL::ON_UNBOUNDED_SIDE)
        points.push_back(rsl.target());
    } else if constexpr (std::is_same<RSL, Ray_2>::value) {
      if (m_polygon.bounded_side(rsl.source()) != CGAL::ON_UNBOUNDED_SIDE)
        points.push_back(rsl.source());
    }

    std::sort(points.begin(), points.end());
    points.erase(std::unique(points.begin(), points.end()), points.end());

    if (points.size() < 2)
      return;

    auto dir_vec = rsl.to_vector();
    Point_2 origin = rsl.point(0);
    std::sort(points.begin(), points.end(),
              [&](const Point_2 &a, const Point_2 &b) {
                return dir_vec * (a - origin) < dir_vec * (b - origin);
              });

    for (size_t i = 0; i + 1 < points.size(); ++i) {
      Point_2 mid = CGAL::midpoint(points[i], points[i + 1]);
      if (m_polygon.bounded_side(mid) == CGAL::ON_BOUNDED_SIDE) {
        m_cropped_segments.emplace_back(points[i], points[i + 1]);
      }
    }
  }

  void compute_errors() {
    assert(!m_orig_sampling_points.empty());
    assert(m_orig_sampling_points.size() == m_sampling_points.size());

    for (unsigned int i = 0; i < m_orig_sampling_points.size(); i++) {
      m_error_for_point[i] = 0;
    }

    auto unbounded_face = arrangement.unbounded_face();
    // Iterate over outer CCBs of the unbounded face
    for (auto ccb = unbounded_face->inner_ccbs_begin();
         ccb != unbounded_face->inner_ccbs_end(); ++ccb) {
      auto curr = *ccb;
      auto start = curr;
      do {
        auto vit = curr->source();
        const auto &p = vit->point();
        auto min_dist = CGAL::squared_distance(p, *m_sampling_points.begin());

        std::vector<unsigned int> incident_points = {0};

        for (unsigned i = 1; i < m_orig_sampling_points.size(); i++) {
          auto current_dist = CGAL::squared_distance(p, m_sampling_points[i]);

          if (min_dist > current_dist) {
            incident_points.clear();
            incident_points.push_back(i);
            min_dist = current_dist;
          } else if (min_dist == current_dist) {
            incident_points.push_back(i);
          }
        }

        assert(!incident_points.empty());

        double vertex_error =
            2 *
            std::sqrt(CGAL::to_double(
                min_dist)); // eventually we need to error bound this operation.

        for (auto &i : incident_points) {
          m_error_for_point[i] = std::max(m_error_for_point[i], vertex_error);
        }

        ++curr;
      } while (curr != start);
    }
  }
};

TEST_CASE("Voronoi of rectangle with errors") {
  std::vector<Point> points;
  points.push_back(Point(0, 0));
  points.push_back(Point(1, 1));
  points.push_back(Point(0, 1));

  auto bbox = Polygon{{{-1, -1}, {-1, 2}, {2, 2}, {2, -1}}};

  tspn::CroppedVoronoiDiagram vor(bbox);
  vor.insert(points.begin(), points.end());
  vor.recompute();

  CHECK(vor.arrangement.number_of_edges() == 9);
  CHECK(vor.arrangement.number_of_faces() == 4);
  CHECK(vor.arrangement.number_of_vertices() == 7);

  CHECK(doctest::Approx(vor.error_for_point(0)) == 4.47214);
  CHECK(doctest::Approx(vor.error_for_point(1)) == 4.47214);
  CHECK(doctest::Approx(vor.error_for_point(2)) == 2.82843);
}

TEST_CASE("Voronoi of polygon") {
  std::vector<Point> points;
  points.push_back(Point(0, 0));
  points.push_back(Point(1, 1));
  points.push_back(Point(0, 1));

  auto polygon = Polygon{{
      {-1, -1},
      {-1, 2},
      {2, 3},
      {3, 3},
      {5, -2},
      {4, -1},
  }};

  tspn::CroppedVoronoiDiagram vor(polygon);
  vor.insert(points.begin(), points.end());
  vor.recompute();

  for (auto vit = vor.arrangement.edges_begin();
       vit != vor.arrangement.edges_end(); ++vit) {
    std::cout << vit->source()->point() << " " << vit->target()->point()
              << std::endl;
  }

  for (unsigned int i = 0; i < points.size(); i++) {
    std::cout << i << ": " << vor.error_for_point(i) << std::endl;
  }

  CHECK(vor.arrangement.number_of_edges() == 12);
  CHECK(vor.arrangement.number_of_faces() == 4);
  CHECK(vor.arrangement.number_of_vertices() == 10);

  CHECK(doctest::Approx(vor.error_for_point(0)) == 4.47214);
  CHECK(doctest::Approx(vor.error_for_point(1)) == 10);
  CHECK(doctest::Approx(vor.error_for_point(2)) == 3.16228);
}
} // namespace tspn

#endif // TSPN_CROPPEDVORONOIDIAGRAM_H
