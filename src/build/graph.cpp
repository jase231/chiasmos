#include "build/graph.hpp"

#include <algorithm>
#include <functional>
#include <ranges>

namespace chiasmos::build {

Graph build_graph(RoadWays const& ways) {
  struct Occurrence {
    std::int64_t node;
    std::uint32_t way, pos; // pos: index into RoadWays::nodes
  };

  std::vector<Occurrence> occurrences;
  occurrences.reserve(ways.nodes.size());
  for (std::uint32_t w = 0; w < ways.size(); ++w)
    for (std::size_t pos = ways.node_off[w]; pos < ways.node_off[w + 1]; ++pos)
      occurrences.push_back(
          { ways.nodes[pos].id, w, static_cast<std::uint32_t>(pos) });

  // sort such that any nodes used by several ways (i.e. intersection
  // candidates) are incident in the vector
  std::ranges::sort(occurrences, {}, [](Occurrence const& o) {
    return std::pair(o.node, o.way);
  });

  auto const is_end = [&](Occurrence const& o) -> std::uint32_t {
    return o.pos ==
               ways.node_off[o.way] || // node_off[way] is first node of way
           o.pos + 1 ==
               ways.node_off[o.way +
                             1]; // node_off[way + 1] is last node of way
  };
  auto const sum = [](auto&& values) {
    return std::ranges::fold_left(values, 0u, std::plus{});
  };
  auto const same_node = [](Occurrence const& a, Occurrence const& b) {
    return a.node == b.node;
  };
  auto const same_way = [](Occurrence const& a, Occurrence const& b) {
    return a.way == b.way;
  };

  Graph g;
  g.off.push_back(0);
  // each run is a chunk of Occurences of the same node
  for (auto const run : occurrences | std::views::chunk_by(same_node)) {
    if (std::ranges::size(run) < 2)
      continue; // reject all nodes referenced only once

    // store intersection candidate node's ID and Coord
    g.node.push_back(run.front().node);
    g.at.push_back(ways.nodes[run.front().pos].at);
    // store the ways incident to the intersection candidate
    // each of_way is a chunk of Occurences of same node and same way
    for (auto const of_way : run | std::views::chunk_by(same_way))
      g.incident.push_back(
          { of_way.front().way,
            sum(of_way | std::views::transform([&](Occurrence const& o) {
                  return 2 - is_end(o); // a way passing through adds 2 arms, an
                                        // end only 1
                })) });
    g.off.push_back(g.incident.size());
  }
  return g;
}

} // namespace chiasmos::build
