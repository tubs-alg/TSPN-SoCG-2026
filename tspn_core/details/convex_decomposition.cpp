#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Constrained_triangulation_plus_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <boost/geometry.hpp>
#include <boost/geometry/algorithms/is_convex.hpp>
#include <list>
#include <ranges>

#include "tspn_core/details/cgal_conversion.h"
#include "tspn_core/details/convex_decomposition.h"
#include "tspn_core/utils/geometry.h"

namespace bg = boost::geometry;

// triangulation following:
// https://doc.cgal.org/latest/Triangulation_2/index.html
struct FaceInfo2 {
  FaceInfo2() {}
  int nesting_level;
  bool in_domain() { return nesting_level % 2 == 1; }
};
typedef CGAL::Triangulation_vertex_base_2<tspn::details::K> Vb;
typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo2, tspn::details::K>
    Fbb;
typedef CGAL::Constrained_triangulation_face_base_2<tspn::details::K, Fbb> Fb;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
typedef CGAL::Exact_predicates_tag Itag;
typedef CGAL::Constrained_Delaunay_triangulation_2<tspn::details::K, TDS, Itag>
    CDT;
typedef CDT::Face_handle Face_handle;

namespace tspn::details {

void _mark_domains(CDT &ct, Face_handle start, int index,
                   std::list<CDT::Edge> &border) {
  if (start->info().nesting_level != -1) {
    return;
  }
  std::list<Face_handle> queue;
  queue.push_back(start);
  while (!queue.empty()) {
    Face_handle fh = queue.front();
    queue.pop_front();
    if (fh->info().nesting_level == -1) {
      fh->info().nesting_level = index;
      for (int i = 0; i < 3; i++) {
        CDT::Edge e(fh, i);
        Face_handle n = fh->neighbor(i);
        if (n->info().nesting_level == -1) {
          if (ct.is_constrained(e))
            border.push_back(e);
          else
            queue.push_back(n);
        }
      }
    }
  }
}

void _mark_domains(CDT &cdt) {
  for (CDT::Face_handle f : cdt.all_face_handles()) {
    f->info().nesting_level = -1;
  }
  std::list<CDT::Edge> border;
  _mark_domains(cdt, cdt.infinite_face(), 0, border);
  while (!border.empty()) {
    CDT::Edge e = border.front();
    border.pop_front();
    Face_handle n = e.first->neighbor(e.second);
    if (n->info().nesting_level == -1) {
      _mark_domains(cdt, n, e.first->info().nesting_level + 1, border);
    }
  }
}

CGAL_Polygon fh_to_cgalpol(Face_handle face_handle) {
  CGAL_Polygon poly;
  poly.push_back(face_handle->vertex(0)->point());
  poly.push_back(face_handle->vertex(1)->point());
  poly.push_back(face_handle->vertex(2)->point());
  return poly;
}

std::vector<SiteVariant> decompose(const Polygon &poly) {
  // to be called ONLY (!) for polygons which do have holes.
  std::vector<SiteVariant> results;
  CDT cdt;
  std::vector<Ring> borders = {poly.outer()};
  for (const auto &inner : poly.inners()) {
    borders.push_back(inner);
  }
  for (const auto &border : borders) {
    std::vector<CGAL_Point> vertices;
    for (unsigned i = 0; i < border.size() - 1; i++) {
      Point bs = border[i];
      Point bt = border[i + 1];
      CGAL_Point csource = CGAL_Point(bs.get<0>(), bs.get<1>());
      CGAL_Point ctarget = CGAL_Point(bt.get<0>(), bt.get<1>());
      cdt.insert_constraint(csource, ctarget);
    }
  }
  assert(cdt.is_valid());
  _mark_domains(cdt);

  std::vector<Face_handle> uncovered;
  for (const auto &face : cdt.finite_face_handles()) {
    if (face->info().in_domain()) {
      uncovered.push_back(face);
    }
  }
  while (!uncovered.empty()) {
    Face_handle start = uncovered.front();
    std::set<Face_handle> poly_faces = {start};
    std::list<Face_handle> neighbors = {
        start->neighbor(0),
        start->neighbor(1),
        start->neighbor(2),
    };
    CGAL_Polygon convex = fh_to_cgalpol(start);
    CGAL_Polygon candidate;
    CGAL_Polygon_wh trial;
    while (!neighbors.empty()) {
      Face_handle neighbor = neighbors.front();
      neighbors.pop_front();
      if (neighbor->info().in_domain()) {
        if (!poly_faces.contains(neighbor)) {
          candidate = fh_to_cgalpol(neighbor);
          CGAL::join(convex, candidate, trial);
          if (trial.has_holes() == false &&
              trial.outer_boundary().is_convex()) {
            convex = trial.outer_boundary();
            poly_faces.insert(neighbor);
            neighbors.push_back(neighbor->neighbor(0));
            neighbors.push_back(neighbor->neighbor(1));
            neighbors.push_back(neighbor->neighbor(2));
          }
        }
      }
    }
    for (const auto &face : poly_faces) {
      uncovered.erase(std::remove(uncovered.begin(), uncovered.end(), face),
                      uncovered.end());
    }
    results.push_back({cgalpoly_to_ring(convex)});
  }
  return results;
}
}; // namespace tspn::details
