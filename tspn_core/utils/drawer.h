#ifndef TSPN_DRAWER_H
#define TSPN_DRAWER_H

#include "tspn_core/common.h"
#include <boost/geometry.hpp>

namespace bg = boost::geometry;

namespace tspn::utils {

void draw_geometries(const std::string &filename,
                     const tspn::Instance &instance,
                     const tspn::Trajectory &trajectory);
} // namespace tspn::utils

#endif // TSPN_DRAWER_H
