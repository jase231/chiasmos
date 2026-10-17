#include "output/geojson.hpp"

#include <cmath>

namespace chiasmos::output {
namespace {

using nlohmann::json;

// coordinates are stored as degrees * 1e7
double degrees(std::int32_t e7) { return e7 / 1e7; }
double rounded(double v, double scale) { return std::round(v * scale) / scale; }

json feature(query::Candidate const& c) {
  json props{ { "score", rounded(c.score, 1e3) },
              { "confidence", to_string(c.confidence) },
              { "junction_kind", to_string(c.kind) },
              { "street1", c.street1 },
              { "street2", c.street2 },
              { "osm_nodes", c.osm_nodes } };
  if (c.distance_to_near_m >= 0)
    props["distance_to_near_m"] = rounded(c.distance_to_near_m, 10);
  return { { "type", "Feature" },
           { "geometry",
             { { "type", "Point" },
               { "coordinates", { degrees(c.rep.x), degrees(c.rep.y) } } } },
           { "properties", std::move(props) } };
}

json echo(query::Request const& req) {
  json q = json::object();
  if (!req.street1.empty())
    q["street1"] = req.street1;
  if (!req.street2.empty())
    q["street2"] = req.street2;
  if (!req.q.empty())
    q["q"] = req.q;
  if (req.near)
    q["near"] = { { "lat", degrees(req.near->y) },
                  { "lon", degrees(req.near->x) } };
  if (req.bbox)
    q["bbox"] = { degrees(req.bbox->lo.x), degrees(req.bbox->lo.y),
                  degrees(req.bbox->hi.x), degrees(req.bbox->hi.y) }; // W,S,E,N
  return q;
}

} // namespace

json to_geojson(query::Request const& req, query::Result const& res) {
  json features = json::array();
  for (query::Candidate const& c : res.candidates)
    features.push_back(feature(c));
  return { { "type", "FeatureCollection" },
           { "status", to_string(res.status) },
           { "notes", res.notes },
           { "query", echo(req) },
           { "features", std::move(features) } };
}

} // namespace chiasmos::output
