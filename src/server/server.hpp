#pragma once

#include <cstdint>
#include <string>

#include "core/error.hpp"
#include "query/engine.hpp"

namespace chiasmos::server {

[[nodiscard]] Result<void> serve(query::Engine const& engine,
                                 std::string const& host, std::uint16_t port);

} // namespace chiasmos::server
