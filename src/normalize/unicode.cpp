// convert UTF-8 text to lowercase ASCII words
#include <algorithm>
#include <optional>

#include "normalize/tables.hpp"

namespace chiasmos::norm {
namespace {

struct Fold {
  char32_t lo, hi;
  std::string_view to; // lowercase ASCII
};
// sorted by Fold::lo
constexpr std::array kFold{
  Fold{ 0xBC, 0xBC, " 1 4" },  Fold{ 0xBD, 0xBD, " 1 2" },
  Fold{ 0xBE, 0xBE, " 3 4" }, // fractions: "1½ St" is "1 1 2 st"
  Fold{ 0xDF, 0xDF, "ss" },    Fold{ 0xE0, 0xE5, "a" },
  Fold{ 0xE6, 0xE6, "ae" },    Fold{ 0xE7, 0xE7, "c" },
  Fold{ 0xE8, 0xEB, "e" },     Fold{ 0xEC, 0xEF, "i" },
  Fold{ 0xF0, 0xF0, "d" },     Fold{ 0xF1, 0xF1, "n" },
  Fold{ 0xF2, 0xF6, "o" },     Fold{ 0xF8, 0xF8, "o" },
  Fold{ 0xF9, 0xFC, "u" },     Fold{ 0xFD, 0xFD, "y" },
  Fold{ 0xFE, 0xFE, "th" },    Fold{ 0xFF, 0xFF, "y" },
  Fold{ 0x100, 0x105, "a" },   Fold{ 0x106, 0x10D, "c" },
  Fold{ 0x10E, 0x111, "d" },   Fold{ 0x112, 0x11B, "e" },
  Fold{ 0x11C, 0x123, "g" },   Fold{ 0x124, 0x127, "h" },
  Fold{ 0x128, 0x131, "i" },   Fold{ 0x132, 0x133, "ij" },
  Fold{ 0x134, 0x135, "j" },   Fold{ 0x136, 0x138, "k" },
  Fold{ 0x139, 0x142, "l" },   Fold{ 0x143, 0x14B, "n" },
  Fold{ 0x14C, 0x151, "o" },   Fold{ 0x152, 0x153, "oe" },
  Fold{ 0x154, 0x159, "r" },   Fold{ 0x15A, 0x161, "s" },
  Fold{ 0x162, 0x167, "t" },   Fold{ 0x168, 0x173, "u" },
  Fold{ 0x174, 0x175, "w" },   Fold{ 0x176, 0x178, "y" },
  Fold{ 0x179, 0x17E, "z" },   Fold{ 0x17F, 0x17F, "s" },
  Fold{ 0x2BC, 0x2BC, "'" },   Fold{ 0x300, 0x36F, "" }, // combining accents
  Fold{ 0x2018, 0x2019, "'" }, Fold{ 0x2032, 0x2032, "'" }
};
static_assert(std::ranges::is_sorted(kFold, {}, &Fold::lo));

std::optional<std::string_view> transliterate(char32_t cp) {
  if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7)
    cp += 0x20;
  auto const it = std::ranges::upper_bound(kFold, cp, {}, &Fold::lo);
  if (it == kFold.begin() || cp > std::prev(it)->hi)
    return std::nullopt;
  return std::prev(it)->to;
}

std::pair<char32_t, std::size_t> decode(std::string_view s) {
  auto const lead = static_cast<unsigned char>(s[0]);
  std::size_t const n = lead >= 0xF0   ? 4
                        : lead >= 0xE0 ? 3
                        : lead >= 0xC0 ? 2
                                       : 1;
  if (n == 1)
    return { lead < 0x80 ? lead : 0xFFFD, 1 };
  if (n > s.size())
    return { 0xFFFD, 1 };
  char32_t cp = lead & (0x7Fu >> n);
  for (std::size_t i = 1; i < n; ++i) {
    auto const b = static_cast<unsigned char>(s[i]);
    if ((b & 0xC0) != 0x80)
      return { 0xFFFD, 1 };
    cp = (cp << 6) | (b & 0x3Fu);
  }
  return { cp, n };
}

} // namespace

std::string fold(std::string_view raw) {
  std::string out;
  bool gap = false;
  auto const put = [&](char c) {
    if (gap && !out.empty())
      out += ' ';
    gap = false;
    out += c;
  };
  auto const map_ascii = [&](char c) {
    if (c >= 'A' && c <= 'Z')
      put(static_cast<char>(c - 'A' + 'a'));
    else if (is_alpha(c) || is_digit(c))
      put(c);
    else if (c == '&')
      gap = true, put('a'), put('n'), put('d'), gap = true;
    else if (c != '\'' && c != '#')
      gap = true;
  };
  for (std::size_t i = 0; i < raw.size();) {
    auto const [cp, len] = decode(raw.substr(i));
    i += len;
    if (cp < 0x80)
      map_ascii(static_cast<char>(cp));
    else if (auto const to = transliterate(cp))
      std::ranges::for_each(*to, map_ascii);
    else
      gap = true;
  }
  return out;
}

} // namespace chiasmos::norm
