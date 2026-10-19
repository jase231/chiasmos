#pragma once

#include <optional>

#include <osmium/osm/tag.hpp>

#include "core/types.hpp"

namespace chiasmos::build {

struct RoadAttrs {
  RoadClass cls = RoadClass::Residential;
  bool roundabout = false;
};

// nullopt unless the way is a road to keep. keep named footways, cycleways and
// paths if (include_paths)
[[nodiscard]] std::optional<RoadAttrs>
classify_road(osmium::TagList const& tags, bool include_paths);

} // namespace chiasmos::build
