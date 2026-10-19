#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "build/build_index.hpp"
#include "build/road_filter.hpp"
#include "core/error.hpp"
#include "core/types.hpp"

namespace chiasmos::build {

struct NodeRef {
  std::int64_t id = 0;
  Coord at{};
};

struct RawName {
  std::uint32_t str = 0; // idx into RoadWays::strings
  NameKind kind = NameKind::Primary;
};

// one entry per road way
struct RoadWays {
  std::vector<std::int64_t> osm_id;
  std::vector<RoadAttrs> attrs;
  std::vector<std::size_t> node_off, name_off;
  std::vector<NodeRef> nodes;
  std::vector<RawName> names;
  std::deque<std::string> strings;

  [[nodiscard]] std::size_t size() const { return osm_id.size(); }
  [[nodiscard]] std::span<NodeRef const> nodes_of(std::size_t way) const {
    return span_of(nodes, node_off, way);
  }
  [[nodiscard]] std::span<RawName const> names_of(std::size_t way) const {
    return span_of(names, name_off, way);
  }

private:
  template <class T>
  static std::span<T const> span_of(std::vector<T> const& v,
                                    std::vector<std::size_t> const& off,
                                    std::size_t way) {
    return std::span<T const>(v).subspan(off[way], off[way + 1] - off[way]);
  }
};

struct Ingested {
  RoadWays ways;
  std::string data_timestamp;
};

[[nodiscard]] Result<Ingested> ingest_roads(std::filesystem::path const& pbf,
                                            Options const& opt);

} // namespace chiasmos::build
