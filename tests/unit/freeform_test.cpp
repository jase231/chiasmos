// parse_freeform tests. Most strings come from the Henrico CAD export
// (data/eval/cad-1.csv).
#include <gtest/gtest.h>

#include "normalize/normalizer.hpp"

namespace chiasmos::norm {
namespace {

struct Case {
  std::string_view query;
  Form form;
  std::string_view street1, street2, locality;
};
void expect_all(std::initializer_list<Case> cases) {
  for (auto const& c : cases) {
    ParsedQuery const p = parse_freeform(c.query);
    EXPECT_EQ(static_cast<int>(p.form), static_cast<int>(c.form)) << c.query;
    EXPECT_EQ(p.street1, c.street1) << c.query;
    EXPECT_EQ(p.street2, c.street2) << c.query;
    EXPECT_EQ(p.locality, c.locality) << c.query;
  }
}
constexpr auto kX = Form::Intersection;
constexpr auto kOne = Form::SingleStreet;

// CAD intersections: "&" is by far the most common separator (7513 of 12280
// distinct strings).
TEST(Freeform, Ampersand) {
  expect_all({
      { "Glenside Dr & W Broad St", kX, "glenside dr", "w broad st", "" },
      { "Grand Reserve & Gran", kX, "grand reserve", "gran",
        "" }, // a truncated CAD field is passed through untouched
      { "WHISPERING BREEZE CT & N CRESTWOOD AVE", kX, "whispering breeze ct",
        "n crestwood ave", "" },
      { "I64 & I64 E Laburnum Ave Ex 195A", kX, "i64",
        "i64 e laburnum ave ex 195a", "" }, // ramp text stays in the side
      { "Rockport Dr&Three Chopt Rd", kX, "rockport dr", "three chopt rd", "" },
  });
}

TEST(Freeform, OtherStrongSeparators) {
  expect_all({
      { "I64 E -- W BROAD ST (SHORT PUMP)", kX, "i64 e", "w broad st",
        "" }, // "--" splits; the parenthetical is dropped
      { "WILLIAMSBURG RD-- NEW KENT LINE", kX, "williamsburg rd",
        "new kent line", "" },
      { "BROOKHILL AZALEA SC XST AZALEA AVE", kX, "brookhill azalea sc",
        "azalea ave", "" },
      { "Main St @ Oak St", kX, "main st", "oak st", "" },
      { "Main St / Oak St", kX, "main st", "oak st", "" },
      { "MAIN ST/OAK ST", kX, "main st", "oak st", "" },
      { "Main St & Oak St & Elm St", kX, "main st", "oak st",
        "" }, // a third side is ignored
  });
}

// "and", "at" and "x" are separators only when no strong separator exists.
TEST(Freeform, WeakSeparators) {
  expect_all({
      { "AIRPORT AND 295", kX, "airport", "295", "" },
      { "Main St at Oak St", kX, "main st", "oak st", "" },
      { "Main St x Oak Ave", kX, "main st", "oak ave", "" },
      { "Main St x Oak", kOne, "main st x oak", "",
        "" }, // "x" needs a street type or route on both sides
      { "A and B and C", kX, "a", "b and c",
        "" }, // nothing looks like a street: the first "and" splits
      { "Main St and Oak Ave and Elm St", kX, "main st", "oak ave and elm st",
        "" },
      { "Black and Decker Rd & Main St", kX, "black and decker rd", "main st",
        "" }, // the strong "&" wins
      { "Black and Decker Rd and Main St", kX, "black and decker rd", "main st",
        "" }, // the split with two street-like sides wins
      { "3733 King And Queen Dr", kOne, "3733 king and queen dr", "",
        "" }, // after a house number "and" is a name word
      { "3733A King And Queen Dr", kOne, "3733a king and queen dr", "", "" },
      { "And Main St", kOne, "and main st", "",
        "" }, // a separator word at either end is a name word
      { "Main St And", kOne, "main st and", "", "" },
  });
}

TEST(Freeform, LeadingPhrase) {
  expect_all({
      { "intersection of Main St and Oak St", kX, "main st", "oak st", "" },
      { "corner of Main St and Oak St", kX, "main st", "oak st", "" },
      { "cnr of Main St & Oak St", kX, "main st", "oak st", "" },
      { "Corner Of Main St & Oak St", kX, "main st", "oak st", "" },
      { "corner of Main St", kOne, "main st", "", "" },
      { "corner of", kOne, "corner of", "",
        "" }, // the phrase needs something after it
  });
}

// CAD strings with no separator are not intersections; they are SingleStreet,
// which the engine answers with NotAnIntersection.
TEST(Freeform, SingleStreet) {
  expect_all({
      { "Ainsworth Ln", kOne, "ainsworth ln", "", "" },
      { "5900 Edgelawn St", kOne, "5900 edgelawn st", "", "" },
      { "2000 Block Mcdonald Rd", kOne, "2000 block mcdonald rd", "",
        "" },                                 // 30824 of 43819 CAD rows
      { "176 I64", kOne, "176 i64", "", "" }, // a mile marker
      { "CITY OF RICHMOND MECHANICSVILLE (RT 360)", kOne,
        "city of richmond mechanicsville", "", "" },
      { "CAP TRAIL MM 46 (1412 NEW MARKET)", kOne, "cap trail mm 46", "", "" },
      { "I295 S Pocahontas Pkwy E En 25", kOne,
        "i295 s pocahontas pkwy e en 25", "", "" }, // a ramp designator
      { "& Broad St", kOne, "broad st", "",
        "" }, // a missing side is never guessed
      { "Main St &", kOne, "main st", "", "" },
      { "Main St &&", kOne, "main st", "", "" },
  });
}

TEST(Freeform, EmptyAndNoise) {
  for (char const* q : { "", "   ", "()", "(Main St)", "&", " & ", "--", "-",
                         "\"\"", ".", ",", "( unbalanced & Main St" })
    EXPECT_EQ(parse_freeform(q).form, Form::Empty) << q;
  expect_all({ { "\"Main St & Oak St\"", kX, "main st", "oak st", "" },
               { "  Main   St  &\tOak St  ", kX, "main st", "oak st", "" },
               { "MAIN ST. & OAK ST.", kX, "main st", "oak st", "" },
               { "tracks- laburnum & i-64", kX, "tracks laburnum", "i-64", "" },
               { "Main St (old) & Oak St", kX, "main st", "oak st", "" },
               { "Main St - Oak St", kOne, "main st oak st", "", "" } });
}

// chiasmos has no city polygons, but a caller may append one; it is cut off and
// returned for information.
TEST(Freeform, TrailingLocality) {
  expect_all({
      { "Main St & 1st Ave, Richmond VA", kX, "main st", "1st ave",
        "Richmond VA" },
      { "Main St & 1st Ave, Richmond, VA", kX, "main st", "1st ave",
        "Richmond, VA" },
      { "Main St & 1st Ave, Richmond, VA 23220", kX, "main st", "1st ave",
        "Richmond, VA 23220" },
      { "Main St & 1st Ave, Richmond va.", kX, "main st", "1st ave",
        "Richmond va." },
      { "Main St & 1st Ave, VA", kX, "main st", "1st ave", "VA" },
      { "Main St and 1st Ave, Glen Allen, VA", kX, "main st", "1st ave",
        "Glen Allen, VA" },
      { "Main St, Richmond VA", kOne, "main st", "", "Richmond VA" },
      { "Main St & 1st Ave, Richmond", kX, "main st", "1st ave richmond",
        "" }, // no state code: not a locality
      { "Main St & 1st Ave, Richmond, Virginia", kX, "main st",
        "1st ave richmond virginia",
        "" }, // spelled-out states are not recognized
      { "Main St & Oak St, NW", kX, "main st", "oak st nw",
        "" }, // NW is a directional, not a state code
      { "Main St & Oak St, Richmond VA & Elm St", kX, "main st",
        "oak st richmond va", "" }, // only a trailing locality is cut
  });
}

} // namespace
} // namespace chiasmos::norm
