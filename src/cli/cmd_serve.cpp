#include <charconv>
#include <print>

#include "cli/commands.hpp"
#include "query/engine.hpp"

#ifdef CHIASMOS_HAS_SERVER
#include "server/server.hpp"
#endif

namespace chiasmos::cli {

int run_serve(ServeOptions const& o) {
#ifdef CHIASMOS_HAS_SERVER
  auto const colon = o.listen.rfind(':');
  unsigned port = 0;
  if (colon == std::string::npos ||
      std::from_chars(o.listen.data() + colon + 1,
                      o.listen.data() + o.listen.size(), port)
              .ec != std::errc{} ||
      port > 65535)
    return std::println(stderr, "--listen must be HOST:PORT"), 2;

  auto const ix = index::Index::open(o.index);
  if (!ix)
    return std::println(stderr, "{}", ix.error().msg), 1;
  std::println("serving {} on http://{}/v1/intersections", ix->region(),
               o.listen);

  auto const done = server::serve(query::Engine(*ix), o.listen.substr(0, colon),
                                  static_cast<std::uint16_t>(port));
  if (!done)
    return std::println(stderr, "serve: {}", done.error().msg), 1;
  return 0;
#else
  (void)o;
  return std::println(stderr,
                      "this build has no HTTP server (CHIASMOS_SERVER=OFF)"),
         1;
#endif
}

} // namespace chiasmos::cli
