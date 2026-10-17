// Normalizer tests: hand-written rule cases.
#include "normalize/normalizer.hpp"

#include <gtest/gtest.h>

#include <format>
#include <string>

namespace chiasmos::norm {
namespace {

Normalized norm(std::string_view raw) {
  Normalized n;
  normalize_street(raw, n);
  return n;
}

struct Case {
  std::string_view raw, full, core;
};
void expect_all(std::initializer_list<Case> cases) {
  for (auto const& c : cases) {
    Normalized const n = norm(c.raw);
    EXPECT_EQ(n.full, c.full) << c.raw;
    EXPECT_EQ(n.core, c.core) << c.raw;
  }
}

std::vector<std::string> alts(std::string_view raw) {
  std::vector<CoreAlt> out;
  query_core_alternatives(norm(raw), out);
  std::vector<std::string> r;
  for (auto const& a : out)
    r.push_back(std::format("{}|{:.2f}|{}", a.core,
                            static_cast<double>(a.prior),
                            static_cast<int>(a.kind)));
  return r;
}

TEST(Normalize, EmptyInput) {
  for (char const* raw : { "", "   ", "-", "#", "'", "()", ". , ;", "北" })
    EXPECT_TRUE(norm(raw).empty()) << raw;
}

TEST(Normalize, Streets) {
  expect_all({
      { "West Broad Street", "w broad st", "broad" },
      { "W Broad St", "w broad st", "broad" },
      { "W. BROAD ST.", "w broad st", "broad" },
      { "Mechanicsville Turnpike", "mechanicsville tpke", "mechanicsville" },
      { "Mechanicsville Tpke", "mechanicsville tpke", "mechanicsville" },
      { "Mechanicsville Trpk", "mechanicsville tpke", "mechanicsville" },
      { "8Th St", "8th st", "8th" },
      { "Eighth Street", "8th st", "8th" },
      { "8th St.", "8th st", "8th" },
      { "Nine Mile Rd", "9 mile rd", "9 mile" },
      { "9 Mile Road", "9 mile rd", "9 mile" },
      { "North Road", "n rd", "n" },
      { "Avenue A", "ave a", "a" },
      { "Avenue N", "ave n", "n" },
      { "Park Avenue", "park ave", "park" },
      { "Saint Augustine Road", "st augustine rd", "st augustine" },
      { "St Augustine Rd", "st augustine rd", "st augustine" },
      { "Old Osborne Tpke", "old osborne tpke", "old osborne" },
      { "Osborne Turnpike", "osborne tpke", "osborne" },
      { "Dr Martin Luther King Jr Blvd", "mlk blvd", "mlk" },
      { "MLK Blvd", "mlk blvd", "mlk" },
      { "Dr Martin L King Blvd", "mlk blvd", "mlk" },
      { "Mc Donald Rd", "mc donald rd", "mc donald" },
      { "McDonald Road", "mcdonald rd", "mcdonald" },
      { "N Main St", "n main st", "main" },
      { "Main St N", "main st n", "main" },
      { "East West Hwy", "e w hwy", "e w" },
      { "NW 146 St", "nw 146th st", "146th" },
      { "NW 146th Street", "nw 146th st", "146th" },
      { "Old Rt 33", "old rte 33", "old rte 33" },
      { "Old Route 33", "old rte 33", "old rte 33" },
      { "Hardings Trace", "hardings trce", "hardings" },
  });
}

TEST(Normalize, ComponentsOfAStreet) {
  Normalized n = norm("North West Main Street East");
  EXPECT_EQ(n.full, "nw main st e");
  EXPECT_EQ(n.comp.pre_dir, Dir::NW);
  EXPECT_EQ(n.comp.suf_dir, Dir::E);
  EXPECT_EQ(type_name(n.comp.type), "st");
  EXPECT_EQ(n.comp.type_pos, TypePos::Suffix);
  EXPECT_EQ(to_string(Dir::None), "");
  EXPECT_EQ(to_string(Dir::SW), "sw");

  n = norm("Avenue A");
  EXPECT_EQ(n.comp.type_pos, TypePos::Prefix);
  EXPECT_EQ(type_name(n.comp.type), "ave");

  n = norm("Main St Ext"); // post-modifiers: EXT BUS BYP ALT CONN TRUCK SPUR
                           // (EXPR only in a route)
  EXPECT_EQ(n.full, "main st ext");
  EXPECT_EQ(n.comp.post_mod, EXT);
  EXPECT_EQ(norm("Main Street Business Bypass").comp.post_mod, BUS | BYP);
  EXPECT_EQ(norm("Main Spur").full, "main spur");

  // The core is never emptied by a type, a directional or a modifier.
  expect_all({ { "Old Rd", "old rd", "old rd" },
               { "The Rd", "the rd", "the rd" },
               { "New St", "new st", "new st" },
               { "Elm", "elm", "elm" },
               { "North", "n", "n" },
               { "South Road", "s rd", "s" },
               { "West Ave", "w ave", "w" },
               { "Court", "ct", "ct" },
               { "Avenue 5", "ave 5th", "5th" } });
}

TEST(Normalize, DirectionalPairs) {
  expect_all({ { "North East Main St", "ne main st", "main" },
               { "N.E. Main St", "ne main st", "main" },
               { "S W 3rd St", "sw 3rd st", "3rd" },
               { "North East", "n e", "n" },
               { "North South Main St", "n s main st", "s main" } });
}

TEST(Normalize, Numbers) {
  expect_all({ { "First Ave", "1st ave", "1st" },
               { "Second Ave", "2nd ave", "2nd" },
               { "Twelfth St", "12th st", "12th" },
               { "Twenty First St", "21st st", "21st" },
               { "Twenty-One Rd", "21st rd", "21st" },
               { "Ninetieth St", "90th st", "90th" },
               { "Two Notch Rd", "2 notch rd", "2 notch" },
               { "1St", "1st", "1st" },
               { "3d St", "3rd st", "3rd" },
               { "22d Ave", "22nd ave", "22nd" },
               { "12nd Rd", "12th rd", "12th" },
               { "21th St", "21st st", "21st" },
               { "04th St", "04th st", "04th" },
               { "08rd St", "8th st", "8th" },
               { "146 St", "146th st", "146th" },
               { "W 3 St", "w 3rd st", "3rd" },
               { "1d St", "1d st", "1d" },
               { "Route 66", "#hwy 66", "#hwy 66" } });
}

TEST(Normalize, Punctuation) {
  expect_all({ { "O'Brien St", "obrien st", "obrien" },
               { "O’Brien St", "obrien st", "obrien" },
               { "St. Mary's Rd", "st marys rd", "st marys" },
               { "AT&T Way", "at and t way", "at and t" },
               { "Main & Oak", "main and oak", "main and oak" },
               { "Rock-Creek Pkwy", "rock crk pkwy", "rock crk" },
               { "#5 Rd", "5th rd", "5th" },
               { "  Main   St  ", "main st", "main" } });
}

TEST(Normalize, AccentsAreFolded) {
  expect_all({ { "Café Rd", "cafe rd", "cafe" },
               { "Ñandú Rd", "nandu rd", "nandu" },
               { "Straße", "strasse", "strasse" },
               { "Łódź Ave", "lodz ave", "lodz" },
               { "Ægir Rd", "aegir rd", "aegir" },
               { "1½ St", "1 1 2 st", "1 1 2" },
               { "é Rd", "e rd", "e" } });
}

TEST(Normalize, TypesMatchTheUspsList) {
  expect_all({ { "Elm Court", "elm ct", "elm" },
               { "Elm Crt", "elm ct", "elm" },
               { "Elm Crescent", "elm cres", "elm" },
               { "Elm Crossing", "elm xing", "elm" },
               { "Park Place", "park pl", "park" },
               { "Pocahontas Parkway", "pocahontas pkwy", "pocahontas" },
               { "Pocahontas Pky", "pocahontas pkwy", "pocahontas" },
               { "Main Expressway", "main expy", "main" },
               { "Mount Vernon Ave", "mt vernon ave", "mt vernon" },
               { "Fort Hunt Rd", "ft hunt rd", "ft hunt" },
               { "Rue Royale", "rue royale", "royale" },
               { "Calle Ocho", "calle ocho", "ocho" },
               { "Via Roma", "via roma", "roma" } });
  EXPECT_EQ(norm("Mill Trk").comp.post_mod,
            TRUCK); // trk is the TRUCK modifier, not TRACK
  EXPECT_EQ(type_name(norm("Elm Track").comp.type), "trak");
  EXPECT_EQ(norm("Mount").comp.type,
            0); // a type word only counts as a type next to a name
}

TEST(Normalize, TypeIdsAreStable) {
  // Type ids are stored as bytes in the index: this pins the table order. Never
  // reorder or remove rows of kTypes.
  EXPECT_EQ(type_name(0), "");
  EXPECT_EQ(type_name(167), "st");
  EXPECT_EQ(type_name(202), "");
  std::uint64_t h = 0xcbf29ce484222325;
  for (std::uint16_t t = 1; t <= 201; ++t) {
    for (char const c : type_name(t))
      h = (h ^ static_cast<unsigned char>(c)) * 0x100000001b3;
    h = h * 0x100000001b3; // the terminating NUL
  }
  EXPECT_EQ(h, 0x1abde00b7a4a5831u);
  EXPECT_EQ(norm("W Broad St").comp.type, 167);
}

TEST(Normalize, Routes) {
  for (char const* raw :
       { "I-64", "I64", "Interstate 64", "Interstate Highway 64", "IH 64",
         "I 64", "i-64", "I.64" }) {
    Normalized const n = norm(raw);
    EXPECT_EQ(n.full, "#i 64") << raw;
    EXPECT_EQ(n.core, "#i 64") << raw;
    EXPECT_EQ(n.comp.route, RouteNet::Interstate) << raw;
    EXPECT_EQ(n.comp.suf_dir, Dir::None) << raw;
  }
  for (char const* raw :
       { "US Highway 250", "U.S. Route 250", "US-250", "US 250", "US250",
         "United States Highway 250", "U S Hwy 250" }) {
    EXPECT_EQ(norm(raw).full, "#us 250") << raw;
    EXPECT_EQ(norm(raw).comp.route, RouteNet::US) << raw;
  }
  for (char const* raw : { "VA 33", "VA33", "State Route 33", "S.R. 33",
                           "SR-33", "State Hwy 33", "St Rd 33" }) {
    EXPECT_EQ(norm(raw).full, "#sr 33") << raw;
    EXPECT_EQ(norm(raw).comp.route, RouteNet::State) << raw;
  }
  for (char const* raw :
       { "County Road 1568", "CR-1568", "Co Rd 1568", "Cnty Hwy 1568" }) {
    EXPECT_EQ(norm(raw).full, "#cr 1568") << raw;
    EXPECT_EQ(norm(raw).comp.route, RouteNet::County) << raw;
  }
  for (char const* raw :
       { "Route 33", "Rt 33", "Hwy 33", "Highway 33", "Rte. 33" }) {
    EXPECT_EQ(norm(raw).full, "#hwy 33") << raw;
    EXPECT_EQ(norm(raw).comp.route, RouteNet::Hwy) << raw;
  }
  // Synonyms are cross products of head and tail words.
  EXPECT_EQ(norm("Cnty Rte 5").full, "#cr 5");
  EXPECT_EQ(norm("United States Road 5").full, "#us 5");
  EXPECT_EQ(norm("Highway 5").core, "#hwy 5");
  EXPECT_TRUE(norm("Highway 5").is_route());
  EXPECT_FALSE(norm("Highway Five Rd").is_route());
}

TEST(Normalize, RouteTravelDirectionAndModifiers) {
  for (char const* raw : { "E I64", "I64 E", "I-64 East" }) {
    Normalized const n = norm(raw);
    EXPECT_EQ(n.full, "#i 64") << raw;
    EXPECT_EQ(n.comp.suf_dir, Dir::E) << raw;
    EXPECT_EQ(n.comp.pre_dir, Dir::None) << raw;
  }
  EXPECT_EQ(norm("I-64 EB").core,
            "i 64 eb"); // travel words are not understood: "I-64 EB" is a
                        // street named "i 64 eb"

  Normalized n = norm("US 19 Alt");
  EXPECT_EQ(n.full, "#us 19 alt");
  EXPECT_EQ(n.core, "#us 19");
  EXPECT_EQ(n.comp.post_mod, ALT);
  n = norm("N US 1 Business");
  EXPECT_EQ(n.full, "#us 1 bus");
  EXPECT_EQ(n.comp.suf_dir, Dir::N);
  EXPECT_EQ(n.comp.post_mod, BUS);
  EXPECT_EQ(norm("I-95 Expy").full, "#i 95 expr");
  EXPECT_EQ(norm("I-95 Expy").comp.post_mod, EXPR);
  EXPECT_EQ(norm("US 460 Bypass").full, "#us 460 byp");
  EXPECT_EQ(norm("US 29 Truck").full, "#us 29 truck");
  EXPECT_EQ(norm("US 15 Connector").full, "#us 15 conn");
  EXPECT_EQ(norm("SR 7 Ext").full, "#sr 7 ext");
  EXPECT_EQ(norm("VA 5 Toll").full,
            "#sr 5"); // toll and scenic are accepted and dropped
  EXPECT_EQ(norm("Hwy 11 Alt E").comp.suf_dir, Dir::E);

  // A route must be the whole name: anything else stays a street.
  EXPECT_FALSE(norm("Old Route 33").is_route());
  EXPECT_FALSE(norm("I-64 Business Loop Rd").is_route());
  EXPECT_FALSE(norm("US 250 Pkwy").is_route());
  EXPECT_FALSE(norm("Main I-64").is_route());
  EXPECT_FALSE(norm("I 64 E W").is_route());
  EXPECT_EQ(norm("Mount 5").full, "#sr 5"); // "mt" is also a state code; before
                                            // a number it is a route prefix
  EXPECT_EQ(norm("Wy 59").full, "#sr 59");
  EXPECT_EQ(norm("Wy Rd").full, "way rd");
  EXPECT_EQ(norm("SR 5000").full, "#sr 5000");
  EXPECT_FALSE(norm("SR 50000").is_route());
  expect_all({ { "A1A", "a1a", "a1a" },
               { "Route A1A", "#hwy a1a", "#hwy a1a" },
               { "US 31E", "#us 31e", "#us 31e" } });
}

TEST(Normalize, QueryCoreAlternatives) {
  using V = std::vector<std::string>;
  EXPECT_EQ(alts("W Broad St"), (V{ "broad|1.00|0" }));
  EXPECT_EQ(alts("I-64 E"), (V{ "#i 64|1.00|0" }));
  EXPECT_EQ(alts("Route 33"), (V{ "#hwy 33|1.00|0", "#us 33|0.90|1",
                                  "#sr 33|0.90|1", "#cr 33|0.90|1" }));
  EXPECT_EQ(alts("295"),
            (V{ "295|1.00|0", "#i 295|0.80|2", "#us 295|0.80|2",
                "#sr 295|0.80|2", "#cr 295|0.80|2", "#hwy 295|0.80|2" }));
  EXPECT_EQ(alts("Magnolia Ridge Dr"),
            (V{ "magnolia rdg|1.00|0", "magnolia|0.85|3" }));
  EXPECT_EQ(alts("Hardings Trace"), (V{ "hardings|1.00|0" }));
  EXPECT_EQ(alts("Black Mountain Rd"),
            (V{ "black mtn|1.00|0", "black|0.85|3" }));
  EXPECT_EQ(
      alts("N 295 St"),
      (V{ "295th|1.00|0" })); // a numbered street is not a bare route number
  EXPECT_TRUE(alts("").empty());
  EXPECT_TRUE(alts("--").empty());

  std::vector<CoreAlt> out;
  query_core_alternatives(norm("Magnolia Ridge Dr"), out);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0].kind, AltKind::Exact);
  EXPECT_EQ(out[1].kind, AltKind::TrimmedSuffix);
  EXPECT_EQ(type_name(out[1].type), "rdg"); // the dropped word's type
  query_core_alternatives(norm(""), out);   // the vector is cleared first
  EXPECT_TRUE(out.empty());
}

} // namespace
} // namespace chiasmos::norm
