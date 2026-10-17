#include "query/candidates.hpp"

#include <algorithm>

#include "query/scoring.hpp"

namespace chiasmos::query {

std::vector<NameCand> name_candidates(index::Index const& ix,
                                      norm::Normalized const& street) {
  std::vector<norm::CoreAlt> alts;
  norm::query_core_alternatives(street, alts);
  std::vector<NameCand> out;
  for (norm::CoreAlt const& a : alts) {
    bool const trimmed = a.kind == norm::AltKind::TrimmedSuffix;
    auto const core = ix.find_core(a.core);
    if (!core || (trimmed && !kScoring.use_trimmed_suffix))
      continue;
    NameCand c{ .core = *core, .prior = a.prior, .comp = street.comp };
    if (trimmed) {
      c.require_type = a.type;
      c.comp.type = 0;
      c.comp.type_pos = norm::TypePos::None;
    } else if (a.kind == norm::AltKind::BareNumber) {
      c.comp.post_mod = 0;
    }
    auto const dup = std::ranges::find(
        out, c.core,
        &NameCand::core); // one entry per core, the best prior wins
    if (dup == out.end())
      out.push_back(c);
    else if (c.prior > dup->prior)
      *dup = c;
  }
  return out;
}

bool is_found(std::span<NameCand const> cands) {
  return std::ranges::any_of(
      cands, [](NameCand const& c) { return c.require_type == 0; });
}

} // namespace chiasmos::query
