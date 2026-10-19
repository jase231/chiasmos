#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "build/ingest.hpp"

namespace chiasmos::build {

// a way incident to (touching) a vertex
// arms = how many segments come in and out of the vertex
struct Incident {
  std::uint32_t way = 0, arms = 0;
};

struct Graph {
  // OSM id of each vertex
  std::vector<std::int64_t> node;
  std::vector<Coord> at;
  // size()+1 offsets into `incident`
  std::vector<std::size_t> off;
  // distinct ways of each vertex, ascending by way
  std::vector<Incident> incident;

  [[nodiscard]] std::size_t size() const { return node.size(); }
  [[nodiscard]] std::span<Incident const> incident_at(std::size_t v) const {
    return std::span<Incident const>(incident).subspan(off[v],
                                                       off[v + 1] - off[v]);
  }
};

[[nodiscard]] Graph build_graph(RoadWays const& ways);

} // namespace chiasmos::build
