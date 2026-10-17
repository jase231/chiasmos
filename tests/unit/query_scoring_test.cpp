#include <gtest/gtest.h>

#include "query/scoring.hpp"
#include "support/index_fixture.hpp"

namespace chiasmos::query {
namespace {

using norm::Dir;
using test::kAve;
using test::kRd;
using test::kSt;

index::FullRec record(Dir pre = Dir::None, std::uint16_t type = 0,
                      Dir suf = Dir::None, std::uint8_t post_mod = 0) {
  index::FullRec r;
  r.pre_dir = static_cast<std::uint8_t>(pre);
  r.suf_dir = static_cast<std::uint8_t>(suf);
  r.type = type;
  r.type_pos = type != 0 ? static_cast<std::uint8_t>(norm::TypePos::Suffix) : 0;
  r.post_mod = post_mod;
  return r;
}

norm::Components query(Dir pre = Dir::None, std::uint16_t type = 0,
                       Dir suf = Dir::None, std::uint8_t post_mod = 0) {
  return { .pre_dir = pre,
           .suf_dir = suf,
           .type = type,
           .type_pos = type != 0 ? norm::TypePos::Suffix : norm::TypePos::None,
           .post_mod = post_mod };
}

float agree(norm::Components const& q, index::FullRec const& r,
            bool numbered = false) {
  return name_agreement(q, r, numbered);
}

TEST(QueryScoring, DirectionalFactors) {
  EXPECT_FLOAT_EQ(agree(query(Dir::N), record(Dir::N)), 1.0f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None), record(Dir::None)), 1.0f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None), record(Dir::N)),
                  0.97f); // the query says less
  EXPECT_FLOAT_EQ(agree(query(Dir::N), record(Dir::None)),
                  0.92f); // the record says less
  EXPECT_FLOAT_EQ(agree(query(Dir::N), record(Dir::S)), 0.60f); // conflict
  EXPECT_FLOAT_EQ(
      agree(query(Dir::None, 0, Dir::N), record(Dir::N)),
      0.98f); // "Main St N" vs "N Main St": same direction, other position
}

TEST(QueryScoring, TypeFactorsAndPostModifier) {
  EXPECT_FLOAT_EQ(agree(query(Dir::None, kSt), record(Dir::None, kSt)), 1.0f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None, 0), record(Dir::None, kSt)), 0.97f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None, kSt), record(Dir::None, 0)), 0.95f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None, kSt), record(Dir::None, kRd)), 0.75f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None, kSt, Dir::None, norm::ALT),
                        record(Dir::None, kSt)),
                  0.85f);
  // Independent factors multiply: direction conflict 0.6 x type conflict 0.75 x
  // modifier mismatch 0.85.
  EXPECT_FLOAT_EQ(
      agree(query(Dir::N, kSt, Dir::None, norm::BUS), record(Dir::S, kRd)),
      0.6f * 0.75f * 0.85f);
}

TEST(QueryScoring, RoutesIgnoreDirectionalsAndTypeButNotModifiers) {
  index::FullRec route = record(
      Dir::None, 0,
      Dir::E); // "I-64 East": the trailing direction is a travel direction
  route.route = static_cast<std::uint8_t>(norm::RouteNet::Interstate);
  norm::Components q = query(Dir::None, 0, Dir::W);
  q.route = norm::RouteNet::Interstate;
  EXPECT_FLOAT_EQ(agree(q, route), 1.0f);
  q.post_mod = norm::BUS;
  EXPECT_FLOAT_EQ(agree(q, route), 0.85f);
  // A route record answers a plain query too (bare number "295"): still no
  // directional or type comparison.
  EXPECT_FLOAT_EQ(agree(query(Dir::N, kSt), route), 1.0f);
}

TEST(QueryScoring, NumberedStreetDropsConflictsInsteadOfPenalizingThem) {
  EXPECT_FLOAT_EQ(
      agree(query(Dir::N, kSt), record(Dir::S, kSt), /*numbered=*/true),
      0.0f); // another quadrant
  EXPECT_FLOAT_EQ(agree(query(Dir::N, kSt), record(Dir::N, kAve), true),
                  0.0f); // another type
  EXPECT_FLOAT_EQ(agree(query(Dir::N, kSt), record(Dir::N, kSt), true), 1.0f);
  EXPECT_FLOAT_EQ(agree(query(Dir::None, kSt), record(Dir::N, kSt), true),
                  0.97f); // saying less is only a penalty
  EXPECT_FLOAT_EQ(agree(query(Dir::N, 0), record(Dir::N, kAve), true), 0.97f);
  EXPECT_FLOAT_EQ(agree(query(Dir::N, kSt), record(Dir::S, kSt), false),
                  0.60f); // an ordinary name keeps the candidate
  EXPECT_TRUE(is_numbered_core("5th"));
  EXPECT_TRUE(is_numbered_core("295"));
  EXPECT_FALSE(is_numbered_core("main"));
  EXPECT_FALSE(is_numbered_core("#i 64"));
}

TEST(QueryScoring, AddressGridStreetsWithTwoDirectionals) {
  auto const w4715s = query(
      Dir::W, 0, Dir::S); // "W 4715 S": the street at 4715 South, west side
  EXPECT_FLOAT_EQ(agree(w4715s, record(Dir::None, 0, Dir::S)),
                  0.92f); // record "4715 South" says less
  EXPECT_FLOAT_EQ(agree(w4715s, record(Dir::W, 0, Dir::S)), 1.0f);
  EXPECT_FLOAT_EQ(agree(w4715s, record(Dir::E, 0, Dir::S)),
                  0.60f); // the other side of the grid
  EXPECT_FLOAT_EQ(agree(w4715s, record(Dir::None, 0, Dir::W)),
                  0.60f); // "4715 West" is another axis
  EXPECT_FLOAT_EQ(agree(query(Dir::None, 0, Dir::S), record(Dir::W, 0, Dir::S)),
                  0.97f); // query says less
  EXPECT_FLOAT_EQ(agree(query(Dir::S, 0, Dir::W), record(Dir::None, 0, Dir::S)),
                  0.60f); // "S 4715 W": south side of 4715 West
}

} // namespace
} // namespace chiasmos::query
