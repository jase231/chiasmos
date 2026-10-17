#pragma once

#include <optional>
#include <string_view>

#include "core/error.hpp"
#include "query/engine.hpp"

namespace chiasmos::output {

// "lat,lon"
[[nodiscard]] std::optional<Coord> parse_near(std::string_view text);
// "west,south,east,north"
[[nodiscard]] std::optional<Bbox> parse_bbox(std::string_view text);

// unknown keys error
[[nodiscard]] Result<query::Request>
request_from_query_string(std::string_view query_string);

} // namespace chiasmos::output
