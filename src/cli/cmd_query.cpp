#include <print>

#include "cli/commands.hpp"
#include "output/geojson.hpp"
#include "output/params.hpp"
#include "query/engine.hpp"

namespace chiasmos::cli {

int run_query(QueryOptions const& o) {
  query::Request req;
  req.street1 = o.street1, req.street2 = o.street2, req.q = o.q,
  req.limit = o.limit, req.min_score = o.min_score;

  if (!o.near.empty() && !(req.near = output::parse_near(o.near)))
    return std::println(stderr, "--near must be LAT,LON"), 2;
  if (!o.bbox.empty() && !(req.bbox = output::parse_bbox(o.bbox)))
    return std::println(stderr, "--bbox must be WEST,SOUTH,EAST,NORTH"), 2;
  if (req.q.empty() && req.street1.empty() && req.street2.empty())
    return std::println(stderr, "give --q, or --street1 and --street2"), 2;

  auto const ix = index::Index::open(o.index);
  if (!ix)
    return std::println(stderr, "{}", ix.error().msg), 1;

  auto const res = query::Engine(*ix).run(req);
  std::println("{}", output::to_geojson(req, res).dump(o.pretty ? 2 : -1));
  return res.status == Status::Ok || res.status == Status::Ambiguous ? 0 : 3;
}

} // namespace chiasmos::cli
