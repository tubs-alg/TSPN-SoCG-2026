/**
 * simplify an instance pre-compute
 */

#ifndef PRE_SIMPLIFY
#define PRE_SIMPLIFY

#include "tspn_core/types.h"

namespace tspn::details {

std::vector<SiteVariant> simplify(std::vector<SiteVariant> &sites);

void remove_holes(std::vector<SiteVariant> &sites);
void convex_hull_fill(std::vector<SiteVariant> &sites);
void remove_supersites(std::vector<SiteVariant> &sites);
}; // namespace tspn::details

#endif // PRE_SIMPLIFY
