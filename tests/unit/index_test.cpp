#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "index/draft.hpp"
#include "index/index.hpp"

namespace chiasmos {
namespace {

using index::Draft;

norm::Normalized name(std::string full, std::string core) {
  norm::Normalized n;
  n.full = std::move(full);
  n.core = std::move(core);
  return n;
}

std::filesystem::path temp_index(char const* stem) {
  return std::filesystem::temp_directory_path() /
         (std::string("chiasmos_") + stem + ".cix");
}

TEST(Index, RoundTripAndLookups) {
  Draft d;
  auto const main_st = d.add_full(name("main st", "main"), "Main Street");
  auto const n_main =
      d.add_full(name("n main st", "main"), "North Main Street");
  auto const oak = d.add_full(name("oak ave", "oak"), "Oak Avenue");
  EXPECT_EQ(d.add_full(name("oak ave", "oak"), "ignored"),
            oak); // interning is by canonical string

  // Added with the "larger" name first and out of order: the writer must
  // canonicalize and sort.
  d.add_junction({ .a = oak,
                   .b = main_st,
                   .rep = { 20, 20 },
                   .osm_node = 7,
                   .class_a = RoadClass::Primary });
  d.add_junction({ .a = main_st,
                   .b = oak,
                   .rep = { 10, 10 },
                   .osm_node = 5,
                   .kind = JunctionKind::Roundabout });
  d.add_junction({ .a = main_st, .b = n_main, .rep = { 1, 1 }, .osm_node = 9 });

  auto const path = temp_index("roundtrip");
  ASSERT_TRUE(d.write(path, { .region = "test/region",
                              .data_timestamp = "2026-01-01T00:00:00Z",
                              .json = "{}" })
                  .has_value());
  auto ix = index::Index::open(path);
  ASSERT_TRUE(ix.has_value()) << ix.error().msg;
  ASSERT_TRUE(ix->validate().has_value());

  EXPECT_EQ(ix->region(), "test/region");
  EXPECT_EQ(ix->meta_json(), "{}");
  ASSERT_EQ(ix->cores().size(), 2u);
  EXPECT_EQ(ix->core_str(0), "main"); // cores are sorted by string
  EXPECT_EQ(ix->core_str(1), "oak");

  auto const main_core = ix->find_core("main"), oak_core = ix->find_core("oak");
  ASSERT_TRUE(main_core && oak_core);
  EXPECT_FALSE(ix->find_core("elm"));
  EXPECT_FALSE(ix->find_core("ma"));

  auto const pair =
      ix->pair_groups(*oak_core, *main_core); // argument order does not matter
  ASSERT_EQ(pair.size(), 2u);
  EXPECT_EQ(pair[0].osm_node,
            9 - 4); // sorted by (x, y): (10,10) before (20,20)
  EXPECT_EQ(static_cast<JunctionKind>(pair[0].kind), JunctionKind::Roundabout);
  EXPECT_EQ(ix->full_display(pair[0].full_a), "Main Street");
  EXPECT_EQ(ix->full_display(pair[0].full_b), "Oak Avenue");
  // class_a belongs to the side named a: the record for (oak, main) swapped
  // sides, so Primary moved to class_b.
  EXPECT_EQ(static_cast<RoadClass>(pair[1].class_b), RoadClass::Primary);

  auto const same_core =
      ix->pair_groups(*main_core, *main_core); // "main st" meets "n main st"
  ASSERT_EQ(same_core.size(), 1u);
  EXPECT_EQ(ix->full_str(same_core[0].full_a), "main st");
  EXPECT_EQ(ix->full_str(same_core[0].full_b), "n main st");
  std::filesystem::remove(path);
}

TEST(Index, RejectsGarbageAndTruncation) {
  auto const path = temp_index("garbage");
  {
    std::ofstream f(path, std::ios::binary);
    f << "this is not an index file, but it is long enough to hold a header "
         "........................................"
         "....................................................................."
         "...................................";
  }
  EXPECT_FALSE(index::Index::open(path).has_value());

  Draft d;
  d.add_full(name("a st", "a"), "A Street");
  ASSERT_TRUE(d.write(path, { .region = "r" }).has_value());
  std::filesystem::resize_file(path, std::filesystem::file_size(path) - 1);
  auto const truncated = index::Index::open(path);
  ASSERT_FALSE(truncated.has_value());
  EXPECT_EQ(truncated.error().code, Errc::Corrupt);
  std::filesystem::remove(path);
}

} // namespace
} // namespace chiasmos
