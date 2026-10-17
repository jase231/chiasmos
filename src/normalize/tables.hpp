#pragma once
// the word tables and the small helpers shared by
// normalizer.cpp, route.cpp, freeform.cpp and unicode.cpp
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "normalize/normalizer.hpp"

namespace chiasmos::norm {

using Tokens = std::vector<std::string>;

constexpr std::string_view first_word(std::string_view row) {
  return row.substr(0, row.find(' '));
}
constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool is_alpha(char c) { return c >= 'a' && c <= 'z'; }
[[nodiscard]] bool all_digits(std::string_view s);
[[nodiscard]] bool is_route_number(std::string_view t);

// street types from USPS Pub 28 Appendix C1 plus a few spelling variants,
// "standard variant variant ...".
inline constexpr std::array<std::string_view, 201> kTypes{
  "aly allee alley ally",
  "anx anex annex annx",
  "arc arcade",
  "ave av aven avenu avenue avn avnue",
  "bch beach",
  "bg burg",
  "bgs burgs",
  "blf bluf bluff",
  "blfs bluffs",
  "blvd blv boul boulevard boulv bvd",
  "bnd bend",
  "br branch brnch",
  "brg brdge bridge",
  "brk brook",
  "brks brooks",
  "btm bot bottm bottom",
  "byu bayoo bayou",
  "calle",
  "camino",
  "cir circ circl circle crcl crcle",
  "cirs circles",
  "clb club",
  "clf cliff",
  "clfs cliffs",
  "cmn common",
  "cmns commons",
  "cor corner",
  "cors corners",
  "cp camp cmp",
  "cpe cape",
  "cres crescent crsent crsnt",
  "crk creek",
  "crse course",
  "crst crest",
  "cswy causeway causwa",
  "ct court crt",
  "ctr cen cent center centr centre cnter cntr",
  "ctrs centers",
  "cts courts",
  "curv curve",
  "cv cove",
  "cvs coves",
  "cyn canyn canyon cnyn",
  "dl dale",
  "dm dam",
  "dr driv drive drv",
  "drs drives",
  "dv div divide dvd",
  "est estate",
  "ests estates",
  "expy exp expr express expressway expw expwy",
  "exts extensions",
  "fall",
  "fld field",
  "flds fields",
  "fls falls",
  "flt flat",
  "flts flats",
  "frd ford",
  "frds fords",
  "frg forg forge",
  "frgs forges",
  "frk fork",
  "frks forks",
  "frst forest forests",
  "fry ferry frry",
  "ft fort frt",
  "fwy freeway freewy frway frwy",
  "gdn garden gardn grden grdn",
  "gdns gardens grdns",
  "gln glen",
  "glns glens",
  "grn green",
  "grns greens",
  "grv grov grove",
  "grvs groves",
  "gtwy gateway gatewy gatway gtway",
  "hbr harb harbor harbr hrbor",
  "hbrs harbors",
  "hl hill",
  "hls hills",
  "holw hllw hollow hollows holws",
  "hts heights ht",
  "hvn haven",
  "hwy highway highwy hiway hiwy hway",
  "inlt inlet",
  "is island islnd",
  "isle isles",
  "iss islands islnds",
  "jct jction jctn junction junctn juncton",
  "jcts jctns junctions",
  "knl knol knoll",
  "knls knolls",
  "ky key",
  "kys keys",
  "land",
  "lck lock",
  "lcks locks",
  "ldg ldge lodg lodge",
  "lf loaf",
  "lgt light",
  "lgts lights",
  "lk lake",
  "lks lakes",
  "ln lane",
  "lndg landing lndng",
  "loop loops",
  "mall",
  "mdw meadow",
  "mdws meadows medows",
  "mews",
  "ml mill",
  "mls mills",
  "mnr manor",
  "mnrs manors",
  "msn mission missn mssn",
  "mt mnt mount",
  "mtn mntain mntn mountain mountin mtin",
  "mtns mntns mountains",
  "mtwy motorway",
  "nck neck",
  "opas overpass",
  "orch orchard orchrd",
  "oval ovl",
  "park parks prk",
  "pass",
  "path paths",
  "pike pikes",
  "pkwy parkway parkways parkwy pkway pkwys pky",
  "pl place",
  "pln plain",
  "plns plains",
  "plz plaza plza",
  "pne pine",
  "pnes pines",
  "pr prairie prr",
  "prt port",
  "prts ports",
  "psge passage",
  "pt point",
  "pts points",
  "radl rad radial radiel",
  "ramp",
  "rd road",
  "rdg rdge ridge",
  "rdgs ridges",
  "rds roads",
  "riv river rivr rvr",
  "rnch ranch ranches rnchs",
  "row",
  "rpd rapid",
  "rpds rapids",
  "rst rest",
  "rte route rt",
  "rue",
  "run",
  "shl shoal",
  "shls shoals",
  "shr shoar shore",
  "shrs shoars shores",
  "skwy skyway",
  "smt sumit sumitt summit",
  "spg spng spring sprng",
  "spgs spngs springs sprngs",
  "sq sqr sqre squ square",
  "sqs sqrs squares",
  "st str street strt",
  "sta station statn stn",
  "stra strav straven stravenue stravn strvn strvnue",
  "strm stream streme",
  "sts streets",
  "ter terr terrace",
  "tpke trnpk trpk turnpike turnpk",
  "trak track tracks trks",
  "trce trace traces",
  "trfy trafficway",
  "trl trail trails trls",
  "trlr trailer trlrs",
  "trwy throughway",
  "tunl tunel tunls tunnel tunnels tunnl",
  "un union",
  "uns unions",
  "upas underpass",
  "via vdct viadct viaduct",
  "vis vist vista vst vsta",
  "vl ville",
  "vlg vill villag village villg villiage",
  "vlgs villages",
  "vly valley vally vlly",
  "vlys valleys",
  "vw view",
  "vws views",
  "walk walks",
  "wall",
  "way wy",
  "ways",
  "wl well",
  "wls wells",
  "xing crossing crssng",
  "xrd crossroad",
  "xrds crossroads",
};
// directionals
inline constexpr std::array<std::string_view, 8> kDirs{
  "n north",      "s south",      "e east",       "w west",
  "ne northeast", "nw northwest", "se southeast", "sw southwest"
};
// post-modifiers
inline constexpr std::array<std::string_view, 7> kMods{
  "ext extension extd extn extnsn",
  "bus business",
  "byp bypass bypa bypas byps",
  "alt alternate",
  "conn connector",
  "truck trk",
  "spur spurs"
};
inline constexpr std::string_view kExprWord = "expy";
// types that stand in front of the name ("Avenue A", "Calle Ocho"), all are
// also in kTypes
inline constexpr std::array<std::string_view, 6> kPrefixTypes{
  "ave", "calle", "camino", "via", "rue", "plz"
};
// "Old Rd" keeps its type word: these may not be the whole core left after the
// type is cut
inline constexpr std::array<std::string_view, 3> kCoreStopwords{ "the", "old",
                                                                 "new" };
// USPS state codes. A state code before a route number is a route prefix ("VA
// 33"), except ne and co
inline constexpr std::string_view kStates =
    "al ak az ar ca co ct de dc fl ga gu hi id il in ia ks ky la me md ma mi "
    "mn ms mo mt ne nv nh nj nm ny nc nd oh ok or pa pr ri sc sd tn tx ut vt "
    "va vi wa wv wi wy";
[[nodiscard]] bool is_state_code(std::string_view w);
// one to nineteen and the tens, cardinal and an ordinal word
inline constexpr std::array<std::pair<std::string_view, std::string_view>, 27>
    kNumbers{ { { "one", "first" },
                { "two", "second" },
                { "three", "third" },
                { "four", "fourth" },
                { "five", "fifth" },
                { "six", "sixth" },
                { "seven", "seventh" },
                { "eight", "eighth" },
                { "nine", "ninth" },
                { "ten", "tenth" },
                { "eleven", "eleventh" },
                { "twelve", "twelfth" },
                { "thirteen", "thirteenth" },
                { "fourteen", "fourteenth" },
                { "fifteen", "fifteenth" },
                { "sixteen", "sixteenth" },
                { "seventeen", "seventeenth" },
                { "eighteen", "eighteenth" },
                { "nineteen", "nineteenth" },
                { "twenty", "twentieth" },
                { "thirty", "thirtieth" },
                { "forty", "fortieth" },
                { "fifty", "fiftieth" },
                { "sixty", "sixtieth" },
                { "seventy", "seventieth" },
                { "eighty", "eightieth" },
                { "ninety", "ninetieth" } } };
// canonical route prefixes and their network
struct RouteWord {
  std::string_view word;
  RouteNet net;
};
inline constexpr std::array<RouteWord, 6> kRouteWords{
  { { "i", RouteNet::Interstate },
    { "us", RouteNet::US },
    { "sr", RouteNet::State },
    { "cr", RouteNet::County },
    { "rte", RouteNet::Hwy },
    { "hwy", RouteNet::Hwy } }
};

[[nodiscard]] constexpr std::string_view mod_name(unsigned bit) {
  return bit < kMods.size() ? first_word(kMods[bit]) : "expr";
}
inline void append_mods(std::string& full, std::uint8_t mods) {
  for (unsigned bit = 0; bit < 8; ++bit)
    if ((unsigned{ mods } >> bit) & 1u)
      full.append(" ").append(mod_name(bit));
}
[[nodiscard]] constexpr char const* ordinal_suffix(unsigned long n) {
  if (n % 100 >= 11 && n % 100 <= 13)
    return "th";
  return n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
}

// how one word can be used by the normalizer
struct Word {
  std::uint16_t type = 0;   // street type id, 0 = not a street type
  Dir dir{};                // directional, ::None = not a directional
  std::uint8_t mod = 0;     // post-modifier bit, 0 = not a modifier
  bool prefix_type = false; // can stand in front of the name
  bool state = false;       // usable as a route prefix
};
// nullptr: an ordinary word
[[nodiscard]] Word const* find_word(std::string_view w);
// standard spelling of a type, directional or modifier
[[nodiscard]] std::string_view canonical(Word const& w);
[[nodiscard]] inline Dir dir_of(std::string_view w) {
  Word const* i = find_word(w);
  return i == nullptr ? Dir::None : i->dir;
}

// number: route number must follow; Whole: phrase + number are entire name
enum class Need : std::uint8_t { Nothing, Number, Whole };
// multi-word synonyms ("state route" -> "sr", "dr martin luther king jr" ->
// "mlk")
struct Synonym {
  Tokens from;
  std::string_view to;
  Need need = Need::Nothing;
};
[[nodiscard]] std::span<Synonym const>
synonyms_starting_with(std::string_view first);

// lowercase words of `raw` separated by single spaces
[[nodiscard]] std::string fold(std::string_view raw);
// whole-name match of [DIR] PREFIX N [MOD...] [DIR]
[[nodiscard]] bool parse_route(Tokens const& t, Normalized& out);

} // namespace chiasmos::norm
