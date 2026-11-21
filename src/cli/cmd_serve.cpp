#include <print>

#include "cli/commands.hpp"
#include "query/engine.hpp"

#ifdef CHIASMOS_HAS_SERVER
#include "server/server.hpp"
#endif

namespace chiasmos::cli {

int run_serve(ServeOptions const& o) {
#ifdef CHIASMOS_HAS_SERVER
  auto const ix = index::Index::open(o.index);
  if (!ix)
    return std::println(stderr, "{}", ix.error().msg), 1;
  std::println("serving {} on http://{}:{}/v1/intersections", ix->region(),
               o.server.host, o.server.port);

  auto const done = server::serve(query::Engine(*ix), o.server);
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
