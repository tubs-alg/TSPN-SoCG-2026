/**
 * partition a multi-polygon w. holes into convex poly-rings.
 */

#ifndef CONVEX_PARTITION
#define CONVEX_PARTITION

#include "tspn_core/types.h"

namespace tspn::details {

std::vector<SiteVariant> partition(const SiteVariant &site);

}; // namespace tspn::details

#endif // CONVEX_PARTITION
