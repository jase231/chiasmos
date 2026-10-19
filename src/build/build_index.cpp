#include "build/build_index.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <tuple>

#include "build/graph.hpp"
#include "build/ingest.hpp"
#include "build/names.hpp"
#include "build/pairs.hpp"
#include "build/roundabouts.hpp"
#include "index/draft.hpp"

namespace chiasmos::build {
namespace {

using Junctions = std::vector<index::Draft::Junction>;

void deduplicate(Junctions& junctions) {
  auto const key = [](index::Draft::Junction const& j) {
    return std::tuple(j.a, j.b, j.osm_node);
  };

  // order provides projection that differentiates roundabouts, so if there are
  // dupes, we keep the one that identifies the junction as a roundabout
  auto const order = [&](index::Draft::Junction const& j) {
    return std::tuple(key(j), j.kind != JunctionKind::Roundabout);
  };

  std::ranges::sort(junctions, { /* less */ }, order);
  // drop any duplicates while preferring roundabouts
  junctions.erase(std::ranges::unique(junctions, {}, key).begin(),
                  junctions.end());
}

std::string stats_json(Stats const& stats, Junctions const& junctions) {
  // kinds indexed by JunctionKind enum cast to size_t
  std::array<std::size_t, 4> kinds{};
  // count up states
  for (auto const& j : junctions)
    ++kinds[static_cast<std::size_t>(j.kind)];
  // return stats formatted in json
  return std::format(
      R"({{"road_ways":{},"vertices":{},"full_names":{},"junctions":{},"roundabouts":{},)"
      R"("kinds":{{"node":{},"roundabout":{},"name_transition":{},"concurrency":{}}}}})",
      stats.road_ways, stats.vertices, stats.full_names, stats.junctions,
      stats.roundabouts, kinds[0], kinds[1], kinds[2], kinds[3]);
}

} // end namespace

Result<Stats> build_index(std::filesystem::path const& pbf,
                          std::filesystem::path const& out,
                          Options const& opt) {
  auto const started = std::chrono::steady_clock::now();
  auto ingested = ingest_roads(pbf, opt);
  if (!ingested)
    return std::unexpected(std::move(ingested.error()));
  RoadWays const& ways = ingested->ways;
  Graph const graph = build_graph(ways);

  index::Draft draft;
  WayNames const names = build_names(ways, draft);
  Junctions junctions;
  PairRule rule(ways, names);
  for (std::size_t v = 0; v < graph.size(); ++v)
    rule.at(graph.incident_at(v), { .at = graph.at[v], .node = graph.node[v] },
            junctions);
  Stats stats;
  stats.roundabouts = add_roundabout_junctions(ways, graph, rule, junctions);
  deduplicate(junctions);
  for (auto const& junction : junctions)
    draft.add_junction(junction);

  stats.road_ways = ways.size();
  stats.vertices = graph.size();
  stats.junctions = junctions.size();
  stats.full_names = draft.full_count();
  index::Meta const meta{ .region = opt.region,
                          .data_timestamp = ingested->data_timestamp,
                          .json = stats_json(stats, junctions) };
  if (auto written = draft.write(out, meta); !written)
    return std::unexpected(std::move(written.error()));
  stats.seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();
  return stats;
}

} // namespace chiasmos::build
