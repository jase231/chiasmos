#include <print>

#include "build/build_index.hpp"
#include "cli/commands.hpp"

namespace chiasmos::cli {

int run_build(BuildOptions const& o) {
  auto const stats = build::build_index(
      o.input, o.output,
      { .region = o.region, .include_paths = o.include_paths });
  if (!stats) {
    std::println(stderr, "build failed: {}", stats.error().msg);
    return 1;
  }
  std::println("{}: {} road ways, {} vertices, {} names, {} junctions ({} "
               "roundabouts) in {:.1f} s",
               o.output.string(), stats->road_ways, stats->vertices,
               stats->full_names, stats->junctions, stats->roundabouts,
               stats->seconds);
  return 0;
}

} // namespace chiasmos::cli
