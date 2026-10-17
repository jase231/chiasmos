#pragma once

#include <span>
#include <vector>

#include "index/index.hpp"
#include "normalize/normalizer.hpp"

namespace chiasmos::query {

struct NameCand {
  std::uint32_t core = 0;
  float prior = 1.0f;
  // query's components, compared against index record by name_agreement()
  norm::Components comp;
  // whether suffixes should strictly match
  std::uint16_t require_type = 0;
};

[[nodiscard]] std::vector<NameCand>
name_candidates(index::Index const& ix, norm::Normalized const& street);

// if suffix is trimmed, force require_type to be the trimmed suffix' type
[[nodiscard]] inline bool meets_type_requirement(NameCand const& c,
                                                 index::FullRec const& r) {
  return c.require_type == 0 ||
         (r.type == c.require_type &&
          r.type_pos == static_cast<std::uint8_t>(norm::TypePos::Suffix));
}

[[nodiscard]] bool is_found(std::span<NameCand const> cands);

} // namespace chiasmos::query
