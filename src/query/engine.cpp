#include "query/engine.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <functional>
#include <memory_resource>
#include <ranges>

#include "query/candidates.hpp"
#include "query/resolve.hpp"
#include "query/scoring.hpp"

namespace chiasmos::query {
namespace {

using index::GroupRec;

[[nodiscard]] Coord at(GroupRec const& g) { return { g.x, g.y }; }
[[nodiscard]] RoadClass main_road(GroupRec const& g) {
  return std::min(static_cast<RoadClass>(g.class_a),
                  static_cast<RoadClass>(g.class_b));
}

// a stored candidate junction
struct Hit {
  GroupRec const* g = nullptr;
  std::uint32_t full1 = 0, full2 = 0;
  float score = 0;
};

struct Place {
  Hit best;       // highest scoring hit at the Place
  RoadClass road; // determines the merge radius
  std::pmr::vector<GroupRec const*> members;

  Place(Hit const& h, std::pmr::memory_resource* mr)
      : best(h), road(main_road(*h.g)), members({ h.g }, mr) {}
  void add(GroupRec const* g) {
    if (std::ranges::contains(members, g))
      return; // the same junction reached through two alternative cores
    members.push_back(g);
    road = std::min(road, main_road(*g));
  }
};

// mean of GroupRecs' locations
[[nodiscard]] Coord centroid(std::span<GroupRec const* const> ms) {
  auto const mean = [&](auto field) {
    auto const sum = std::ranges::fold_left(ms | std::views::transform(field),
                                            std::int64_t{ 0 }, std::plus<>{});
    return static_cast<std::int32_t>(std::llround(
        static_cast<double>(sum) / static_cast<double>(ms.size())));
  };
  return { mean(&GroupRec::x), mean(&GroupRec::y) };
}

// score a candidate (as identified by core names only) by comparing and
// scoring the candidate based on full names, kinds, and location as
// stored in the GroupRec
[[nodiscard]] Hit score_group(index::Index const& ix, Request const& req,
                              GroupRec const& g, NameCand const& c1,
                              NameCand const& c2) {
  struct Arm {
    std::uint32_t core, full;
    std::uint8_t name_kind;
  };
  std::array<Arm, 2> const arms{ { { g.core_a, g.full_a, g.name_kind_a },
                                   { g.core_b, g.full_b, g.name_kind_b } } };
  auto const side =
      [&](NameCand const& c,
          Arm const& arm) { // prior x agreement x how the way carries the name
        index::FullRec const& r = ix.fulls()[arm.full];
        if (!meets_type_requirement(c, r))
          return 0.0f;
        return c.prior *
               name_agreement(c.comp, r,
                              is_numbered_core(ix.core_str(c.core))) *
               name_kind_factor(static_cast<NameKind>(arm.name_kind));
      };
  Hit best{ .g = &g };
  for (bool const swap : { false, true }) {
    Arm const &a = arms[swap], &b = arms[!swap];
    if (a.core != c1.core || b.core != c2.core)
      continue;
    if (float const s = side(c1, a) * side(c2, b); s > best.score)
      best = { &g, a.full, b.full, s };
  }
  best.score *= junction_kind_factor(static_cast<JunctionKind>(g.kind));
  if (req.near)
    best.score *= near_factor(dist_m(at(g), *req.near));
  return best;
}

// iterate over all hits to produce Places, merge several hits into a
// single place if GroupRecs are identical or if their names match and
// are within their RoadClass' merge_radius
void merge_hits(std::pmr::vector<Hit>& hits, std::size_t max_places,
                std::pmr::memory_resource* mr,
                std::pmr::vector<Place>& places) {
  std::ranges::sort(hits, [](Hit const& a, Hit const& b) {
    return a.score != b.score ? a.score > b.score : a.g < b.g;
  }); // total order
  for (Hit const& h : hits) {
    GroupRec const& g = *h.g;
    auto const home = std::ranges::find_if(places, [&](Place const& p) {
      return p.best.g->core_a == g.core_a && p.best.g->core_b == g.core_b &&
             (std::ranges::contains(p.members, h.g) ||
              dist_m(centroid(p.members), at(g)) <=
                  merge_radius_m(std::min(p.road, main_road(g))));
    });
    if (home != places.end())
      home->add(h.g);
    else if (places.size() < max_places)
      places.emplace_back(h, mr);
    else
      break;
  }
}

[[nodiscard]] Candidate to_candidate(index::Index const& ix, Request const& req,
                                     Place const& p, bool ambiguous) {
  Candidate c;
  c.rep = centroid(p.members);
  c.score = p.best.score;
  c.confidence = c.score >= kScoring.conf_high && !ambiguous ? Confidence::High
                 : c.score >= kScoring.conf_medium ? Confidence::Medium
                                                   : Confidence::Low;
  c.kind = p.members.size() > 1 ? JunctionKind::Grouped
                                : static_cast<JunctionKind>(p.best.g->kind);
  c.street1 = ix.full_display(p.best.full1);
  c.street2 = ix.full_display(p.best.full2);
  c.osm_nodes = p.members | std::views::transform(&GroupRec::osm_node) |
                std::ranges::to<std::vector>();
  std::ranges::sort(c.osm_nodes);
  c.osm_nodes.erase(std::ranges::unique(c.osm_nodes).begin(),
                    c.osm_nodes.end());
  if (req.near)
    c.distance_to_near_m = static_cast<float>(dist_m(c.rep, *req.near));
  return c;
}

} // namespace

Result resolve(index::Index const& ix, Request const& req,
               std::array<norm::Normalized, 2> const& sides) {
  std::array const cands{ name_candidates(ix, sides[0]),
                          name_candidates(ix, sides[1]) };

  // every temp container object gets allocated by an arena allocator
  // using a stack array as its upfront buffer
  // need to do some testing and see whether 32KB is a wise choice
  std::array<std::byte, 32 * 1024> buffer;
  std::pmr::monotonic_buffer_resource pool(buffer.data(), buffer.size());
  std::pmr::vector<Hit> hits(&pool);
  std::size_t outside_bbox = 0;
  for (NameCand const& c1 : cands[0])
    for (NameCand const& c2 : cands[1])
      for (GroupRec const& g : ix.pair_groups(c1.core, c2.core)) {
        if (req.bbox && !req.bbox->contains(at(g)))
          ++outside_bbox;
        else if (Hit const h = score_group(ix, req, g, c1, c2); h.score > 0)
          hits.push_back(h);
      }

  int const limit = std::clamp(req.limit, 1, 1000);
  std::pmr::vector<Place> places(&pool);
  merge_hits(hits, static_cast<std::size_t>(std::max(4 * limit, 32)), &pool,
             places);
  std::size_t const found = places.size();
  std::erase_if(places,
                [&](Place const& p) { return p.best.score < req.min_score; });

  Result res;
  if (places.empty()) {
    auto const missing = [&](std::size_t i) { return !is_found(cands[i]); };
    if (found > 0) {
      res.status = Status::NoIntersection;
      res.notes.push_back(std::format(
          "every junction found scores below min_score {}", req.min_score));
    } else if (missing(0) || missing(1)) {
      res.status = Status::NoStreetMatch;
      for (std::size_t i = 0; i < 2; ++i)
        if (missing(i))
          res.notes.push_back(
              std::format("street {} not found: \"{}\"", i + 1, sides[i].full));
    } else {
      res.status = Status::NoIntersection;
      res.notes.push_back(
          outside_bbox > 0
              ? std::format("{} junction(s) lie outside the bbox", outside_bbox)
              : "both streets exist but no junction joins them");
    }
    return res;
  }

  Place const& top = places.front();
  bool const ambiguous =
      std::ranges::any_of(places | std::views::drop(1), [&](Place const& p) {
        return p.best.score >= kScoring.ambiguous_ratio * top.best.score &&
               dist_m(centroid(p.members), centroid(top.members)) >
                   static_cast<double>(kScoring.ambiguous_distance_m);
      });
  res.status = ambiguous ? Status::Ambiguous : Status::Ok;
  for (Place const& p : places | std::views::take(limit))
    res.candidates.push_back(to_candidate(ix, req, p, ambiguous && &p == &top));
  return res;
}

Result Engine::run(Request const& req) const {
  bool const structured = !req.street1.empty() || !req.street2.empty();
  norm::ParsedQuery parsed;
  if (!structured)
    parsed = norm::parse_freeform(req.q);
  std::array<norm::Normalized, 2> sides;
  norm::normalize_street(structured ? req.street1 : parsed.street1, sides[0]);
  norm::normalize_street(structured ? req.street2 : parsed.street2, sides[1]);

  Result res;
  if (sides[0].empty() && sides[1].empty()) {
    res.notes.push_back(
        "give street1 and street2, or q"); // status stays InvalidRequest
  } else if (sides[0].empty() || sides[1].empty()) {
    res.status = Status::NotAnIntersection;
    res.notes.push_back("an intersection needs two streets");
  } else {
    res = resolve(*ix_, req, sides);
    if (!parsed.locality.empty())
      res.notes.push_back(std::format(
          "locality \"{}\" ignored: use near or bbox to narrow the search",
          parsed.locality));
  }
  return res;
}

} // namespace chiasmos::query
