#pragma once

#include <vector>

#include "build/pairs.hpp"

namespace chiasmos::build {

// appends the roundabout junctions of every ring
std::size_t add_roundabout_junctions(RoadWays const& ways, Graph const& graph,
                                     PairRule& rule,
                                     std::vector<index::Draft::Junction>& out);

} // namespace chiasmos::build
