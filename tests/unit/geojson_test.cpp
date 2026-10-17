#include <gtest/gtest.h>

#include "output/geojson.hpp"

namespace chiasmos::output {
namespace {

using nlohmann::json;

query::Result result(Status status,
                     std::vector<query::Candidate> candidates = {},
                     std::vector<std::string> notes = {}) {
  query::Result r;
  r.status = status, r.candidates = std::move(candidates),
  r.notes = std::move(notes);
  return r;
}

query::Candidate candidate() {
  query::Candidate c;
  c.rep = { -775181654, 373000001 }; // -77.5181654, 37.3000001
  c.score = 0.123456f;
  c.confidence = query::Confidence::Medium;
  c.kind = JunctionKind::Grouped;
  c.street1 = "Glenside Drive";
  c.street2 = "West Broad Street";
  c.osm_nodes = { 11, 22 };
  return c;
}

TEST(Geojson, StructureOfAnAnswer) {
  query::Request req;
  req.street1 = "Glenside Dr", req.street2 = "W Broad St";
  json const j =
      to_geojson(req, result(Status::Ok, { candidate() }, { "a note" }));

  EXPECT_EQ(j["type"], "FeatureCollection");
  EXPECT_EQ(j["status"], "ok");
  EXPECT_EQ(j["notes"], json::array({ "a note" }));
  ASSERT_EQ(j["features"].size(), 1u);
  json const& f = j["features"][0];
  EXPECT_EQ(f["type"], "Feature");
  EXPECT_EQ(f["geometry"]["type"], "Point");
  EXPECT_EQ(f["properties"]["score"], 0.123);
  EXPECT_EQ(f["properties"]["confidence"], "medium");
  EXPECT_EQ(f["properties"]["junction_kind"], "grouped");
  EXPECT_EQ(f["properties"]["street1"], "Glenside Drive");
  EXPECT_EQ(f["properties"]["street2"], "West Broad Street");
  EXPECT_EQ(f["properties"]["osm_nodes"], json::array({ 11, 22 }));
  EXPECT_FALSE(f["properties"].contains(
      "distance_to_near_m")); // only when the request had a near point
}

TEST(Geojson, CoordinatesPrintWithoutBinaryNoise) {
  json const j = to_geojson({}, result(Status::Ok, { candidate() }));
  json const& xy = j["features"][0]["geometry"]["coordinates"];
  ASSERT_EQ(xy.size(), 2u);
  EXPECT_EQ(xy[0].get<double>(),
            -77.5181654); // exactly the decimal, not -77.51816540000001
  EXPECT_EQ(xy[1].get<double>(), 37.3000001);
  std::string const text = j.dump();
  EXPECT_NE(text.find("[-77.5181654,37.3000001]"), std::string::npos) << text;
}

TEST(Geojson, WholeDegreesAndNegativeFractionsRoundTrip) {
  query::Candidate c = candidate();
  c.rep = { -50, 10000000 }; // -0.000005, 1.0
  json const j = to_geojson({}, result(Status::Ok, { c }));
  EXPECT_EQ(j["features"][0]["geometry"]["coordinates"][0].get<double>(),
            -0.000005);
  EXPECT_EQ(j["features"][0]["geometry"]["coordinates"][1].get<double>(), 1.0);
}

TEST(Geojson, NearDistanceAndQueryEcho) {
  query::Candidate c = candidate();
  c.distance_to_near_m = 1234.5678f;
  query::Request req;
  req.q = "Glenside Dr & W Broad St";
  req.near = Coord{ -775181654, 373000001 };
  req.bbox = Bbox{ { -780000000, 370000000 }, { -770000000, 380000000 } };
  json const j = to_geojson(req, result(Status::Ambiguous, { c }));
  EXPECT_EQ(j["status"], "ambiguous");
  EXPECT_EQ(j["features"][0]["properties"]["distance_to_near_m"], 1234.6);

  json const& q = j["query"];
  EXPECT_EQ(q["q"], "Glenside Dr & W Broad St");
  EXPECT_FALSE(q.contains("street1")); // only what was set
  EXPECT_FALSE(q.contains("street2"));
  EXPECT_EQ(q["near"], (json{ { "lat", 37.3000001 }, { "lon", -77.5181654 } }));
  EXPECT_EQ(q["bbox"], json::array({ -78.0, 37.0, -77.0,
                                     38.0 })); // west, south, east, north
}

TEST(Geojson, NoCandidatesStillCarriesStatusAndNotes) {
  query::Request req;
  req.street1 = "Foo", req.street2 = "Bar";
  json const j = to_geojson(req, result(Status::NoStreetMatch, {},
                                        { "street 1 not found: \"foo\"" }));
  EXPECT_EQ(j["status"], "no_street_match");
  EXPECT_TRUE(j["features"].is_array());
  EXPECT_TRUE(j["features"].empty());
  EXPECT_EQ(j["notes"].size(), 1u);
  EXPECT_EQ(j["query"]["street1"], "Foo");
  EXPECT_FALSE(j["query"].contains("near"));
  EXPECT_FALSE(j["query"].contains("bbox"));
  EXPECT_TRUE(to_geojson({}, result(Status::Ok))["notes"]
                  .is_array()); // always present, possibly empty
}

} // namespace
} // namespace chiasmos::output
