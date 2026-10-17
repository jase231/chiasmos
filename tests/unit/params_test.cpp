#include <gtest/gtest.h>

#include "output/params.hpp"

namespace chiasmos::output {
namespace {

TEST(Params, NearAndBbox) {
  auto const near = parse_near("37.5,-77.4");
  ASSERT_TRUE(near);
  EXPECT_EQ(*near, to_coord(-77.4, 37.5));
  EXPECT_FALSE(parse_near("37.5"));
  EXPECT_FALSE(parse_near("37.5,-77.4,1"));
  EXPECT_FALSE(parse_near("91,0")); // latitude out of range
  EXPECT_FALSE(parse_near("a,b"));

  auto const box = parse_bbox("-77.7,37.4,-77.2,37.7");
  ASSERT_TRUE(box);
  EXPECT_TRUE(box->contains(to_coord(-77.4, 37.5)));
  EXPECT_FALSE(box->contains(to_coord(-78, 37.5)));
  EXPECT_FALSE(parse_bbox("-77.2,37.4,-77.7,37.7")); // west > east
}

TEST(Params, QueryStringDecodingAndFields) {
  auto const r =
      request_from_query_string("q=Main+St+%26+1st%20Ave&near=37.5,-77.4&limit="
                                "3&min_score=0.5&bbox=-78,37,-77,38");
  ASSERT_TRUE(r) << r.error().msg;
  EXPECT_EQ(r->q, "Main St & 1st Ave");
  ASSERT_TRUE(r->near);
  ASSERT_TRUE(r->bbox);
  EXPECT_EQ(r->limit, 3);
  EXPECT_FLOAT_EQ(r->min_score, 0.5f);

  auto const s = request_from_query_string("street1=A%20St&street2=B+St");
  ASSERT_TRUE(s);
  EXPECT_EQ(s->street1, "A St");
  EXPECT_EQ(s->street2, "B St");
  EXPECT_TRUE(request_from_query_string(
      "")); // an empty request parses; the engine reports InvalidRequest
}

TEST(Params, RejectsBadInput) {
  EXPECT_FALSE(request_from_query_string("limit=0"));
  EXPECT_FALSE(request_from_query_string("limit=abc"));
  EXPECT_FALSE(request_from_query_string("near=1"));
  EXPECT_FALSE(request_from_query_string("nope=1"));
  auto const malformed_escape = request_from_query_string("q=100%");
  ASSERT_TRUE(malformed_escape);
  EXPECT_EQ(malformed_escape->q, "100%");
}

} // namespace
} // namespace chiasmos::output
