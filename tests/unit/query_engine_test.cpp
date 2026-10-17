// Engine tests on hand-built indexes. Scores are checked against the products
// written in the comments.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "query/engine.hpp"
#include "query/resolve.hpp"
#include "support/index_fixture.hpp"

namespace chiasmos::query {
namespace {

using norm::Dir;
using norm::TypePos;
using namespace test;

Coord const kP = to_coord(
    -77.4, 37.5); // a point in Virginia; everything else is offset from it

Street named(std::string_view full, std::string_view core,
             std::string_view display, Dir pre, std::uint16_t type) {
  return { std::string(full),
           std::string(core),
           { .pre_dir = pre, .type = type, .type_pos = TypePos::Suffix },
           std::string(display) };
}
Street n_main() {
  return named("n main st", "main", "North Main Street", Dir::N, kSt);
}
Street s_main() {
  return named("s main st", "main", "South Main Street", Dir::S, kSt);
}
Street n_parham() {
  return named("n parham rd", "parham", "North Parham Road", Dir::N, kRd);
}
Street e_parham() {
  return named("e parham rd", "parham", "East Parham Road", Dir::E, kRd);
}
Street ordinal(std::string_view full, Dir pre, std::uint16_t type) {
  return named(full, "5th", full, pre, type);
}

Street route(std::string_view full, std::string_view display,
             norm::RouteNet net) {
  return {
    std::string(full), std::string(full), { .route = net }, std::string(display)
  };
}

Street with_road(Street s, RoadClass road) {
  s.road = road;
  return s;
}
Street with_kind(Street s, NameKind kind) {
  s.kind = kind;
  return s;
}

// Designated initializers of Request trip -Wmissing-field-initializers (its
// strings and optionals have no default member initializer).
Request streets(std::string a, std::string b = "") {
  Request r;
  r.street1 = std::move(a), r.street2 = std::move(b);
  return r;
}
Request free_form(std::string q) {
  Request r;
  r.q = std::move(q);
  return r;
}

Result run(index::Index const& ix, Street const& s1, Street const& s2,
           Request const& req = {}) {
  return resolve(ix, req, { query_side(s1), query_side(s2) });
}

bool mentions(Result const& r, std::string_view text) {
  return std::ranges::any_of(r.notes, [&](std::string const& n) {
    return n.find(text) != std::string::npos;
  });
}

TEST(QueryEngine, ExactCrossing) {
  TestIndex t;
  t.add(main_st(), oak_ave(), kP, 100);
  Result const r = run(t.open(), main_st(), oak_ave());
  ASSERT_EQ(r.status, Status::Ok);
  ASSERT_EQ(r.candidates.size(), 1u);
  Candidate const& c = r.candidates[0];
  EXPECT_EQ(c.rep, kP);
  EXPECT_FLOAT_EQ(c.score,
                  1.0f); // 1 x 1 x 1: both names agree in every component
  EXPECT_EQ(c.confidence, Confidence::High);
  EXPECT_EQ(c.kind, JunctionKind::Node);
  EXPECT_EQ(c.street1, "Main Street");
  EXPECT_EQ(c.street2, "Oak Avenue");
  EXPECT_EQ(c.osm_nodes, std::vector<std::int64_t>{ 100 });
  EXPECT_FLOAT_EQ(c.distance_to_near_m, -1.0f);
}

TEST(QueryEngine, DirectionalAgreementRanksTheRightQuadrantFirst) {
  TestIndex t;
  t.add(n_main(), oak_ave(), kP, 1);
  t.add(s_main(), oak_ave(), offset(kP, 500), 2);
  Result const r = run(t.open(), n_main(), oak_ave());
  ASSERT_EQ(r.status, Status::Ok); // 0.6 < 0.9 x 1.0: not a rival
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_FLOAT_EQ(r.candidates[0].score, 1.0f);
  EXPECT_EQ(r.candidates[0].street1, "North Main Street");
  EXPECT_FLOAT_EQ(r.candidates[1].score, 0.6f); // direction conflict
  EXPECT_EQ(r.candidates[1].street1, "South Main Street");
  EXPECT_EQ(r.candidates[1].confidence, Confidence::Medium);
}

TEST(QueryEngine, AQueryWithoutDirectionCannotChooseBetweenQuadrants) {
  TestIndex t;
  t.add(n_main(), oak_ave(), kP, 1);
  t.add(s_main(), oak_ave(), offset(kP, 500), 2);
  Result const r = run(t.open(), main_st(), oak_ave());
  EXPECT_EQ(r.status, Status::Ambiguous);
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_FLOAT_EQ(r.candidates[0].score,
                  0.97f); // the query says less than the record
  EXPECT_FLOAT_EQ(r.candidates[1].score, 0.97f);
  EXPECT_EQ(r.candidates[0].confidence,
            Confidence::Medium); // never High while another place is as good
}

// The factors themselves are pinned in query_scoring_test; here only that they
// multiply and map onto the confidence bands.
TEST(QueryEngine, ConfidenceBands) {
  TestIndex t;
  t.add(s_main(), oak_ave(), kP, 1);
  struct Case {
    Street query;
    float score;
    Confidence confidence;
  };
  Case const cases[] = {
    { s_main(), 1.0f, Confidence::High },
    { named("s main rd", "main", "South Main Road", Dir::S, kRd), 0.75f,
      Confidence::Medium }, // type conflict
    { named("n main rd", "main", "North Main Road", Dir::N, kRd), 0.6f * 0.75f,
      Confidence::Low }, // direction x type conflict = 0.45
  };
  for (Case const& c : cases) {
    SCOPED_TRACE(c.query.full);
    Result const r = run(t.open(), c.query, oak_ave());
    ASSERT_EQ(r.candidates.size(), 1u);
    EXPECT_FLOAT_EQ(r.candidates[0].score, c.score);
    EXPECT_EQ(r.candidates[0].confidence, c.confidence);
  }
}

TEST(QueryEngine, NumberedStreetsDropAnotherQuadrantOrType) {
  TestIndex t;
  t.add(ordinal("n 5th st", Dir::N, kSt), oak_ave(), kP, 1);
  t.add(ordinal("s 5th st", Dir::S, kSt), oak_ave(), offset(kP, 500), 2);
  t.add(ordinal("5th ave", Dir::None, kAve), oak_ave(), offset(kP, 1000), 3);
  Result const r = run(t.open(), ordinal("n 5th st", Dir::N, kSt), oak_ave());
  ASSERT_EQ(
      r.candidates.size(),
      1u); // "S 5th St" and "5th Ave" are other streets, not weaker matches
  EXPECT_EQ(r.candidates[0].osm_nodes, std::vector<std::int64_t>{ 1 });
  EXPECT_EQ(r.status, Status::Ok);
}

TEST(QueryEngine, NameKindFactors) {
  TestIndex t;
  t.add(with_kind(main_st(), NameKind::Alt), oak_ave(), kP, 1);
  t.add(with_kind(n_main(), NameKind::Old), with_kind(oak_ave(), NameKind::Alt),
        offset(kP, 1000), 2);
  Result const r = run(t.open(), main_st(), oak_ave());
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_FLOAT_EQ(r.candidates[0].score, 0.97f); // alternate name: 0.97 x 1
  EXPECT_FLOAT_EQ(r.candidates[1].score,
                  0.97f * 0.85f * 0.97f); // direction (query says less) x old
                                          // name x alternate name = 0.7996
}

TEST(QueryEngine, StreetOrderFollowsTheRequest) {
  {
    TestIndex t;
    t.add(main_st(), oak_ave(), kP,
          100); // the file stores the pair in core order: main, oak
    Result const r = run(t.open(), oak_ave(), main_st());
    ASSERT_EQ(r.status, Status::Ok);
    ASSERT_EQ(r.candidates.size(), 1u);
    EXPECT_EQ(r.candidates[0].street1, "Oak Avenue");
    EXPECT_EQ(r.candidates[0].street2, "Main Street");
    EXPECT_FLOAT_EQ(r.candidates[0].score, 1.0f);
  }
  TestIndex t; // the same core on both sides: either side may be stored first
  t.add(n_parham(), e_parham(), kP,
        7); // stored as (e parham rd, n parham rd): the other side comes first
  Result const ne = run(t.open(), n_parham(), e_parham());
  ASSERT_EQ(ne.status, Status::Ok);
  EXPECT_FLOAT_EQ(ne.candidates.at(0).score, 1.0f);
  EXPECT_EQ(ne.candidates[0].street1, "North Parham Road");
  EXPECT_EQ(ne.candidates[0].street2, "East Parham Road");
  Result const en = run(t.open(), e_parham(), n_parham());
  ASSERT_EQ(en.status, Status::Ok);
  EXPECT_FLOAT_EQ(en.candidates.at(0).score, 1.0f);
  EXPECT_EQ(en.candidates[0].street1, "East Parham Road");
  EXPECT_EQ(en.candidates[0].street2, "North Parham Road");
  EXPECT_EQ(en.candidates.size(), 1u); // one junction, not one per assignment
}

// Route queries reach their records through query_core_alternatives, and the
// alternative's prior is the score.
TEST(QueryEngine, RouteQueriesScoreTheirAlternativesPrior) {
  TestIndex t;
  t.add(route("#i 64", "I 64", norm::RouteNet::Interstate), main_st(), kP, 1);
  t.add(route("#i 295", "I 295", norm::RouteNet::Interstate), main_st(),
        offset(kP, 5000), 2);
  t.add(route("#us 295", "US 295", norm::RouteNet::US), main_st(),
        offset(kP, 10000), 3);
  t.add(route("#us 250", "US 250", norm::RouteNet::US), main_st(),
        offset(kP, 15000), 4);
  struct Expected {
    std::string_view street1;
    float score;
  };
  struct Case {
    std::string_view query;
    Status status;
    std::vector<Expected> candidates;
  };
  Case const cases[] = {
    { "I-64", Status::Ok, { { "I 64", 1.0f } } }, // exact route core, prior 1
    { "295",
      Status::Ambiguous,
      { { "I 295", 0.8f },
        { "US 295", 0.8f } } }, // a bare number tries every network at 0.8
    { "Highway 250",
      Status::Ok,
      { { "US 250",
          0.9f } } }, // a generic highway tries the named networks at 0.9
  };
  for (Case const& c : cases) {
    SCOPED_TRACE(c.query);
    norm::Normalized q;
    norm::normalize_street(c.query, q);
    Result const r = resolve(t.open(), Request{}, { q, query_side(main_st()) });
    EXPECT_EQ(r.status, c.status);
    ASSERT_EQ(r.candidates.size(), c.candidates.size());
    for (std::size_t i = 0; i < c.candidates.size(); ++i) {
      EXPECT_EQ(r.candidates[i].street1, c.candidates[i].street1);
      EXPECT_FLOAT_EQ(
          r.candidates[i].score,
          c.candidates[i].score); // routes skip directional and type agreement
    }
  }
}

TEST(QueryEngine,
     TheSameJunctionReachedThroughTwoAlternativeCoresIsNotGrouped) {
  TestIndex t;
  t.add(route("#i 295", "I 295", norm::RouteNet::Interstate),
        route("#us 295", "US 295", norm::RouteNet::US), kP, 9);
  norm::Normalized bare;
  norm::normalize_street("295", bare);
  Result const r = resolve(
      t.open(), Request{},
      { bare,
        bare }); // (#i, #us) and (#us, #i) both reach the one stored junction
  ASSERT_EQ(r.status, Status::Ok);
  ASSERT_EQ(r.candidates.size(), 1u);
  EXPECT_EQ(r.candidates[0].kind, JunctionKind::Node);
  EXPECT_FLOAT_EQ(r.candidates[0].score, 0.8f * 0.8f);
  EXPECT_EQ(r.candidates[0].osm_nodes, std::vector<std::int64_t>{ 9 });
}

TEST(QueryEngine, TrimmedSuffixNeedsTheDroppedWordAsTheRecordsType) {
  norm::Normalized query;
  norm::normalize_street("Magnolia Ridge Dr", query);
  {
    TestIndex t; // the OSM name ends in the dropped word: "Magnolia Ridge" is
                 // core "magnolia", type ridge
    t.add(raw_street("Magnolia Ridge"), oak_ave(), kP, 1);
    Result const r =
        resolve(t.open(), Request{}, { query, query_side(oak_ave()) });
    ASSERT_EQ(r.status, Status::Ok);
    EXPECT_EQ(r.candidates.at(0).street1, "Magnolia Ridge");
    EXPECT_LT(r.candidates[0].score,
              1.0f); // prior 0.85 x the street-type factor
  }
  {
    TestIndex t; // "Magnolia Road" shares the core but not the dropped word: a
                 // different street
    t.add(raw_street("Magnolia Rd"), oak_ave(), kP, 1);
    Result const r =
        resolve(t.open(), Request{}, { query, query_side(oak_ave()) });
    EXPECT_EQ(r.status,
              Status::NoStreetMatch); // a trimmed guess alone does not prove
                                      // that the street exists
    EXPECT_TRUE(mentions(r, "street 1"));
  }
}

TEST(QueryEngine, MergesVerticesOfADividedRoad) {
  auto const main_p = with_road(main_st(), RoadClass::Primary),
             oak_p = with_road(oak_ave(), RoadClass::Primary);
  TestIndex t;
  t.add(main_p, oak_p, kP, 1);
  t.add(main_p, oak_p, offset(kP, 10), 2);
  t.add(main_p, oak_p, offset(kP, 50), 3); // all within 90 m (primary)
  Result const r = run(t.open(), main_st(), oak_ave());
  ASSERT_EQ(r.status, Status::Ok);
  ASSERT_EQ(r.candidates.size(), 1u);
  Candidate const& c = r.candidates[0];
  EXPECT_EQ(c.kind, JunctionKind::Grouped);
  EXPECT_LT(dist_m(c.rep, offset(kP, 20)),
            0.5); // (0 + 10 + 50) / 3, not the 27.5 of a mean of means
  EXPECT_EQ(c.osm_nodes,
            (std::vector<std::int64_t>{ 1, 2, 3 })); // union, sorted
  EXPECT_FLOAT_EQ(c.score, 1.0f);
}

TEST(QueryEngine, MergeRadiusFollowsTheMostImportantRoad) {
  struct Case {
    RoadClass a, b;
    double gap_m;
    std::size_t places;
  };
  Case const cases[] = {
    { RoadClass::Residential, RoadClass::Residential, 50, 1 },
    { RoadClass::Residential, RoadClass::Residential, 70, 2 }, // 60 m
    { RoadClass::Primary, RoadClass::Residential, 80, 1 },
    { RoadClass::Secondary, RoadClass::Residential, 100,
      2 }, // 90 m, either side
    { RoadClass::Residential, RoadClass::Trunk, 140, 1 },
    { RoadClass::Motorway, RoadClass::Primary, 160, 2 }, // 150 m
  };
  for (Case const& k : cases) {
    SCOPED_TRACE(testing::Message()
                 << "classes " << static_cast<int>(k.a) << "/"
                 << static_cast<int>(k.b) << " gap " << k.gap_m);
    TestIndex t;
    t.add(with_road(main_st(), k.a), with_road(oak_ave(), k.b), kP, 1);
    t.add(with_road(main_st(), k.a), with_road(oak_ave(), k.b),
          offset(kP, k.gap_m), 2);
    EXPECT_EQ(run(t.open(), main_st(), oak_ave()).candidates.size(), k.places);
  }
}

TEST(QueryEngine, SplitNamesAtOneNodeAreOnePlace) {
  TestIndex
      t; // "N Main St" and "S Main St" both meet Oak Ave at the same vertex
  t.add(n_main(), oak_ave(), kP, 1);
  t.add(s_main(), oak_ave(), kP, 2);
  Result const r = run(t.open(), main_st(), oak_ave());
  ASSERT_EQ(r.status, Status::Ok);
  ASSERT_EQ(r.candidates.size(), 1u);
  EXPECT_EQ(r.candidates[0].kind, JunctionKind::Grouped);
  EXPECT_EQ(r.candidates[0].osm_nodes, (std::vector<std::int64_t>{ 1, 2 }));
  EXPECT_EQ(r.candidates[0].rep, kP);
}

TEST(QueryEngine,
     SeparatePlacesOfEqualScoreAreAmbiguousBeyondTheAmbiguityDistance) {
  struct Case {
    double gap_m;
    Status status;
  };
  for (Case const& k :
       { Case{ 100, Status::Ok }, Case{ 200, Status::Ambiguous },
         Case{ 500, Status::Ambiguous } }) {
    SCOPED_TRACE(k.gap_m);
    TestIndex t; // residential: 100 m is beyond the 60 m merge radius, so these
                 // are two places
    t.add(main_st(), oak_ave(), kP, 1);
    t.add(main_st(), oak_ave(), offset(kP, k.gap_m), 2);
    Result const r = run(t.open(), main_st(), oak_ave());
    EXPECT_EQ(r.status, k.status); // within 150 m the runner-up is the same
                                   // neighbourhood, not a rival
    EXPECT_EQ(r.candidates.size(), 2u);
  }
}

TEST(QueryEngine, NearDemotesFarJunctions) {
  TestIndex t;
  t.add(main_st(), oak_ave(), kP, 1);
  t.add(main_st(), oak_ave(), offset(kP, 5000), 2);
  Request req;
  req.near = kP;
  Result r = run(t.open(), main_st(), oak_ave(), req);
  ASSERT_EQ(r.status, Status::Ok); // exp(-5/3) = 0.189 is no rival of 1.0
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_FLOAT_EQ(r.candidates[0].score, 1.0f);
  EXPECT_NEAR(r.candidates[0].distance_to_near_m, 0.0, 0.5);
  EXPECT_NEAR(r.candidates[1].score, std::exp(-5000.0 / 3000.0), 1e-4);
  EXPECT_NEAR(r.candidates[1].distance_to_near_m, 5000.0, 1.0);

  req.near = offset(kP, 5000); // now the other junction is the near one
  r = run(t.open(), main_st(), oak_ave(), req);
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_EQ(r.candidates[0].osm_nodes, std::vector<std::int64_t>{ 2 });

  req.near = offset(
      kP,
      100000); // far from both: 0.05 each, below the default min_score of 0.15
  r = run(t.open(), main_st(), oak_ave(), req);
  EXPECT_EQ(r.status, Status::NoIntersection);
  EXPECT_TRUE(mentions(r, "min_score"));
  req.min_score = 0;
  r = run(t.open(), main_st(), oak_ave(), req);
  ASSERT_EQ(r.candidates.size(), 2u);
  EXPECT_FLOAT_EQ(r.candidates[0].score, 0.05f); // floored, not zero
}

TEST(QueryEngine, BboxIsAHardFilter) {
  TestIndex t;
  t.add(main_st(), oak_ave(), kP, 1);
  t.add(main_st(), oak_ave(), offset(kP, 5000), 2);
  Coord const b = offset(kP, 5000);
  Request req;
  req.bbox = Bbox{ offset(b, -100, -100), offset(b, 100, 100) };
  Result r = run(t.open(), main_st(), oak_ave(), req);
  ASSERT_EQ(
      r.status,
      Status::Ok); // the other junction is not even a rival: it is filtered out
  ASSERT_EQ(r.candidates.size(), 1u);
  EXPECT_EQ(r.candidates[0].osm_nodes, std::vector<std::int64_t>{ 2 });

  req.bbox =
      Bbox{ offset(kP, 1000, -100), offset(kP, 2000, 100) }; // contains neither
  r = run(t.open(), main_st(), oak_ave(), req);
  EXPECT_EQ(r.status, Status::NoIntersection);
  EXPECT_TRUE(mentions(r, "bbox"));
}

TEST(QueryEngine, JunctionKindFactorsMinScoreAndLimit) {
  TestIndex t;
  t.add(main_st(), oak_ave(), kP, 1);
  t.add(main_st(), oak_ave(), offset(kP, 1000), 2,
        JunctionKind::NameTransition);
  t.add(main_st(), oak_ave(), offset(kP, 2000), 3, JunctionKind::Concurrency);
  Result const all = run(t.open(), main_st(), oak_ave());
  EXPECT_EQ(all.status, Status::Ok); // 0.85 < 0.9 x 1
  ASSERT_EQ(all.candidates.size(), 3u);
  EXPECT_FLOAT_EQ(all.candidates[0].score, 1.0f);
  EXPECT_FLOAT_EQ(all.candidates[1].score, 0.85f);
  EXPECT_EQ(all.candidates[1].kind, JunctionKind::NameTransition);
  EXPECT_FLOAT_EQ(all.candidates[2].score, 0.80f);
  EXPECT_EQ(all.candidates[2].kind, JunctionKind::Concurrency);

  auto const count = [&](int limit, float min_score) {
    Request req;
    req.limit = limit, req.min_score = min_score;
    return run(t.open(), main_st(), oak_ave(), req).candidates.size();
  };
  EXPECT_EQ(count(2, 0.15f), 2u);
  EXPECT_EQ(count(0, 0.15f), 1u); // at least one
  EXPECT_EQ(count(std::numeric_limits<int>::max(), 0.15f),
            3u); // a huge limit is clamped, not multiplied into overflow
  EXPECT_EQ(count(5, 0.82f), 2u);
  EXPECT_EQ(count(5, 0.9f), 1u);
  Request strict;
  strict.min_score = 1.1f;
  Result const none = run(t.open(), main_st(), oak_ave(), strict);
  EXPECT_EQ(none.status, Status::NoIntersection);
  EXPECT_TRUE(none.candidates.empty());
}

TEST(QueryEngine, ManyPlacesAreCappedAndStillAnswer) {
  TestIndex t; // 3000 separate places, 100 m apart: past merge_hits' place cap
               // and past the 32 KiB stack buffer
  for (int i = 0; i < 3000; ++i)
    t.add(main_st(), oak_ave(), offset(kP, 100.0 * i), i);
  Result const r = run(t.open(), main_st(), oak_ave());
  EXPECT_EQ(r.status, Status::Ambiguous);
  EXPECT_EQ(r.candidates.size(), 5u); // the default limit
  EXPECT_FLOAT_EQ(r.candidates[0].score, 1.0f);
}

TEST(QueryEngine, NoStreetMatchNamesTheMissingSide) {
  TestIndex t;
  t.add(main_st(), named("elm ave", "elm", "Elm Avenue", Dir::None, kAve), kP,
        1);
  t.add(oak_ave(), named("pine ave", "pine", "Pine Avenue", Dir::None, kAve),
        offset(kP, 1000), 2);
  Street const foo = named("foo st", "foo", "Foo Street", Dir::None, kSt),
               bar = named("bar st", "bar", "Bar Street", Dir::None, kSt);

  Result r = run(t.open(), foo, oak_ave());
  EXPECT_EQ(r.status, Status::NoStreetMatch);
  EXPECT_TRUE(mentions(r, "street 1 not found"));
  EXPECT_FALSE(mentions(r, "street 2"));
  r = run(t.open(), oak_ave(), foo);
  EXPECT_EQ(r.status, Status::NoStreetMatch);
  EXPECT_TRUE(mentions(r, "street 2 not found"));
  r = run(t.open(), foo, bar);
  EXPECT_EQ(r.status, Status::NoStreetMatch);
  EXPECT_EQ(r.notes.size(), 2u);
  EXPECT_TRUE(r.candidates.empty());
}

TEST(QueryEngine, NoIntersectionWhenBothStreetsExistButNeverMeet) {
  TestIndex t;
  t.add(main_st(), named("elm ave", "elm", "Elm Avenue", Dir::None, kAve), kP,
        1);
  t.add(oak_ave(), named("pine ave", "pine", "Pine Avenue", Dir::None, kAve),
        offset(kP, 1000), 2);
  Result const r = run(t.open(), main_st(), oak_ave());
  EXPECT_EQ(r.status, Status::NoIntersection);
  EXPECT_TRUE(mentions(r, "no junction"));
  EXPECT_TRUE(r.candidates.empty());
}

// Form -> status only; parse_freeform's handling of each string is in
// freeform_test.
TEST(QueryEngine, InputHandling) {
  TestIndex t;
  t.add(main_st(), oak_ave(), kP, 1);
  Engine const engine(t.open());
  Result const empty = engine.run(Request{});
  EXPECT_EQ(empty.status, Status::InvalidRequest);
  EXPECT_FALSE(empty.notes.empty());
  EXPECT_EQ(engine.run(free_form("&")).status, Status::InvalidRequest);
  EXPECT_EQ(engine.run(streets("Main St")).status,
            Status::NotAnIntersection); // one street is not an intersection
  EXPECT_EQ(engine.run(streets("", "Main St")).status,
            Status::NotAnIntersection);
  EXPECT_EQ(engine.run(free_form("Main St")).status, Status::NotAnIntersection);
}

// Integration: index and query both go through norm::normalize_street,
// whichever implementation is linked.
TEST(QueryEngine, EndToEndThroughTheNormalizer) {
  TestIndex t;
  t.add(raw_street("Glenside Dr"), raw_street("W Broad St", RoadClass::Primary),
        kP, 42);
  Engine const engine(t.open());
  for (Request const& req : { streets("Glenside Dr", "W Broad St"),
                              free_form("Glenside Dr & W Broad St") }) {
    Result const r = engine.run(req);
    ASSERT_EQ(r.status, Status::Ok);
    ASSERT_EQ(r.candidates.size(), 1u);
    EXPECT_EQ(r.candidates[0].rep, kP);
    EXPECT_EQ(r.candidates[0].street1, "Glenside Dr");
    EXPECT_EQ(r.candidates[0].street2, "W Broad St");
    EXPECT_GT(r.candidates[0].score, 0.9f);
    EXPECT_EQ(r.candidates[0].osm_nodes, std::vector<std::int64_t>{ 42 });
  }
  Result const reversed = engine.run(free_form("W Broad St & Glenside Dr"));
  ASSERT_EQ(reversed.status, Status::Ok);
  EXPECT_EQ(reversed.candidates.at(0).street1, "W Broad St");
  EXPECT_EQ(engine.run(streets("Glenside Dr", "Nowhere Ln")).status,
            Status::NoStreetMatch);
  Result const located =
      engine.run(free_form("Glenside Dr & W Broad St, Richmond VA"));
  ASSERT_EQ(located.status, Status::Ok);
  EXPECT_TRUE(mentions(
      located,
      "Richmond VA")); // the locality is not used, and the caller is told so
}

} // namespace
} // namespace chiasmos::query
