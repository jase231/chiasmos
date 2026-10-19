#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/error.hpp"

// orchestrates the offline pipeline building a chiasmos idx from OSM data:
// ingests OSM ways -> finds vertices -> normalizes their names ->
// detect whether an intersection exists at each vertex -> classify junctions ->
// index::Draft -> write to file

namespace chiasmos::build {

struct Options {
  std::string region;         // stored in the index header, eg "us/virginia"
  bool include_paths = false; // also keep named footways, cycleways and paths
};

struct Stats {
  std::uint64_t road_ways = 0, vertices = 0, junctions = 0, roundabouts = 0,
                full_names = 0;
  double seconds = 0;
};

[[nodiscard]] Result<Stats> build_index(std::filesystem::path const& pbf,
                                        std::filesystem::path const& out,
                                        Options const& opt);

} // namespace chiasmos::build
