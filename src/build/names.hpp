#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "build/ingest.hpp"
#include "index/draft.hpp"

namespace chiasmos::build {

// a normalized name carried by a way
struct NameKey {
  index::Draft::FullId full = 0;
  NameKind kind = NameKind::Primary;
};

// stores which unique, normalized names any given way carries
struct WayNames {
  std::vector<std::size_t> off{ 0 };
  std::vector<NameKey> keys;

  [[nodiscard]] std::span<NameKey const> of(std::size_t way) const {
    return std::span<NameKey const>(keys).subspan(off[way],
                                                  off[way + 1] - off[way]);
  }
};

// normalizes each distinct name once
[[nodiscard]] WayNames build_names(RoadWays const& ways, index::Draft& draft);

} // namespace chiasmos::build
