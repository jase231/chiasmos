// Roundabouts: an unnamed ring shares no vertex between the streets that enter
// it, so the ring is treated as one place.
#include <gtest/gtest.h>

#include "support/pbf_fixture.hpp"

namespace chiasmos {
namespace {

using namespace test;

// A square ring of `radius` units around the origin (nodes 10..13, a closed
// way), and three streets entering it from the east, north and west at nodes
// 10, 11 and 12. The fourth ring node (13, south) has no street.
PbfFixture& roundabout(PbfFixture& f, double radius, Tags ring) {
  f.node(10, radius, 0)
      .node(11, 0, radius)
      .node(12, -radius, 0)
      .node(13, 0, -radius);
  f.node(20, 3 * radius, 0).node(21, 0, 3 * radius).node(22, -3 * radius, 0);
  f.way(30, { 10, 11, 12, 13, 10 }, std::move(ring));
  f.way(31, { 20, 10 }, road("Main Street"))
      .way(32, { 21, 11 }, road("Oak Street"))
      .way(33, { 22, 12 }, road("Elm Street"));
  return f;
}

Tags unnamed_ring() {
  return { { "highway", "residential" }, { "junction", "roundabout" } };
}

TEST(BuildRoundabout, UnnamedRingPairsEveryArm) {
  PbfFixture f;
  roundabout(f, 3, unnamed_ring());
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->stats.roundabouts, 1u);
  EXPECT_EQ(built->stats.junctions,
            3u); // Main-Oak, Main-Elm, Oak-Elm: nothing else is named
  std::vector<std::int64_t> evidence;
  for (auto const& [a, b] : { std::pair{ "Main Street", "Oak Street" },
                              { "Main Street", "Elm Street" },
                              { "Oak Street", "Elm Street" } }) {
    auto const j = only_junction(built->index, a, b);
    ASSERT_TRUE(j) << a << " & " << b;
    EXPECT_EQ(static_cast<JunctionKind>(j->kind), JunctionKind::Roundabout);
    EXPECT_TRUE(j->osm_node >= 10 && j->osm_node <= 13)
        << "evidence is a ring node";
    evidence.push_back(j->osm_node);
  }
  EXPECT_EQ(evidence[0], evidence[1]); // one place, one evidence node
  EXPECT_EQ(evidence[1], evidence[2]);
}

// Variations on the basic ring above: which loops count as one place.
TEST(BuildRoundabout, WhichRingsCount) {
  struct Case {
    char const* what;
    double radius;
    Tags ring;
    std::uint64_t roundabouts, junctions;
  };
  Case const cases[] = {
    { "the same loop without junction=roundabout: no shared node between the "
      "arms",
      3,
      { { "highway", "residential" } },
      0,
      0 },
    { "about 260 m across: an ordinary road", 12, unnamed_ring(), 0, 0 },
    { "unnamed service roads are dropped unless they are ring members",
      3,
      { { "highway", "service" }, { "junction", "roundabout" } },
      1,
      3 },
  };
  for (Case const& c : cases) {
    SCOPED_TRACE(c.what);
    PbfFixture f;
    roundabout(f, c.radius, c.ring);
    auto const built = f.build();
    ASSERT_TRUE(built) << built.error().msg;
    EXPECT_EQ(built->stats.roundabouts, c.roundabouts);
    EXPECT_EQ(built->stats.junctions, c.junctions);
  }
}

TEST(BuildRoundabout, CircularRingsCountAndSplitRingsAreOneRing) {
  PbfFixture f;
  // The same ring as two half-ways sharing nodes 10 and 12, tagged
  // junction=circular.
  f.node(10, 3, 0)
      .node(11, 0, 3)
      .node(12, -3, 0)
      .node(13, 0, -3)
      .node(20, 9, 0)
      .node(21, 0, 9)
      .node(22, -9, 0);
  Tags const ring{ { "highway", "residential" }, { "junction", "circular" } };
  f.way(30, { 10, 11, 12 }, ring).way(34, { 12, 13, 10 }, ring);
  f.way(31, { 20, 10 }, road("Main Street"))
      .way(32, { 21, 11 }, road("Oak Street"))
      .way(33, { 22, 12 }, road("Elm Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->stats.roundabouts, 1u);
  EXPECT_EQ(built->stats.junctions, 3u);
}

TEST(BuildRoundabout, ANamedRingMeetsItsArmsToo) {
  PbfFixture f;
  roundabout(f, 3,
             { { "highway", "residential" },
               { "junction", "roundabout" },
               { "name", "Fountain Circle" } });
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  for (char const* street : { "Main Street", "Oak Street", "Elm Street" })
    EXPECT_FALSE(junctions_of(built->index, "Fountain Circle", street).empty())
        << street;
  auto const arms = junctions_of(built->index, "Main Street", "Oak Street");
  ASSERT_EQ(arms.size(), 1u);
  EXPECT_EQ(static_cast<JunctionKind>(arms.front().kind),
            JunctionKind::Roundabout);
}

} // namespace
} // namespace chiasmos
