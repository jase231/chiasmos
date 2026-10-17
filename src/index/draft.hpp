#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/error.hpp"
#include "core/types.hpp"
#include "normalize/normalizer.hpp"

namespace chiasmos::index {

struct Meta {
  std::string region{};         // eg. "us/virginia"
  std::string data_timestamp{}; // OSM extract timestamp
  std::string json{};           // build statistics
};

class Draft {
public:
  using FullId = std::uint32_t;

  struct Junction {
    // two names, unordered
    FullId a = 0, b = 0;
    Coord rep{};
    std::int64_t osm_node = 0;
    JunctionKind kind = JunctionKind::Node;
    // best class carrying a and b
    RoadClass class_a = RoadClass::Residential,
              class_b = RoadClass::Residential;
    NameKind name_kind_a = NameKind::Primary, name_kind_b = NameKind::Primary;
  };

  // uses full name to prevent duplicate fulls_ entries, then interns the string
  // and its components into fulls_ if not seen before
  FullId add_full(norm::Normalized const& n, std::string_view display);
  void add_junction(Junction const& j) { junctions_.push_back(j); }

  [[nodiscard]] std::size_t full_count() const { return fulls_.size(); }
  [[nodiscard]] std::size_t junction_count() const { return junctions_.size(); }

  // sorts Draft vectors for canonical order, then writes to out
  [[nodiscard]] Result<void> write(std::filesystem::path const& out,
                                   Meta const& meta) const;

private:
  struct Entry {
    std::string full, core, display;
    norm::Components comp;
  };
  std::unordered_map<std::string, FullId> ids_;
  std::vector<Entry> fulls_;
  std::vector<Junction> junctions_;
};

} // namespace chiasmos::index
