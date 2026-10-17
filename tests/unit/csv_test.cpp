#include <gtest/gtest.h>

#include <sstream>

#include "output/csv.hpp"

namespace chiasmos::output {
namespace {

using Record = std::vector<std::string>;

TEST(Csv, ReadsQuotedFieldsAndLineEndings) {
  std::istringstream in(
      "a,\"b,c\",\"say \"\"hi\"\"\"\r\nx,,\"two\nlines\"\n\nlast,1");
  EXPECT_EQ(read_csv_record(in), (Record{ "a", "b,c", "say \"hi\"" }));
  EXPECT_EQ(read_csv_record(in), (Record{ "x", "", "two\nlines" }));
  EXPECT_EQ(read_csv_record(in),
            (Record{ "" })); // a blank line is one empty field
  EXPECT_EQ(read_csv_record(in), (Record{ "last", "1" }));
  EXPECT_FALSE(read_csv_record(in).has_value());
}

TEST(Csv, DropsUtf8Bom) {
  std::istringstream in("\xEF\xBB\xBF"
                        "CAD Number,Block/Intersection\n1,Main St\n");
  EXPECT_EQ(read_csv_record(in),
            (Record{ "CAD Number", "Block/Intersection" }));
  EXPECT_EQ(read_csv_record(in), (Record{ "1", "Main St" }));
}

TEST(Csv, WriteQuotesOnlyWhenNeededAndRoundTrips) {
  Record const fields{ "plain", "a,b", "q\"uote", "line\nbreak", "" };
  std::ostringstream out;
  write_csv_record(out, fields);
  EXPECT_EQ(out.str(), "plain,\"a,b\",\"q\"\"uote\",\"line\nbreak\",\n");
  std::istringstream in(out.str());
  EXPECT_EQ(read_csv_record(in), fields);
}

} // namespace
} // namespace chiasmos::output
