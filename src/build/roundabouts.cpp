#include "build/roundabouts.hpp"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <ranges>

namespace chiasmos::build {
namespace {

constexpr double kMaxRingDiagonalM = 150;

class Rings {
public:
  explicit Rings(std::size_t n) : parent_(n) {
    std::iota(parent_.begin(), parent_.end(), 0u);
  }
  std::uint32_t find(std::uint32_t w) {
    while (parent_[w] != w)
      w = parent_[w] = parent_[parent_[w]];
    return w;
  }
  void unite(std::uint32_t a, std::uint32_t b) { parent_[find(a)] = find(b); }

private:
  std::vector<std::uint32_t> parent_;
};

} // namespace

std::size_t add_roundabout_junctions(RoadWays const& ways, Graph const& graph,
                                     PairRule& rule,
                                     std::vector<index::Draft::Junction>& out) {
  auto const is_ring = [&](Incident const& i) {
    return ways.attrs[i.way].roundabout;
  };
  auto const ring_way_at =
      [&](std::size_t v) { // any ring way at the vertex, or nullptr
        auto const incident = graph.incident_at(v);
        auto const it = std::ranges::find_if(incident, is_ring);
        return it == incident.end() ? nullptr : &*it;
      };

  Rings rings(ways.size());
  for (std::size_t v = 0; v < graph.size(); ++v)
    if (Incident const* first = ring_way_at(v))
      for (Incident const& i : graph.incident_at(v))
        if (is_ring(i))
          rings.unite(i.way, first->way);
  std::vector<std::pair<std::uint32_t, std::size_t>>
      ring_vertices; // (ring id, vertex)
  for (std::size_t v = 0; v < graph.size(); ++v)
    if (Incident const* first = ring_way_at(v))
      ring_vertices.emplace_back(rings.find(first->way), v);
  std::ranges::sort(ring_vertices);

  std::size_t handled = 0;
  std::vector<Incident> incident;
  std::vector<NodeRef> nodes;
  for (auto const ring :
       ring_vertices | std::views::chunk_by([](auto const& a, auto const& b) {
         return a.first == b.first;
       })) {
    incident.clear();
    for (auto const& [id, v] : ring)
      incident.insert(incident.end(), graph.incident_at(v).begin(),
                      graph.incident_at(v).end());
    std::ranges::sort(incident, {}, &Incident::way);
    incident.erase(std::ranges::unique(incident, {}, &Incident::way).begin(),
                   incident.end());

    nodes.clear();
    for (Incident const& i : incident)
      if (is_ring(i))
        std::ranges::copy(ways.nodes_of(i.way), std::back_inserter(nodes));
    Coord lo = nodes.front().at, hi = lo;
    std::int64_t sum_x = 0, sum_y = 0;
    for (NodeRef const& n : nodes) {
      lo = { std::min(lo.x, n.at.x), std::min(lo.y, n.at.y) };
      hi = { std::max(hi.x, n.at.x), std::max(hi.y, n.at.y) };
      sum_x += n.at.x, sum_y += n.at.y;
    }
    if (dist_m(lo, hi) > kMaxRingDiagonalM)
      continue;
    auto const count = static_cast<std::int64_t>(nodes.size());
    Coord const center{ static_cast<std::int32_t>(sum_x / count),
                        static_cast<std::int32_t>(sum_y / count) };
    NodeRef const& evidence = *std::ranges::min_element(
        nodes, {}, [&](NodeRef const& n) { return dist_m(n.at, center); });
    rule.at(incident, { .at = evidence.at, .node = evidence.id, .ring = true },
            out);
    ++handled;
  }
  return handled;
}

} // end namespace chiasmos::build
