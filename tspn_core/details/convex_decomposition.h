/**
 * decompose a multi-polygon w. holes into
 * (potentially overlapping) convex poly-rings.
 */

#ifndef CONVEX_DECOMPOSITION
#define CONVEX_DECOMPOSITION

#include "tspn_core/types.h"

namespace tspn::details {

std::vector<SiteVariant> decompose(const Polygon &poly);

}; // namespace tspn::details

#endif // CONVEX_DECOMPOSITION
