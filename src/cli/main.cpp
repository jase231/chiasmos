#include <CLI/CLI.hpp>

#include "cli/commands.hpp"
#include "index/format.hpp"
#include "normalize/normalizer.hpp"

int main(int argc, char** argv) {
  using namespace chiasmos;
  CLI::App app{ "chiasmos: fast OpenStreetMap intersection geocoder" };
  app.set_version_flag("--version",
                       std::format("chiasmos 0.1.0 (format {}, normalizer {})",
                                   index::kFormatVersion,
                                   norm::kNormalizerVersion));
  app.require_subcommand(1, 1);

  cli::BuildOptions build;
  auto* b =
      app.add_subcommand("build", "Build an index from an OSM PBF extract");
  b->add_option("-i,--input", build.input, "OSM .osm.pbf file")
      ->required()
      ->check(CLI::ExistingFile);
  b->add_option("-o,--output", build.output, "index file to write (.cix)")
      ->required();
  b->add_option("--region", build.region,
                "region label stored in the index, e.g. us/virginia");
  b->add_flag("--include-paths", build.include_paths,
              "also keep named footways, cycleways and paths");
  b->callback([&] { std::exit(cli::run_build(build)); });

  cli::QueryOptions query;
  auto* q =
      app.add_subcommand("query", "Geocode one intersection and print GeoJSON");
  q->add_option("-i,--index", query.index, "index file")
      ->required()
      ->check(CLI::ExistingFile);
  q->add_option("--q", query.q, "free-form text: \"Main St & 1st Ave\"");
  q->add_option("--street1", query.street1, "first street");
  q->add_option("--street2", query.street2, "second street");
  q->add_option("--near", query.near, "prefer results near LAT,LON");
  q->add_option("--bbox", query.bbox,
                "only results inside WEST,SOUTH,EAST,NORTH");
  q->add_option("--limit", query.limit, "maximum number of results")
      ->check(CLI::Range(1, 50));
  q->add_option("--min-score", query.min_score,
                "drop results scoring below this")
      ->check(CLI::Range(0.0f, 1.0f));
  q->add_flag("--pretty", query.pretty, "indent the JSON");
  q->callback([&] { std::exit(cli::run_query(query)); });

  cli::BatchOptions batch;
  auto* t = app.add_subcommand(
      "batch", "Geocode a CSV file: the input columns plus chiasmos_* columns");
  t->add_option("-i,--index", batch.index, "index file")
      ->required()
      ->check(CLI::ExistingFile);
  t->add_option("--in", batch.in, "input CSV")
      ->required()
      ->check(CLI::ExistingFile);
  t->add_option("--out", batch.out, "output CSV")->required();
  t->add_option("--q-col", batch.q_col,
                "name of the column holding the location text")
      ->required();
  t->add_option("--near", batch.near,
                "prefer results near LAT,LON (every row)");
  t->add_option("--bbox", batch.bbox,
                "only results inside WEST,SOUTH,EAST,NORTH (every row)");
  t->add_option(
       "--limit", batch.limit,
       "results per row (the first is the answer, the rest are alternatives)")
      ->check(CLI::Range(1, 50));
  t->callback([&] { std::exit(cli::run_batch(batch)); });

  cli::ServeOptions serve;
  auto* s =
      app.add_subcommand("serve", "Serve GET /v1/intersections over HTTP");
  s->add_option("-i,--index", serve.index, "index file")
      ->required()
      ->check(CLI::ExistingFile);
  s->add_option("--listen", serve.listen, "HOST:PORT")->capture_default_str();
  s->callback([&] { std::exit(cli::run_serve(serve)); });

  cli::InspectOptions inspect;
  auto* n = app.add_subcommand("inspect", "Print what an index contains");
  n->add_option("-i,--index", inspect.index, "index file")
      ->required()
      ->check(CLI::ExistingFile);
  n->add_flag("--verify", inspect.verify,
              "check every cross-reference (reads the whole file)");
  n->callback([&] { std::exit(cli::run_inspect(inspect)); });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
