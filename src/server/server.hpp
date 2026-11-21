#pragma once

#include "core/error.hpp"
#include "query/engine.hpp"
#include "server/options.hpp"

namespace chiasmos::server {

[[nodiscard]] Result<void> serve(query::Engine const& engine,
                                 Options const& opts);

} // namespace chiasmos::server
