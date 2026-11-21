#pragma once

#include <cstdint>
#include <string>

namespace chiasmos::server {

struct Options {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8080;
  std::uint16_t threads = 1;
};

} // namespace chiasmos::server
