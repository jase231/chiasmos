#include "normalize/normalizer.hpp"

#include <algorithm>
#include <charconv>
#include <optional>
#include <ranges>

#include "normalize/tables.hpp"

namespace chiasmos::norm {

std::string_view to_string(Dir d) {
  return d == Dir::None ? std::string_view{}
                        : first_word(kDirs[static_cast<std::size_t>(d) - 1]);
}
std::string_view type_name(std::uint16_t type) {
  return type == 0 || type > kTypes.size() ? std::string_view{}
                                           : first_word(kTypes[type - 1u]);
}

namespace {

// "i64" -> "i" "64", "va33" -> "va" "33"
Tokens split_glued(Tokens const& in) {
  Tokens out;
  for (std::string const& tok : in) {
    auto const n = static_cast<std::size_t>(
        std::ranges::find_if_not(tok, is_alpha) - tok.begin());
    std::string_view const prefix = std::string_view(tok).substr(0, n),
                           rest = std::string_view(tok).substr(n);
    bool const shaped = n >= 1 && n <= 3 && !rest.empty() &&
                        is_digit(rest[0]) && is_route_number(rest);
    Word const* w = shaped ? find_word(prefix) : nullptr;
    bool const glued =
        shaped &&
        ((w != nullptr && w->state) || prefix == "ih" || prefix == "rt" ||
         std::ranges::contains(kRouteWords, prefix, &RouteWord::word));
    if (glued)
      out.emplace_back(prefix), out.emplace_back(rest);
    else
      out.push_back(tok);
  }
  return out;
}

struct NumberWord {
  unsigned long value;
  bool ordinal;
};
std::optional<NumberWord> number_word(std::string_view w) {
  auto const it = std::ranges::find_if(
      kNumbers, [&](auto const& p) { return p.first == w || p.second == w; });
  if (it == kNumbers.end())
    return std::nullopt;
  auto const i = static_cast<unsigned long>(it - kNumbers.begin());
  return NumberWord{ i < 19 ? i + 1 : (i - 17) * 10, it->second == w };
}

// standardizes ordinals
Tokens convert_numbers(Tokens const& in) {
  Tokens out;
  for (std::size_t i = 0; i < in.size(); ++i) {
    std::string_view const tok = in[i];
    auto const digits = static_cast<std::size_t>(
        std::ranges::find_if_not(tok, is_digit) - tok.begin());
    std::string_view const suffix = tok.substr(digits);
    if (digits >= 1 && tok.size() >= 2 && digits <= 9 &&
        (suffix == "st" || suffix == "nd" || suffix == "rd" || suffix == "th" ||
         suffix == "d")) {
      unsigned long n = 0;
      std::from_chars(tok.data(), tok.data() + digits, n);
      bool const right = suffix == ordinal_suffix(n) ||
                         (suffix == "d" && n % 10 != 2 &&
                          n % 10 != 3); // "1d" is not an ordinal: leave it
      out.push_back(right ? std::string(tok)
                          : std::to_string(n) + ordinal_suffix(n));
    } else if (auto nw = number_word(tok)) {
      if (!nw->ordinal && nw->value >= 20 && nw->value % 10 == 0 &&
          i + 1 < in.size()) { // "twenty first", "twenty one"
        if (auto const unit = number_word(in[i + 1]);
            unit && unit->value >= 1 && unit->value <= 9) {
          nw = NumberWord{ nw->value + unit->value, unit->ordinal };
          ++i;
        }
      }
      out.push_back(std::to_string(nw->value) +
                    (nw->ordinal ? ordinal_suffix(nw->value) : ""));
    } else {
      out.emplace_back(tok);
    }
  }
  return out;
}

bool applies(Synonym const& s, std::span<std::string const> t, std::size_t i) {
  std::size_t const n = s.from.size();
  if (i + n > t.size() || !std::ranges::equal(t.subspan(i, n), s.from))
    return false;
  if (s.need == Need::Nothing)
    return true;
  // "state road 7" -> "sr 7", but bare "state road" remains
  if (i + n >= t.size() || !is_route_number(t[i + n]))
    return false;
  return s.need == Need::Number || (i == 0 && t.size() == n + 1);
}

// longest synonym first at each position
// eg. if options are:
// martin luther king jr -> mlk
// martin luther king -> mlk
// choose the longer one so theres no stray jr
Tokens apply_synonyms(Tokens const& in) {
  Tokens out;
  for (std::size_t i = 0; i < in.size();) {
    auto const cands = synonyms_starting_with(in[i]);
    auto const hit = std::ranges::find_if(
        cands, [&](Synonym const& s) { return applies(s, in, i); });
    if (hit == cands.end())
      out.push_back(in[i++]);
    else
      out.emplace_back(hit->to), i += hit->from.size();
  }
  return out;
}

void canonicalize(Tokens& t) {
  for (std::size_t i = 0; i < t.size(); ++i) {
    Word const* w = find_word(t[i]);
    if (w == nullptr || canonical(*w).empty() ||
        (w->state && i + 1 < t.size() && is_route_number(t[i + 1])))
      continue;
    t[i] = canonical(*w);
  }
}

// "n e" -> "ne"
// but only if other parts remain
void merge_dir_pairs(Tokens& t) {
  if (t.size() < 3)
    return;
  Tokens out;
  for (std::size_t i = 0; i < t.size(); ++i) {
    Dir const a = dir_of(t[i]),
              b = i + 1 < t.size() ? dir_of(t[i + 1]) : Dir::None;
    if ((a == Dir::N || a == Dir::S) && (b == Dir::E || b == Dir::W)) {
      out.emplace_back(
          to_string(static_cast<Dir>(5 + 2 * (a == Dir::S) + (b == Dir::W))));
      ++i;
    } else {
      out.push_back(std::move(t[i]));
    }
  }
  t = std::move(out);
}

// split into [pre dir] [prefix type] core [suffix type] [modifiers] [suffix
// dir]
void split_components(Tokens const& t, Normalized& out) {
  Components& c = out.comp;
  auto const word = [&](std::size_t i) { return find_word(t[i]); };
  std::size_t b = 0, e = t.size();
  bool suffix_type = false;

  if (e == 2 && word(0) != nullptr && word(0)->prefix_type &&
      (word(1) == nullptr || word(1)->type == 0)) {
    c.type = word(0)->type, c.type_pos = TypePos::Prefix,
    b = 1; // "Avenue A", "Calle Ocho": the second word is the name
  } else {
    if (Word const* w = word(e - 1);
        e >= 2 && w != nullptr && w->dir != Dir::None)
      c.suf_dir = w->dir, --e;
    while (e - b >= 2 && word(e - 1) != nullptr && word(e - 1)->mod != 0)
      c.post_mod |= word(e - 1)->mod, --e;
    if (Word const* w = word(e - 1);
        e - b >= 2 && w != nullptr && w->type != 0 &&
        !(e - b == 2 && std::ranges::contains(kCoreStopwords, t[b]))) {
      c.type = w->type, c.type_pos = TypePos::Suffix, --e;
      suffix_type = true;
    }
    if (Word const* w = word(b);
        e - b >= 2 && w != nullptr && w->dir != Dir::None &&
        std::ranges::any_of(std::views::iota(b + 1, e), [&](std::size_t i) {
          return dir_of(t[i]) == Dir::None;
        }))
      c.pre_dir = w->dir, ++b;
    if (Word const* w = word(b);
        !suffix_type && e - b >= 2 && w != nullptr && w->prefix_type)
      c.type = w->type, c.type_pos = TypePos::Prefix, ++b;
  }

  out.core = t | std::views::drop(b) | std::views::take(e - b) |
             std::views::join_with(' ') | std::ranges::to<std::string>();
  if (e - b == 1 && all_digits(out.core) && out.core.size() <= 9 &&
      (c.type != 0 || c.pre_dir != Dir::None)) {
    unsigned long n = 0;
    std::from_chars(out.core.data(), out.core.data() + out.core.size(), n);
    out.core += ordinal_suffix(n);
  }

  auto const add = [&](std::string_view s) {
    if (s.empty())
      return;
    if (!out.full.empty())
      out.full += ' ';
    out.full += s;
  };
  add(to_string(c.pre_dir));
  if (c.type_pos == TypePos::Prefix)
    add(type_name(c.type));
  add(out.core);
  if (c.type_pos == TypePos::Suffix)
    add(type_name(c.type));
  append_mods(out.full, c.post_mod);
  add(to_string(c.suf_dir));
}

} // namespace

void normalize_street(std::string_view raw, Normalized& out) {
  std::string const text = fold(raw);
  out.clear();
  if (text.empty())
    return;
  Tokens t = text | std::views::split(' ') | std::ranges::to<Tokens>();
  t = split_glued(t);
  t = convert_numbers(t);
  t = apply_synonyms(t);
  canonicalize(t);
  merge_dir_pairs(t);
  if (!parse_route(t, out))
    split_components(t, out);
}

void query_core_alternatives(Normalized const& n, std::vector<CoreAlt>& out) {
  out.clear();
  if (n.empty())
    return;
  out.push_back({ n.core, 1.0f, AltKind::Exact, 0 });
  if (n.comp.route == RouteNet::Hwy) {
    for (std::string_view const prefix : { "#us ", "#sr ", "#cr " })
      out.push_back({ std::string(prefix) + n.core.substr(5), 0.9f,
                      AltKind::RouteNetwork, 0 });
  } else if (n.is_route()) {
    return;
    // "AIRPORT AND 295": a bare number is some route 295
  } else if (n.comp.type == 0 && all_digits(n.core) &&
             is_route_number(n.core)) {
    for (std::string_view const prefix :
         { "#i ", "#us ", "#sr ", "#cr ", "#hwy " })
      out.push_back(
          { std::string(prefix) + n.core, 0.8f, AltKind::BareNumber, 0 });
    // "Magnolia Ridge" may be "Magnolia" + the type word Ridge
  } else if (auto const sp = n.core.rfind(' '); sp != std::string::npos) {
    if (Word const* w = find_word(std::string_view(n.core).substr(sp + 1));
        w != nullptr && w->type != 0)
      out.push_back(
          { n.core.substr(0, sp), 0.85f, AltKind::TrimmedSuffix, w->type });
  }
}

} // namespace chiasmos::norm
