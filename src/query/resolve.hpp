#pragma once

#include <array>

#include "normalize/normalizer.hpp"
#include "query/engine.hpp"

namespace chiasmos::query {

[[nodiscard]] Result resolve(index::Index const& ix, Request const& req,
                             std::array<norm::Normalized, 2> const& sides);

}
