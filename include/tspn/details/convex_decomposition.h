/**
 * decompose a multi-polygon w. holes into
 * (potentially overlapping) convex poly-rings.
 */

#ifndef CONVEX_DECOMPOSITION
#define CONVEX_DECOMPOSITION

#include "tspn/types.h"

namespace tspn::details {

std::vector<SiteVariant> decompose(const Polygon &poly);

void triangulate(std::vector<Polygon> &polys,
                 std::map<Polygon *, std::vector<Polygon *>> &neighbor_map,
                 Polygon &poly);

}; // namespace tspn::details

#endif // CONVEX_DECOMPOSITION
