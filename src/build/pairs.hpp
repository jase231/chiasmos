#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "build/graph.hpp"
#include "build/names.hpp"

namespace chiasmos::build {

inline constexpr std::size_t kMaxWays = 64; // one bit per way in a 64-bit mask

struct Place {
  Coord at{};
  // node ID of the node in question, or if (ring) then it is node ID of
  // centermost node
  std::int64_t node = 0;
  bool ring = false; // a roundabout
};

// every road name at a place is assigned a mask representing the incident ways
// carrying it. two road names at a place form a junction if they continue on a
// way the other doesn't, ie. if their masks contain unique bits that arent
// subsets of each other
class PairRule {
public:
  PairRule(RoadWays const& ways, WayNames const& names)
      : ways_(ways), names_(names) {}

  // appends to out the junctions formed between names that meet at place
  // every Junction is normalized such that Junction.a < Junction.b
  void at(std::span<Incident const> incident, Place const& place,
          std::vector<index::Draft::Junction>& out);

private:
  // a specific name, carried by one or more ways at a junction.
  // each set bit represents one way that is labeled with that name
  struct Mask {
    index::Draft::FullId full;
    std::uint64_t ways; // bit i: kept_[i] carries this name
    NameKind kind;      // best kind among those ways
  };
  // best class of a name carried by one or more ways at a junction
  [[nodiscard]] RoadClass best_class(std::uint64_t mask) const;

  RoadWays const& ways_;
  WayNames const& names_;
  std::vector<Incident> kept_;
  std::vector<Mask> masks_;
};

} // namespace chiasmos::build
