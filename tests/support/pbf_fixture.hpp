#pragma once
// Test support for the build module: describe a tiny OSM extract (nodes and
// ways) in C++, write it as a temp .osm.pbf, run build_index on it and open the
// result. Node positions are offsets from a fixed origin in units of 1e-4
// degrees (about 10 m), so a test reads like a street map on graph paper.
#include <gtest/gtest.h>

#include <osmium/builder/attr.hpp>
#include <osmium/builder/osm_object_builder.hpp>
#include <osmium/io/pbf_output.hpp>
#include <osmium/io/writer.hpp>
#include <osmium/memory/buffer.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "build/build_index.hpp"
#include "index/index.hpp"
#include "normalize/normalizer.hpp"

namespace chiasmos::test {

using Tags = std::vector<std::pair<std::string, std::string>>;

// highway=<highway> with a name, the common case.
inline Tags road(std::string name, std::string highway = "residential") {
  return { { "highway", std::move(highway) }, { "name", std::move(name) } };
}

struct Built {
  build::Stats stats;
  index::Index index;
};

class PbfFixture {
public:
  static constexpr double kOriginLon = -77.0, kOriginLat = 38.0, kUnit = 1e-4;

  PbfFixture() {
    auto const* t = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = std::filesystem::temp_directory_path() /
           (std::string("chiasmos_build_") + t->test_suite_name() + "_" +
            t->name());
    std::filesystem::create_directories(dir_);
  }
  ~PbfFixture() { std::filesystem::remove_all(dir_); }
  PbfFixture(PbfFixture const&) = delete;
  PbfFixture& operator=(PbfFixture const&) = delete;

  // The coordinate of node(id, east, north).
  static Coord at(double east, double north) {
    return to_coord(kOriginLon + east * kUnit, kOriginLat + north * kUnit);
  }

  // Both replace an earlier node or way with the same id, so a test can change
  // one object between builds.
  PbfFixture& node(std::int64_t id, double east, double north) {
    put(nodes_, Node{ id, east, north });
    return *this;
  }
  PbfFixture& way(std::int64_t id, std::vector<std::int64_t> refs, Tags tags) {
    put(ways_, Way{ id, std::move(refs), std::move(tags) });
    return *this;
  }
  // Writes ways before nodes, which a single-pass reader cannot use.
  PbfFixture& ways_first() {
    ways_first_ = true;
    return *this;
  }
  PbfFixture& timestamp(std::string t) {
    timestamp_ = std::move(t);
    return *this;
  }

  [[nodiscard]] std::filesystem::path pbf_path() const {
    return dir_ / "extract.osm.pbf";
  }
  [[nodiscard]] std::filesystem::path
  index_path(char const* name = "out.cix") const {
    return dir_ / name;
  }

  void write_pbf() const {
    osmium::io::Header header;
    header.set("generator", "chiasmos-test");
    if (!timestamp_.empty())
      header.set("osmosis_replication_timestamp", timestamp_);
    osmium::io::Writer writer{ pbf_path().string(), header,
                               osmium::io::overwrite::allow };
    osmium::memory::Buffer buffer{ 1 << 16,
                                   osmium::memory::Buffer::auto_grow::yes };
    auto sorted_nodes =
        nodes_; // PBF consumers expect nodes, then ways, each by id
    std::ranges::sort(sorted_nodes, {}, &Node::id);
    auto sorted_ways = ways_;
    std::ranges::sort(sorted_ways, {}, &Way::id);
    auto const add_nodes = [&] {
      for (Node const& n : sorted_nodes)
        osmium::builder::add_node(
            buffer, osmium::builder::attr::_id(n.id),
            osmium::builder::attr::_location(osmium::Location{
                kOriginLon + n.east * kUnit, kOriginLat + n.north * kUnit }));
    };
    auto const add_ways = [&] {
      for (Way const& w : sorted_ways)
        osmium::builder::add_way(buffer, osmium::builder::attr::_id(w.id),
                                 osmium::builder::attr::_nodes(w.refs),
                                 osmium::builder::attr::_tags(w.tags));
    };
    if (ways_first_)
      add_ways(), add_nodes();
    else
      add_nodes(), add_ways();
    writer(std::move(buffer));
    writer.close();
  }

