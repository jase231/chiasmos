#include <algorithm>
#include <format>

#include "normalize/tables.hpp"

namespace chiasmos::norm {

// [DIR] PREFIX N [MOD...] [DIR]
bool parse_route(Tokens const& t, Normalized& out) {
  std::size_t i = 0;
  Dir dir = Dir::None;
  if (t.size() >= 3 && dir_of(t[0]) != Dir::None)
    dir = dir_of(t[i++]);
  if (t.size() - i < 2 || !is_route_number(t[i + 1]))
    return false;

  Word const* word = find_word(t[i]);
  auto const route = std::ranges::find(kRouteWords, t[i], &RouteWord::word);
  // a state code is a state route
  RouteNet net = RouteNet::State;
  if (word == nullptr || !word->state) {
    if (route == kRouteWords.end())
      return false;
    net = route->net;
  }
  std::string_view const prefix = net == RouteNet::Hwy     ? "hwy"
                                  : net == RouteNet::State ? "sr"
                                                           : route->word;

  std::uint8_t mods = 0;
  std::size_t j = i + 2;
  for (; j < t.size(); ++j) {
    if (Word const* m = find_word(t[j]); m != nullptr && m->mod != 0)
      mods |= m->mod;
    // the USPS form of "express"
    else if (t[j] == kExprWord)
      mods |= EXPR;
    // accepted and dropped
    else if (t[j] != "toll" && t[j] != "scenic")
      break;
  }
  if (j < t.size() && dir == Dir::None && dir_of(t[j]) != Dir::None)
    dir = dir_of(t[j++]);
  if (j != t.size())
    return false;

  out.core = std::format("#{} {}", prefix, t[i + 1]);
  out.full = out.core;
  append_mods(out.full, mods);
  out.comp = { .suf_dir = dir, .post_mod = mods, .route = net };
  return true;
}

} // namespace chiasmos::norm
