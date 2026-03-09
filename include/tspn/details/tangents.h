#pragma once

#include "tspn/types.h"

namespace tspn::details {

std::tuple<bool, std::array<Segment, 2>> find_tangents(const Ring &hulla,
                                                       const Ring &hullb);
class TangentResult {
public:
  bool is_valid;
  Segment upper;
  Segment lower;
};
}; // namespace tspn::details
