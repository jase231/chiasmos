#include "build/names.hpp"

#include <algorithm>
#include <numeric>
#include <ranges>
#include <tuple>

#include "normalize/normalizer.hpp"

namespace chiasmos::build {

WayNames build_names(RoadWays const& ways, index::Draft& draft) {
  auto const& strings = ways.strings;
  std::vector<norm::Normalized> normalized(strings.size());
  std::vector<std::uint32_t> uses(strings.size(), 0);

  for (std::size_t s = 0; s < strings.size(); ++s)
    norm::normalize_street(strings[s], normalized[s]);
  for (RawName const& n : ways.names)
    ++uses[n.str];

  // ways.strings contains only unique names, but spelling variations still
  // exist, and we need one authoritative display variation. the most common
  // variation is used.
  std::vector<std::uint32_t> spellings;
  spellings.reserve(strings.size());
  for (std::uint32_t s = 0; s < strings.size(); ++s)
    if (!normalized[s].empty())
      spellings.push_back(s);
  // each bunch is sorted in descending order according to use
  std::ranges::sort(spellings, [&](std::uint32_t a, std::uint32_t b) {
    return std::tie(normalized[a].full, uses[b], strings[a]) <
           std::tie(normalized[b].full, uses[a], strings[b]);
  });
  std::vector<index::Draft::FullId> full_of(strings.size());
  auto const same_full_name = [&](std::uint32_t a, std::uint32_t b) {
    return normalized[a].full == normalized[b].full;
  };
  for (auto const same_full :
       spellings | std::views::chunk_by(same_full_name)) {
    std::uint32_t const display = same_full.front();
    index::Draft::FullId const id =
        draft.add_full(normalized[display], strings[display]);
    for (std::uint32_t const s : same_full)
      full_of[s] = id;
  }

  WayNames names;
  std::vector<NameKey> keys;
  for (std::size_t w = 0; w < ways.size(); ++w) {
    keys.clear();
    for (RawName const& n : ways.names_of(w))
      if (!normalized[n.str].empty())
        keys.push_back({ full_of[n.str], n.kind });
    std::ranges::sort(
        keys, {}, [](NameKey const& k) { return std::pair(k.full, k.kind); });

    // keep only the best NameKey per full (prefer Primary over Alt or Old)
    keys.erase(std::ranges::unique(keys, {}, &NameKey::full).begin(),
               keys.end());
    names.keys.insert(names.keys.end(), keys.begin(), keys.end());
    names.off.push_back(names.keys.size());
  }
  return names;
}

} // namespace chiasmos::build
