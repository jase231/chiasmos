#include "build/pairs.hpp"

#include <algorithm>
#include <bit>
#include <tuple>

namespace chiasmos::build {

using index::Draft;

RoadClass PairRule::best_class(std::uint64_t mask) const {
  RoadClass best = RoadClass::Path; // the least important class, highest value
  for (; mask != 0; mask &= mask - 1)
    best = std::min(
        best,
        ways_.attrs[kept_[static_cast<std::size_t>(std::countr_zero(mask))].way]
            .cls);
  return best;
}

void PairRule::at(std::span<Incident const> incident, Place const& place,
                  std::vector<Draft::Junction>& out) {
  if (incident.size() < 2)
    return;
  kept_.assign(incident.begin(), incident.end());
  auto const named = [&](Incident const& i) {
    return !names_.of(i.way).empty();
  };
  if (kept_.size() > kMaxWays)
      [[unlikely]] { // keep the named ways, longest first
    auto const length = [&](Incident const& i) {
      return ways_.node_off[i.way + 1] - ways_.node_off[i.way];
    };
    std::ranges::sort(kept_, [&](Incident const& a, Incident const& b) {
      // length(b) first so longer sorts
      return std::tuple(!named(a), length(b), a.way) <
             std::tuple(!named(b), length(a), b.way);
    });
    kept_.resize(kMaxWays);
  }

  masks_.clear();
  // way passing through a place contributes 2 arms, a way ending here 1
  std::uint32_t named_arms = 0;
  // initialize mask for every name distinct name carried by every way in kept_
  for (std::size_t i = 0; i < kept_.size(); ++i) {
    if (named(kept_[i]))
      named_arms += kept_[i].arms;
    for (NameKey const& key : names_.of(kept_[i].way)) {
      auto mask = std::ranges::find(masks_, key.full, &Mask::full);
      if (mask == masks_.end())
        mask = masks_.emplace(masks_.end(), Mask{ key.full, 0, key.kind });
      mask->ways |= std::uint64_t{ 1 } << i;
      mask->kind = std::min(
          mask->kind, key.kind); // promote the kind if better option exists
    }
  }

  // for each pair where mask check passes, create Junction
  for (std::size_t i = 0; i < masks_.size(); ++i) {
    for (std::size_t j = i + 1; j < masks_.size(); ++j) {
      Mask a = masks_[i], b = masks_[j];
      if ((a.ways & ~b.ways) == 0 || (b.ways & ~a.ways) == 0)
        continue; // aliases, or one name inside the other's footprint
      if (b.full < a.full)
        std::swap(a, b);
      Draft::Junction junction{
        .a = a.full, .b = b.full, .rep = place.at, .osm_node = place.node
      };
      if ((a.ways & b.ways) != 0)
        junction.kind = JunctionKind::Concurrency;
      else if (place.ring)
        junction.kind = JunctionKind::Roundabout;
      else if (named_arms == 2 && std::has_single_bit(a.ways) &&
               std::has_single_bit(b.ways))
        junction.kind = JunctionKind::NameTransition;
      junction.class_a = best_class(a.ways),
      junction.class_b = best_class(b.ways);
      junction.name_kind_a = a.kind, junction.name_kind_b = b.kind;
      out.push_back(junction);
    }
  }
}

} // namespace chiasmos::build