  // Writes the PBF, builds the index next to it and opens it. Index::validate()
  // must pass on every result.
  [[nodiscard]] Result<Built>
  build(build::Options const& options = { .region = "test/region" },
        char const* name = "out.cix") const {
    write_pbf();
    auto stats = build::build_index(pbf_path(), index_path(name), options);
    if (!stats)
      return std::unexpected(stats.error());
    auto ix = index::Index::open(index_path(name));
    if (!ix)
      return std::unexpected(ix.error());
    if (auto ok = ix->validate(); !ok)
      return std::unexpected(ok.error());
    return Built{ *stats, std::move(*ix) };
  }

private:
  struct Node {
    std::int64_t id;
    double east, north;
  };
  struct Way {
    std::int64_t id;
    std::vector<std::int64_t> refs;
    Tags tags;
  };
  template <class T> static void put(std::vector<T>& all, T item) {
    auto const it = std::ranges::find(all, item.id, &T::id);
    if (it == all.end())
      all.push_back(std::move(item));
    else
      *it = std::move(item);
  }

  std::filesystem::path dir_;
  std::vector<Node> nodes_;
  std::vector<Way> ways_;
  std::string timestamp_;
  bool ways_first_ = false;
};

// The junctions of two street names, found the way a query would: normalize
// both, look up their cores, take the pair's range and keep the records whose
// full names are exactly these two (so "N Main St" and "Main St" do not blur
// together).
inline std::vector<index::GroupRec> junctions_of(index::Index const& ix,
                                                 std::string_view first,
                                                 std::string_view second) {
  norm::Normalized a, b;
  norm::normalize_street(first, a);
  norm::normalize_street(second, b);
  auto const core_a = ix.find_core(a.core), core_b = ix.find_core(b.core);
  std::vector<index::GroupRec> found;
  if (!core_a || !core_b)
    return found;
  for (index::GroupRec const& g : ix.pair_groups(*core_a, *core_b)) {
    auto const fa = ix.full_str(g.full_a), fb = ix.full_str(g.full_b);
    if ((fa == a.full && fb == b.full) || (fa == b.full && fb == a.full))
      found.push_back(g);
  }
  return found;
}

// The junction of two names when there is exactly one, else nullopt (so
// `ASSERT_TRUE(only_junction(...))` checks both).
inline std::optional<index::GroupRec> only_junction(index::Index const& ix,
                                                    std::string_view first,
                                                    std::string_view second) {
  auto const found = junctions_of(ix, first, second);
  if (found.size() != 1)
    return std::nullopt;
  return found.front();
}

// What a junction record says about one of its two streets.
struct Side {
  RoadClass road;
  NameKind kind;
};
inline Side side_of(index::Index const& ix, index::GroupRec const& g,
                    std::string_view street) {
  norm::Normalized n;
  norm::normalize_street(street, n);
  bool const is_a = ix.full_str(g.full_a) == n.full;
  return { static_cast<RoadClass>(is_a ? g.class_a : g.class_b),
           static_cast<NameKind>(is_a ? g.name_kind_a : g.name_kind_b) };
}

inline std::string display_of(index::Index const& ix, index::GroupRec const& g,
                              std::string_view street) {
  norm::Normalized n;
  norm::normalize_street(street, n);
  return std::string(
      ix.full_display(ix.full_str(g.full_a) == n.full ? g.full_a : g.full_b));
}

inline std::vector<char> read_bytes(std::filesystem::path const& p) {
  std::ifstream f(p, std::ios::binary);
  return { std::istreambuf_iterator<char>(f),
           std::istreambuf_iterator<char>() };
}

} // namespace chiasmos::test
