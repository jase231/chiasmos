#pragma once

#include <algorithm>
#include <cmath>

#include "core/types.hpp"
#include "index/format.hpp"
#include "normalize/normalizer.hpp"

namespace chiasmos::query {

struct Scoring {
  // directional agreement query and record, "N Main" vs "S Main"
  float dir_other_position = 0.98f, dir_query_none = 0.97f,
        dir_record_none = 0.92f, dir_conflict = 0.60f;
  // street type agreement, "St" vs "Rd"
  float type_query_none = 0.97f, type_record_none = 0.95f,
        type_conflict = 0.75f, mod_mismatch = 0.85f;
  // how a way carries its name, ie. NameKind
  float kind_alternate = 0.97f, kind_old = 0.85f;
  // near hint of query, calc using exp(-d / scale)
  float near_floor = 0.05f, near_scale_m = 3000.0f;
  // name transitions and concurrencies are less likely what user wants
  float jk_transition = 0.85f, jk_concurrency = 0.80f;
  // generate an extra Cand when last word of query core is street type
  // eg. Magnolia Ridge Dr -> Magnolia Ridge, Magnolia
  bool use_trimmed_suffix = true;
  // junctions closer to eachother than these vals are considered one
  float merge_motorway_m = 150, merge_primary_m = 90, merge_other_m = 60;
  // threshold for defining a result to be ambiguous
  float ambiguous_ratio = 0.90f, ambiguous_distance_m = 150;
  float conf_high = 0.80f, conf_medium = 0.50f;
};
inline constexpr Scoring kScoring{};

// agreement between a query's components and a stored FullRec
[[nodiscard]] float name_agreement(norm::Components const& q,
                                   index::FullRec const& r,
                                   bool numbered_street);
[[nodiscard]] inline bool is_numbered_core(std::string_view core) {
  return !core.empty() && core.front() >= '0' && core.front() <= '9';
}

[[nodiscard]] inline float name_kind_factor(NameKind k) {
  return k == NameKind::Primary ? 1.0f
         : k == NameKind::Alt   ? kScoring.kind_alternate
                                : kScoring.kind_old;
}
[[nodiscard]] inline float junction_kind_factor(JunctionKind k) {
  return k == JunctionKind::NameTransition ? kScoring.jk_transition
         : k == JunctionKind::Concurrency  ? kScoring.jk_concurrency
                                           : 1.0f;
}
[[nodiscard]] inline float near_factor(double meters) {
  return std::max(kScoring.near_floor,
                  static_cast<float>(std::exp(
                      -meters / static_cast<double>(kScoring.near_scale_m))));
}
[[nodiscard]] inline float merge_radius_m(RoadClass c) {
  return c <= RoadClass::Trunk       ? kScoring.merge_motorway_m
         : c <= RoadClass::Secondary ? kScoring.merge_primary_m
                                     : kScoring.merge_other_m;
}

} // namespace chiasmos::query
