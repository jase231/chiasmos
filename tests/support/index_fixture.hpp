#pragma once
// Test support: builds a small index file from a compact description (two
// streets, a point, a junction kind) and opens it. Streets are hand-built (full
// name, core, components), so tests do not depend on the normalizer;
// `raw_street` goes through norm::normalize_street for the few integration
// tests that want the real thing.
#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <numbers>
#include <optional>
#include <string>

#include "index/draft.hpp"
#include "index/index.hpp"
#include "normalize/normalizer.hpp"

namespace chiasmos::test {

// Type ids of hand-built names. The engine only compares them for equality, so
// any distinct non-zero values do.
inline constexpr std::uint16_t kSt = 1, kRd = 2, kAve = 3;

// One street as one way carries it at a junction.
struct Street {
  std::string full, core;
  norm::Components comp;
  std::string display;
  NameKind kind = NameKind::Primary;
  RoadClass road = RoadClass::Residential;
};

// Street "Main" in its several forms, as the normalizer would produce them for
// the same words.
inline Street main_st() {
  return { "main st",
           "main",
           { .type = kSt, .type_pos = norm::TypePos::Suffix },
           "Main Street" };
}
inline Street oak_ave() {
  return { "oak ave",
           "oak",
           { .type = kAve, .type_pos = norm::TypePos::Suffix },
           "Oak Avenue" };
}

inline Street raw_street(std::string_view display,
                         RoadClass road = RoadClass::Residential) {
  norm::Normalized n;
  norm::normalize_street(display, n);
  return {
    n.full, n.core, n.comp, std::string(display), NameKind::Primary, road
  };
}

// What the engine would get after normalizing the query text (hand-built, like
// the records).
inline norm::Normalized query_side(Street const& s) {
  return { s.full, s.core, s.comp };
}

// The point `north_m` metres north and `east_m` metres east of `c`.
inline Coord offset(Coord c, double north_m, double east_m = 0) {
  constexpr double kMetresPerDegree = 6371008.8 * std::numbers::pi / 180;
  double const lat = lat_deg(c) + north_m / kMetresPerDegree;
  return to_coord(
      lon_deg(c) +
          east_m / (kMetresPerDegree * std::cos(lat * std::numbers::pi / 180)),
      lat);
}

// The index lives in a temp file named after the running test, removed when the
// fixture goes out of scope.
class TestIndex {
public:
  TestIndex() {
    auto const* t = ::testing::UnitTest::GetInstance()->current_test_info();
    path_ = std::filesystem::temp_directory_path() /
            (std::string("chiasmos_") + t->test_suite_name() + "_" + t->name() +
             ".cix");
  }
  ~TestIndex() {
    ix_.reset();
    std::filesystem::remove(path_);
  }
  TestIndex(TestIndex const&) = delete;
  TestIndex& operator=(TestIndex const&) = delete;

  // `a` and `b` meet at `at`. Road classes and name kinds travel with the
  // streets.
  void add(Street const& a, Street const& b, Coord at, std::int64_t node = 0,
           JunctionKind kind = JunctionKind::Node) {
    draft_.add_junction({ .a = intern(a),
                          .b = intern(b),
                          .rep = at,
                          .osm_node = node,
                          .kind = kind,
                          .class_a = a.road,
                          .class_b = b.road,
                          .name_kind_a = a.kind,
                          .name_kind_b = b.kind });
  }

  // Writes the file on first use.
  index::Index const& open() {
    if (!ix_) {
      EXPECT_TRUE(draft_.write(path_, { .region = "test/region", .json = "{}" })
                      .has_value());
      auto opened = index::Index::open(path_);
      EXPECT_TRUE(opened.has_value());
      ix_.emplace(std::move(*opened));
      EXPECT_TRUE(ix_->validate().has_value());
    }
    return *ix_;
  }

private:
  index::Draft::FullId intern(Street const& s) {
    return draft_.add_full({ s.full, s.core, s.comp }, s.display);
  }

  index::Draft draft_;
  std::filesystem::path path_;
  std::optional<index::Index> ix_;
};

} // namespace chiasmos::test
