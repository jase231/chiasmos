#include "output/params.hpp"

#include <charconv>
#include <ranges>
#include <string>

namespace chiasmos::output {

namespace {

std::optional<double> number(std::string_view s) {
  double v = 0;
  auto const [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc{} || end != s.data() + s.size())
    return std::nullopt;
  return v;
}

// parses text as N comma-sep numbers
// nullopt if params are > or < N.
template <std::size_t N>
std::optional<std::array<double, N>> numbers(std::string_view text) {
  std::array<double, N> out{};
  std::size_t n = 0;
  for (auto const part : std::views::split(text, ',')) {
    auto const v = number(std::string_view(part.begin(), part.end()));
    if (!v || n == N)
      return std::nullopt;
    out[n++] = *v;
  }
  return n == N ? std::optional(out) : std::nullopt;
}

bool valid(double lon, double lat) {
  return lon >= -180 && lon <= 180 && lat >= -90 && lat <= 90;
}

// application/x-www-form-urlencoded: '+' is a space, %XX is a byte
std::string decode(std::string_view s) {
  std::string out;
  for (std::size_t i = 0; i < s.size(); ++i) {
    unsigned byte = 0;
    if (s[i] == '+')
      out += ' ';
    else if (s[i] == '%' && i + 2 < s.size() &&
             std::from_chars(s.data() + i + 1, s.data() + i + 3, byte, 16).ec ==
                 std::errc{}) {
      out += static_cast<char>(byte);
      i += 2;
    } else
      out += s[i];
  }
  return out;
}

} // namespace

std::optional<Coord> parse_near(std::string_view text) {
  auto const v = numbers<2>(text);
  if (!v || !valid((*v)[1], (*v)[0]))
    return std::nullopt;
  return to_coord((*v)[1], (*v)[0]);
}

std::optional<Bbox> parse_bbox(std::string_view text) {
  auto const v = numbers<4>(text);
  if (!v || !valid((*v)[0], (*v)[1]) || !valid((*v)[2], (*v)[3]) ||
      (*v)[0] > (*v)[2] || (*v)[1] > (*v)[3])
    return std::nullopt;
  return Bbox{ to_coord((*v)[0], (*v)[1]), to_coord((*v)[2], (*v)[3]) };
}

Result<query::Request>
request_from_query_string(std::string_view query_string) {
  query::Request req;
  for (auto const pair : std::views::split(query_string, '&')) {
    std::string_view const kv(pair.begin(), pair.end());
    if (kv.empty())
      continue;
    auto const eq = kv.find('=');
    std::string const key = decode(kv.substr(0, eq)),
                      value = eq == std::string_view::npos
                                  ? ""
                                  : decode(kv.substr(eq + 1));
    auto const bad = [&](std::string_view what) {
      return fail(Errc::InvalidArgument,
                  std::string(what) + ": '" + value + "'");
    };
    if (key == "street1")
      req.street1 = value;
    else if (key == "street2")
      req.street2 = value;
    else if (key == "q")
      req.q = value;
    else if (key == "near") {
      if (!(req.near = parse_near(value)))
        return bad("near must be lat,lon");
    } else if (key == "bbox") {
      if (!(req.bbox = parse_bbox(value)))
        return bad("bbox must be west,south,east,north");
    } else if (key == "limit") {
      auto const n = number(value);
      if (!n || *n < 1 || *n > 50)
        return bad("limit must be 1..50");
      req.limit = static_cast<int>(*n);
    } else if (key == "min_score") {
      auto const n = number(value);
      if (!n || *n < 0 || *n > 1)
        return bad("min_score must be 0..1");
      req.min_score = static_cast<float>(*n);
    } else
      return fail(Errc::InvalidArgument, "unknown parameter '" + key + "'");
  }
  return req;
}

} // namespace chiasmos::output
