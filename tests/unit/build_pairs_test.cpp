// The mask rule end to end: tiny synthetic extracts in, junction records out.
#include <gtest/gtest.h>

#include "support/pbf_fixture.hpp"

namespace chiasmos {
namespace {

using namespace test;

// Main Street runs west-east through node 2 at the origin, Oak Street
// north-south through it.
PbfFixture& cross_roads(PbfFixture& f) {
  return f.node(1, -10, 0).node(2, 0, 0).node(3, 10, 0).node(4, 0, -10).node(
      5, 0, 10);
}

TEST(BuildPairs, PlainCrossing) {
  PbfFixture f;
  cross_roads(f)
      .way(10, { 1, 2, 3 }, road("Main Street", "primary"))
      .way(11, { 4, 2, 5 }, road("Oak Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const j = only_junction(built->index, "Main Street", "Oak Street");
  ASSERT_TRUE(j);
  EXPECT_EQ(j->osm_node, 2);
  EXPECT_EQ((Coord{ j->x, j->y }), PbfFixture::at(0, 0));
  EXPECT_EQ(static_cast<JunctionKind>(j->kind), JunctionKind::Node);
  EXPECT_EQ(side_of(built->index, *j, "Main Street").road, RoadClass::Primary);
  EXPECT_EQ(side_of(built->index, *j, "Oak Street").road,
            RoadClass::Residential);
  EXPECT_EQ(built->stats.junctions, 1u);
  EXPECT_EQ(built->stats.vertices, 1u); // the crossing and the four way ends
  EXPECT_EQ(built->stats.road_ways, 2u);
}

TEST(BuildPairs, TJunction) {
  PbfFixture f;
  cross_roads(f)
      .way(10, { 1, 2, 3 }, road("Main Street"))
      .way(11, { 4, 2 }, road("Oak Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const j = only_junction(built->index, "Main Street", "Oak Street");
  ASSERT_TRUE(j);
  EXPECT_EQ(j->osm_node, 2);
  EXPECT_EQ(static_cast<JunctionKind>(j->kind),
            JunctionKind::Node); // a through road with a side street is not a
                                 // name change
}

TEST(BuildPairs, DeadEndsAndNearMissesMakeNoJunction) {
  PbfFixture f;
  // Oak stops one node short of Main: close on the map, but they share no node.
  f.node(1, -10, 0).node(2, 0, 0).node(3, 10, 0).node(4, 0, -10).node(5, 0, -1);
  f.way(10, { 1, 2, 3 }, road("Main Street"))
      .way(11, { 4, 5 }, road("Oak Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_TRUE(junctions_of(built->index, "Main Street", "Oak Street").empty());
  EXPECT_EQ(built->stats.junctions, 0u);
}

TEST(BuildPairs, AliasesOfOneRoadDoNotCross) {
  PbfFixture f;
  // West Broad Street is also US 250: the same two ways carry both names, and
  // Glenside Drive crosses at node 2.
  cross_roads(f).way(10, { 1, 2 },
                     { { "highway", "primary" },
                       { "name", "West Broad Street" },
                       { "ref", "US 250" } });
  f.way(11, { 2, 3 },
        { { "highway", "primary" },
          { "name", "West Broad Street" },
          { "ref", "US 250" } });
  f.way(12, { 4, 2, 5 }, road("Glenside Drive"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_TRUE(
      junctions_of(built->index, "West Broad Street", "US 250").empty());
  EXPECT_TRUE(
      only_junction(built->index, "West Broad Street", "Glenside Drive"));
  EXPECT_TRUE(only_junction(built->index, "US 250", "Glenside Drive"));
  EXPECT_EQ(built->stats.junctions, 2u);
}

TEST(BuildPairs, NameKindsAndAlternates) {
  PbfFixture f;
  cross_roads(f).way(10, { 1, 2, 3 }, road("Main Street"));
  f.way(11, { 4, 2, 5 },
        { { "highway", "residential" },
          { "name", "Oak Street" },
          { "alt_name", "Maple Street" },
          { "old_name", "Elm Street" } });
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const& ix = built->index;
  auto const oak = only_junction(ix, "Main Street", "Oak Street"),
             maple = only_junction(ix, "Main Street", "Maple Street"),
             elm = only_junction(ix, "Main Street", "Elm Street");
  ASSERT_TRUE(oak && maple && elm);
  EXPECT_EQ(side_of(ix, *oak, "Oak Street").kind, NameKind::Primary);
  EXPECT_EQ(side_of(ix, *maple, "Maple Street").kind, NameKind::Alt);
  EXPECT_EQ(side_of(ix, *elm, "Elm Street").kind, NameKind::Old);
  EXPECT_EQ(side_of(ix, *oak, "Main Street").kind, NameKind::Primary);
}

TEST(BuildPairs, ASplitWayDoesNotMeetItself) {
  PbfFixture f;
  // Main Street is two ways joined at node 2, where Oak Street crosses: Main
  // meets Oak, never Main.
  cross_roads(f)
      .way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 2, 3 }, road("Main Street"))
      .way(12, { 4, 2, 5 }, road("Oak Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_TRUE(only_junction(built->index, "Main Street", "Oak Street"));
  EXPECT_EQ(built->stats.junctions, 1u);
}

TEST(BuildPairs, OneRoadChangingNameIsATransitionUntilANamedSpurJoins) {
  PbfFixture f;
  cross_roads(f)
      .way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 2, 3 }, road("Elm Street"));
  auto const plain = f.build({ .region = "test/region" }, "plain.cix");
  ASSERT_TRUE(plain) << plain.error().msg;
  auto const i = only_junction(plain->index, "Main Street", "Elm Street");
  ASSERT_TRUE(i);
  EXPECT_EQ(static_cast<JunctionKind>(i->kind), JunctionKind::NameTransition);

  // An unnamed connector is kept in the graph but is not a named arm: Main ->
  // Elm is still one road changing name.
  f.way(12, { 2, 4 }, { { "highway", "residential" } });
  auto const with_spur = f.build();
  ASSERT_TRUE(with_spur) << with_spur.error().msg;
  auto const j = only_junction(with_spur->index, "Main Street", "Elm Street");
  ASSERT_TRUE(j);
  EXPECT_EQ(static_cast<JunctionKind>(j->kind), JunctionKind::NameTransition);

  // The same spur with a name is a third street: three named arms, so a plain
  // meeting.
  f.way(12, { 2, 4 }, road("Oak Street")); // replaces the unnamed spur
  auto const with_street = f.build({ .region = "test/region" }, "named.cix");
  ASSERT_TRUE(with_street) << with_street.error().msg;
  auto const k = only_junction(with_street->index, "Main Street", "Elm Street");
  ASSERT_TRUE(k);
  EXPECT_EQ(static_cast<JunctionKind>(k->kind), JunctionKind::Node);
}

TEST(BuildPairs, ALoopEndingOnAStreetIsATNotATransition) {
  PbfFixture f;
  // Elm Circle is a closed way whose closure node (2) is the end of Main
  // Street: its two arms make three named arms.
  f.node(1, -10, 0).node(2, 0, 0).node(3, 5, 5).node(4, 5, -5);
  f.way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 2, 3, 4, 2 }, road("Elm Circle"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const j = only_junction(built->index, "Main Street", "Elm Circle");
  ASSERT_TRUE(j);
  EXPECT_EQ(static_cast<JunctionKind>(j->kind), JunctionKind::Node);
}

TEST(BuildPairs, TwoNamesSharingAStretchAreConcurrent) {
  PbfFixture f;
  // Main Street runs 1-2-3 and Elm Street runs 4-2-3 on the same way 2-3, which
  // carries both names. At node 2 each name continues on a way the other does
  // not, and the masks overlap.
  f.node(1, -10, 0).node(2, 0, 0).node(3, 10, 0).node(4, -10, 8);
  f.way(10, { 1, 2 }, road("Main Street"))
      .way(11, { 2, 3 },
           { { "highway", "primary" }, { "name", "Main Street;Elm Street" } })
      .way(12, { 4, 2 }, road("Elm Street"));
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  auto const j = only_junction(built->index, "Main Street", "Elm Street");
  ASSERT_TRUE(j);
  EXPECT_EQ(j->osm_node, 2);
  EXPECT_EQ(static_cast<JunctionKind>(j->kind), JunctionKind::Concurrency);
  EXPECT_EQ(side_of(built->index, *j, "Main Street").road,
            RoadClass::Primary); // the best road carrying each name
}

TEST(BuildPairs, ANameInsideAnothersFootprintMakesNoJunction) {
  PbfFixture f;
  // Way 11 leaves Main Street at node 2 carrying both Main Street and Oak
  // Street: Oak never continues anywhere Main does not.
  f.node(1, -10, 0).node(2, 0, 0).node(3, 10, 0).node(4, 0, 10);
  f.way(10, { 1, 2, 3 }, road("Main Street"))
      .way(11, { 2, 4 },
           { { "highway", "residential" },
             { "name", "Main Street" },
             { "alt_name", "Oak Street" } });
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->stats.junctions, 0u);
}

TEST(BuildPairs, KeepsTheNamedWaysOfAHugeVertex) {
  PbfFixture f;
  f.node(1, 0, 0);
  constexpr int kSpokes = 70;
  for (int i = 0; i < kSpokes; ++i) {
    std::string const name = std::string("Zq") +
                             static_cast<char>('a' + i / 26) +
                             static_cast<char>('a' + i % 26) + " Street";
    f.node(100 + i, 10 + i, 10).way(200 + i, { 1, 100 + i }, road(name));
  }
  auto const built = f.build();
  ASSERT_TRUE(built) << built.error().msg;
  EXPECT_EQ(built->stats.junctions,
            64u * 63 / 2); // one bit per way: 64 are kept
}

} // namespace
} // namespace chiasmos
