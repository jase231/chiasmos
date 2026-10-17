// Road filter, name tags, refusing missing nodes, index metadata and
// determinism, all through a real PBF.
#include <gtest/gtest.h>

#include "support/pbf_fixture.hpp"

namespace chiasmos {
namespace {

using namespace test;

// Main Street along the x axis through nodes 1..segments+1 (node i+1 at x = 10
// i).
void main_street(PbfFixture& f, int segments) {
  std::vector<std::int64_t> refs;
  for (int i = 0; i <= segments; ++i) {
    f.node(i + 1, 10 * i, 0);
    refs.push_back(i + 1);
  }
  f.way(1, refs, road("Main Street"));
}
// A street that meets Main Street at node `at` + 1.
void side_street(PbfFixture& f, int at, Tags tags) {
  f.node(100 + at, 10 * at, 10);
  f.way(100 + at, { 100 + at, at + 1 }, std::move(tags));
}

TEST(BuildIngest, RoadFilterKeepsRoadsAndDropsTheRest) {
  PbfFixture f;
  main_street(f, 8);
  side_street(f, 1, road("Kept Residential"));
  side_street(f, 2, road("Alley Service", "service")); // named service: kept
  side_street(f, 3, { { "highway", "service" } }); // unnamed service: dropped
  side_street(f, 4,
              road("Trail Footway", "footway")); // paths need include_paths
  side_street(f, 5,
              { { "highway", "residential" },
                { "name", "Plaza Street" },
                { "area", "yes" } });
  side_street(f, 6, road("Future Street", "construction"));
  side_street(f, 7,
              road("Ramp Link", "primary_link")); // _link of a main class: kept
  side_street(f, 8,
              road("Bogus Link", "residential_link")); // there is no such thing
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const kept = [&](char const* street) {
    return !junctions_of(built->index, "Main Street", street).empty();
  };
  EXPECT_TRUE(kept("Kept Residential"));
  EXPECT_TRUE(kept("Alley Service"));
  EXPECT_FALSE(kept("Trail Footway"));
  EXPECT_FALSE(kept("Plaza Street"));
  EXPECT_FALSE(kept("Future Street"));
  EXPECT_TRUE(kept("Ramp Link"));
  EXPECT_FALSE(kept("Bogus Link"));
  EXPECT_EQ(built->stats.road_ways,
            1u + 3u); // Main + the three kept side streets (the unnamed service
                      // is gone too)
}

TEST(BuildIngest, IncludePathsKeepsNamedFootwaysButNotSidewalks) {
  PbfFixture f;
  main_street(f, 3);
  side_street(f, 1, road("Trail Footway", "footway"));
  side_street(f, 2,
              { { "highway", "footway" },
                { "name", "Walk Sidewalk" },
                { "footway", "sidewalk" } });
  side_street(f, 3, { { "highway", "cycleway" } }); // unnamed
  auto const built =
      f.build({ .region = "test/region", .include_paths = true });
  ASSERT_TRUE(built) << built.error().msg;
  auto const path = only_junction(built->index, "Main Street", "Trail Footway");
  ASSERT_TRUE(path);
  EXPECT_EQ(side_of(built->index, *path, "Trail Footway").road,
            RoadClass::Path);
  EXPECT_TRUE(
      junctions_of(built->index, "Main Street", "Walk Sidewalk").empty());
  EXPECT_EQ(built->stats.road_ways, 2u);
}

TEST(BuildIngest, AWayWithOneNodeIsDropped) {
  PbfFixture f;
  f.node(1, 0, 0).node(2, 10, 0).node(3, 20, 0);
  f.way(10, { 1, 2 }, road("Main Street")).way(11, { 3 }, road("Stub Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->stats.road_ways, 1u);
  EXPECT_EQ(built->stats.full_names,
            1u); // the dropped way's name is not in the index
}

TEST(BuildIngest, RefusesAWayWithANodeMissingFromTheFile) {
  PbfFixture f;
  f.node(1, 0, 0).node(2, 10, 0);
  f.way(10, { 1, 99, 2 }, road("Main Street")); // node 99 is not in the file
  auto const built = f.build();
  ASSERT_FALSE(built);
  EXPECT_EQ(built.error().code, Errc::Corrupt);
}

TEST(BuildIngest, NameTagsAreSplitTrimmedAndFiltered) {
  PbfFixture f;
  f.node(1, 0, 0).node(2, 10, 0).node(3, 20, 0).node(4, 10, 10).node(5, 10,
                                                                     -10);
  f.way(10, { 1, 2, 3 }, road("Main Street"));
  std::string const long_name(61, 'x');
  f.way(11, { 4, 2, 5 },
        { { "highway", "residential" },
          { "name", "Oak Street ;  Maple Street" }, // two names, spaces trimmed
          { "ref", "Elm Street, North" },           // a comma: skipped
          { "alt_name", long_name },           // longer than 60 bytes: skipped
          { "loc_name", ";;Birch Street;" },   // empty parts ignored
          { "description", "Pine Street" } }); // not a name tag
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const& ix = built->index;
  EXPECT_TRUE(only_junction(ix, "Main Street", "Oak Street"));
  EXPECT_TRUE(only_junction(ix, "Main Street", "Maple Street"));
  EXPECT_TRUE(only_junction(ix, "Main Street", "Birch Street"));
  EXPECT_TRUE(junctions_of(ix, "Main Street", "Elm Street").empty());
  EXPECT_TRUE(junctions_of(ix, "Main Street", "Pine Street").empty());
  EXPECT_EQ(built->stats.junctions, 3u);
}

TEST(BuildIngest, DisplayNameIsTheMostFrequentSpelling) {
  PbfFixture f;
  // Three spellings of one full name (they differ only in case, which every
  // normalizer folds): "Main Street" twice beats "MAIN STREET" once, and Oak
  // Street's two spellings tie, so the smaller string wins.
  f.node(1, 0, 0)
      .node(2, 10, 0)
      .node(3, 20, 0)
      .node(4, 30, 0)
      .node(5, 10, 10)
      .node(6, 20, 10);
  f.way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 2, 3 }, road("MAIN STREET"))
      .way(12, { 3, 4 }, road("Main Street"));
  f.way(13, { 5, 2 }, road("Oak Street")).way(14, { 6, 3 }, road("OAK STREET"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const found = junctions_of(built->index, "Main Street", "Oak Street");
  ASSERT_FALSE(found.empty());
  EXPECT_EQ(display_of(built->index, found.front(), "Main Street"),
            "Main Street");
  EXPECT_EQ(display_of(built->index, found.front(), "Oak Street"),
            "OAK STREET");
}

TEST(BuildIngest, IndexCarriesRegionTimestampAndStats) {
  PbfFixture f;
  f.timestamp("2026-03-01T12:00:00Z");
  f.node(1, 0, 0).node(2, 10, 0).node(3, 0, 10);
  f.way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 3, 2 }, road("Oak Street"));
  auto const built = f.build({ .region = "us/testland" });
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->index.region(), "us/testland");
  EXPECT_EQ(built->index.data_timestamp(), "2026-03-01T12:00:00Z");
  EXPECT_NE(built->index.meta_json().find("\"junctions\":1"),
            std::string_view::npos)
      << built->index.meta_json();
  EXPECT_EQ(built->stats.full_names, 2u);
}

TEST(BuildIngest, ReportsAMissingFileInsteadOfThrowing) {
  auto const result = build::build_index(
      "/nonexistent/extract.osm.pbf",
      std::filesystem::temp_directory_path() / "chiasmos_never.cix",
      { .region = "x" });
  ASSERT_FALSE(result);
  EXPECT_EQ(result.error().code, Errc::Io);
}

TEST(BuildIngest, RefusesAPbfWhoseWaysComeBeforeItsNodes) {
  PbfFixture f;
  f.node(1, 0, 0)
      .node(2, 10, 0)
      .way(10, { 1, 2 }, road("Main Street"))
      .ways_first();
  auto const built = f.build();
  ASSERT_FALSE(built);
  EXPECT_EQ(built.error().code, Errc::Corrupt);
}

// Catches output order that depends on libosmium's reader-thread scheduling or
// on memory addresses. Both builds run in one process with one standard
// library, so it cannot catch order that depends on a particular std::hash.
TEST(BuildIngest, TwoBuildsOfOnePbfAreByteIdentical) {
  PbfFixture f;
  // A small city grid with a roundabout, aliases, alternates and repeated
  // spellings, so every stage has something to order.
  constexpr int kN = 6;
  for (int i = 0; i < kN; ++i)
    for (int j = 0; j < kN; ++j)
      f.node(1 + i * 10 + j, 10 * i, 10 * j);
  for (int i = 0; i < kN; ++i) {
    std::vector<std::int64_t> row, col;
    for (int j = 0; j < kN; ++j)
      row.push_back(1 + j * 10 + i), col.push_back(1 + i * 10 + j);
    f.way(100 + i, row,
          { { "highway", "secondary" },
            { "name", i % 2 ? "Avenue " + std::to_string(i)
                            : "AVENUE " + std::to_string(i) },
            { "ref", "SR " + std::to_string(30 + i) } });
    f.way(200 + i, col,
          { { "highway", "residential" },
            { "name", "Elm Street" },
            { "alt_name",
              "Street " + std::string(1, static_cast<char>('A' + i)) } });
  }
  f.node(900, 100, 100)
      .node(901, 103, 103)
      .node(902, 100, 106)
      .node(903, 97, 103);
  f.way(300, { 900, 901, 902, 903, 900 },
        { { "highway", "residential" }, { "junction", "roundabout" } });
  f.node(910, 110, 103).node(911, 90, 103);
  f.way(301, { 1 + 5 * 10 + 5, 900 }, road("Spur Road"))
      .way(302, { 901, 910 }, road("East Road"))
      .way(303, { 903, 911 }, road("West Road"));
  auto const first = f.build({ .region = "test/region" }, "first.cix");
  auto const second = f.build({ .region = "test/region" }, "second.cix");
  ASSERT_TRUE(first) << first.error().msg;
  ASSERT_TRUE(second) << second.error().msg;
  EXPECT_GT(first->stats.junctions, 20u);
  EXPECT_EQ(first->stats.roundabouts, 1u);
  auto const bytes = read_bytes(f.index_path("first.cix"));
  EXPECT_FALSE(bytes.empty());
  EXPECT_EQ(bytes, read_bytes(f.index_path("second.cix")));
}

} // namespace
} // namespace chiasmos
