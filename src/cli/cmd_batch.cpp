#include <algorithm>
#include <format>
#include <fstream>
#include <print>
#include <ranges>

#include "cli/commands.hpp"
#include "output/csv.hpp"
#include "output/params.hpp"
#include "query/engine.hpp"

namespace chiasmos::cli {
namespace {

constexpr std::array<std::string_view, 10> kColumns{
  "chiasmos_status",      "chiasmos_lat",        "chiasmos_lon",
  "chiasmos_score",       "chiasmos_confidence", "chiasmos_kind",
  "chiasmos_street1",     "chiasmos_street2",    "chiasmos_candidates",
  "chiasmos_alternatives"
};

std::string coordinate(double v) { return std::format("{:.7f}", v); }

std::vector<std::string> answer_columns(query::Result const& r) {
  std::vector<std::string> c(kColumns.size());
  c[0] = to_string(r.status);
  c[8] = std::to_string(r.candidates.size());
  if (r.candidates.empty())
    return c;
  query::Candidate const& best = r.candidates.front();
  c[1] = coordinate(lat_deg(best.rep)), c[2] = coordinate(lon_deg(best.rep));
  c[3] = std::format("{:.3f}", best.score), c[4] = to_string(best.confidence),
  c[5] = to_string(best.kind);
  c[6] = best.street1, c[7] = best.street2;
  for (auto const& alt : r.candidates | std::views::drop(1))
    c[9] += std::format("{}{} {} {:.3f}", c[9].empty() ? "" : ";",
                        coordinate(lat_deg(alt.rep)),
                        coordinate(lon_deg(alt.rep)), alt.score);
  return c;
}

} // namespace

int run_batch(BatchOptions const& o) {
  query::Request base;
  base.limit = o.limit;
  if (!o.near.empty() && !(base.near = output::parse_near(o.near)))
    return std::println(stderr, "--near must be LAT,LON"), 2;
  if (!o.bbox.empty() && !(base.bbox = output::parse_bbox(o.bbox)))
    return std::println(stderr, "--bbox must be WEST,SOUTH,EAST,NORTH"), 2;
  auto const ix = index::Index::open(o.index);
  if (!ix)
    return std::println(stderr, "{}", ix.error().msg), 1;
  query::Engine const engine(*ix);

  std::ifstream in(o.in, std::ios::binary);
  std::ofstream out(o.out, std::ios::binary | std::ios::trunc);
  if (!in || !out)
    return std::println(stderr, "cannot open {}",
                        in ? o.out.string() : o.in.string()),
           1;
  auto header = output::read_csv_record(in);
  if (!header)
    return std::println(stderr, "{}: empty file", o.in.string()), 1;
  auto const col = std::ranges::find(*header, o.q_col);
  if (col == header->end())
    return std::println(stderr, "{}: no column named '{}'", o.in.string(),
                        o.q_col),
           1;
  auto const q_index = static_cast<std::size_t>(col - header->begin());

  header->insert(header->end(), kColumns.begin(), kColumns.end());
  output::write_csv_record(out, *header);

  std::size_t rows = 0, answered = 0;
  while (auto row = output::read_csv_record(in)) {
    if (row->size() == 1 && row->front().empty())
      continue; // blank line
    query::Request req = base;
    if (q_index < row->size())
      req.q = (*row)[q_index];
    query::Result const res = engine.run(req);
    row->resize(header->size() - kColumns.size());
    for (auto& field : answer_columns(res))
      row->push_back(std::move(field));
    output::write_csv_record(out, *row);
    ++rows,
        answered += res.status == Status::Ok || res.status == Status::Ambiguous;
  }
  std::println("{} rows, {} answered ({:.1f}%) -> {}", rows, answered,
               rows == 0 ? 0.0
                         : 100.0 * static_cast<double>(answered) /
                               static_cast<double>(rows),
               o.out.string());
  return 0;
}

} // namespace chiasmos::cli
