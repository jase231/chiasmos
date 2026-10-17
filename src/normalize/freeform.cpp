#include <algorithm>
#include <optional>
#include <ranges>

#include "normalize/tables.hpp"

namespace chiasmos::norm {
namespace {

using namespace std::string_view_literals;

// strong seperators, xst = cross-street
constexpr std::array kStrong{ "&"sv, "@"sv, "/"sv, "--"sv, "xst"sv };
constexpr bool is_blank(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '"';
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && is_blank(s.front()))
    s.remove_prefix(1);
  while (!s.empty() && is_blank(s.back()))
    s.remove_suffix(1);
  return s;
}

Tokens lex(std::string_view q) {
  Tokens toks;
  std::string word;
  int depth = 0;
  auto const flush = [&] {
    while (!word.empty() && ".,-"sv.contains(word.back()))
      word.pop_back();
    if (!word.empty() && depth == 0)
      toks.push_back(word);
    word.clear();
  };
  for (std::size_t i = 0; i < q.size(); ++i) {
    char const c = q[i];
    if (is_blank(c))
      flush();
    else if (c == '(')
      flush(), ++depth;
    else if (c == ')')
      flush(), depth = std::max(depth - 1, 0);
    else if (c == '&' || c == '@' || c == '/' ||
             (c == '-' && q.substr(i, 2) == "--")) {
      flush();
      if (depth == 0)
        toks.emplace_back(c == '-' ? "--"sv : q.substr(i, 1));
      while (c == '-' && i + 1 < q.size() && q[i + 1] == '-')
        ++i;
    } else if (c != '-' || !word.empty()) {
      word += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }
  }
  flush();
  return toks;
}

// cuts a trailing ", City ST" and the like
// returns them in locality (unused at the moment)
// spelled-out states and missing comma are not recognized.
std::string_view strip_locality(std::string_view q, std::string& locality) {
  std::size_t cut = q.rfind(',');
  if (cut == std::string_view::npos)
    return q;
  Tokens tail = lex(q.substr(cut + 1));
  if (!tail.empty() && tail.back().size() == 5 && all_digits(tail.back()))
    tail.pop_back();
  if (tail.empty() || !is_state_code(tail.back()))
    return q;
  if (tail.size() == 1 && cut > 0 &&
      q.rfind(',', cut - 1) != std::string_view::npos)
    cut = q.rfind(',', cut - 1); // "..., City, ST"
  locality = trim(q.substr(cut + 1));
  return q.substr(0, cut);
}

bool house_number(std::string const& w) { // "3733", "123a"
  auto const digits = static_cast<std::size_t>(
      std::ranges::find_if_not(w, is_digit) - w.begin());
  return digits >= 1 &&
         (digits == w.size() || (digits + 1 == w.size() && is_alpha(w.back())));
}

std::string join(std::span<std::string const> words) {
  return words | std::views::join_with(' ') | std::ranges::to<std::string>();
}

bool street_like(std::string_view text) {
  Normalized n;
  normalize_street(text, n);
  return n.is_route() || n.comp.type != 0;
}

// consider "A and B", "A at B", "A x B" when kStrongs fail
std::optional<std::size_t> weak_separator(Tokens const& w) {
  if (w.size() < 3 || house_number(w[0]))
    return std::nullopt;
  std::optional<std::size_t> best;
  int best_score = -1;
  for (std::size_t i = 1; i + 1 < w.size(); ++i) {
    if (w[i] != "and" && w[i] != "at" && w[i] != "x")
      continue;
    int const score = street_like(join(std::span(w).first(i))) +
                      street_like(join(std::span(w).subspan(i + 1)));
    if (w[i] == "x" && score < 2)
      continue;
    if (score > best_score)
      best = i, best_score = score;
  }
  return best;
}

} // namespace

ParsedQuery parse_freeform(std::string_view q) {
  ParsedQuery p;
  Tokens toks = lex(strip_locality(q, p.locality));
  if (toks.size() >= 3 && toks[1] == "of" &&
      (toks[0] == "intersection" || toks[0] == "corner" || toks[0] == "cnr"))
    toks.erase(toks.begin(), toks.begin() + 2);

  std::vector<Tokens> sides(1);
  for (std::string& t : toks) {
    if (std::ranges::contains(kStrong, t))
      sides.emplace_back();
    else
      sides.back().push_back(std::move(t));
  }
  bool const strong = sides.size() > 1;
  std::erase_if(sides, [](Tokens const& s) { return s.empty(); });
  if (!strong && sides.size() == 1)
    if (auto const at = weak_separator(sides[0])) {
      Tokens const right(sides[0].begin() + static_cast<std::ptrdiff_t>(*at) +
                             1,
                         sides[0].end());
      sides[0].resize(*at);
      sides.push_back(right);
    }

  if (sides.empty())
    return p;
  p.street1 = join(sides[0]);
  if (sides.size() == 1) {
    p.form = Form::SingleStreet;
  } else {
    p.street2 = join(sides[1]);
    p.form = Form::Intersection;
  }
  return p;
}

} // namespace chiasmos::norm
