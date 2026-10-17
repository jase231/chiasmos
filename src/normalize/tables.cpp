#include <algorithm>
#include <bit>
#include <ranges>
#include <unordered_map>

#include "normalize/tables.hpp"

namespace chiasmos::norm {
namespace {

auto words_of(std::string_view row) {
  return row | std::views::split(' ') |
         std::views::transform([](auto w) { return std::string_view(w); });
}

std::unordered_map<std::string_view, Word> const& word_map() {
  static auto const map = [] {
    std::unordered_map<std::string_view, Word> m;
    for (auto const [i, row] : std::views::enumerate(kTypes))
      for (auto const w : words_of(row))
        m[w].type = static_cast<std::uint16_t>(i + 1);
    for (auto const [i, row] : std::views::enumerate(kDirs))
      for (auto const w : words_of(row))
        m[w].dir = static_cast<Dir>(i + 1);
    for (auto const [i, row] : std::views::enumerate(kMods))
      for (auto const w : words_of(row))
        m[w].mod = static_cast<std::uint8_t>(1u << i);
    for (auto const w : kPrefixTypes)
      m[w].prefix_type = true;
    for (auto const w : words_of(kStates))
      if (w != "ne" && w != "co")
        m[w].state = true;
    return m;
  }();
  return map;
}

std::vector<Synonym> make_synonyms() {
  std::vector<Synonym> v;
  auto const add = [&](std::string_view phrase, std::string_view to,
                       Need need) {
    v.push_back({ words_of(phrase) | std::views::transform([](auto w) {
                    return std::string(w);
                  }) | std::ranges::to<Tokens>(),
                  to, need });
  };
  // cross product
  auto const cross = [&](std::initializer_list<std::string_view> heads,
                         std::initializer_list<std::string_view> tails,
                         std::string_view to) {
    for (auto const h : heads)
      for (auto const t : tails)
        add(std::string(h) + ' ' + std::string(t), to, Need::Number);
  };
  for (auto const pre : { "", "dr ", "doctor " })
    for (auto const name :
         { "martin luther king", "martin l king", "m l king" })
      for (auto const suf : { "", " jr", " junior" })
        add(std::string(pre) + name + suf, "mlk", Need::Nothing);
  add("mlk jr", "mlk", Need::Nothing), add("mlk junior", "mlk", Need::Nothing);
  add("saint", "st", Need::Nothing), add("sainte", "ste", Need::Nothing);
  cross({ "county", "co", "cnty", "cty" },
        { "road", "rd", "route", "rte", "highway", "hwy" }, "cr");
  cross({ "state", "st" },
        { "road", "rd", "route", "rte", "rt", "highway", "hwy" }, "sr");
  cross({ "us", "u s", "united states" },
        { "highway", "hwy", "route", "rte", "rt", "road" }, "us");
  cross({ "interstate" }, { "highway", "hwy" }, "i");
  add("c r", "cr", Need::Number), add("s r", "sr", Need::Number),
      add("u s", "us", Need::Number);
  add("interstate", "i", Need::Number), add("ih", "i", Need::Number),
      add("i h", "i", Need::Number);
  add("rd", "cr", Need::Whole),
      add("road", "cr",
          Need::Whole); // TIGER's "Rd 7125" is OSM's "County Road 7125"
  std::ranges::stable_sort(v, [](Synonym const& a, Synonym const& b) {
    return a.from[0] != b.from[0] ? a.from[0] < b.from[0]
                                  : a.from.size() > b.from.size();
  });
  return v;
}

} // namespace

bool all_digits(std::string_view s) {
  return !s.empty() && std::ranges::all_of(s, is_digit);
}

bool is_route_number(std::string_view t) {
  if (!t.empty() && is_alpha(t.front()))
    t.remove_prefix(1);
  if (!t.empty() && is_alpha(t.back()))
    t.remove_suffix(1);
  return t.size() >= 1 && t.size() <= 4 && all_digits(t);
}

bool is_state_code(std::string_view w) {
  return std::ranges::contains(words_of(kStates), w);
}

Word const* find_word(std::string_view w) {
  auto const& m = word_map();
  auto const it = m.find(w);
  return it == m.end() ? nullptr : &it->second;
}

std::string_view canonical(Word const& w) {
  if (w.mod != 0)
    return mod_name(static_cast<unsigned>(std::countr_zero(w.mod)));
  if (w.dir != Dir::None)
    return to_string(w.dir);
  return type_name(w.type);
}

std::span<Synonym const> synonyms_starting_with(std::string_view first) {
  static std::vector<Synonym> const all = make_synonyms();
  auto const r = std::ranges::equal_range(all, first, {}, [](Synonym const& s) {
    return std::string_view(s.from[0]);
  });
  return { r.begin(), r.end() };
}

} // namespace chiasmos::norm
