#pragma once

#include <nlohmann/json.hpp>

#include "query/engine.hpp"

namespace chiasmos::output {

// { "status", "notes", "query": {...}, "type": "FeatureCollection", "features":
// [ Point + properties ] }. coordinates are written with 7 decimals
[[nodiscard]] nlohmann::json to_geojson(query::Request const& req,
                                        query::Result const& res);

} // namespace chiasmos::output
