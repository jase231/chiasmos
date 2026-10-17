#pragma once
// on disk layout of chiasmos index, for mmaped reads only
// everything after header on 64-byte boundries
// Header (192 B) | Strings | Fulls | Cores | Groups | Meta
//
// must change kFormatVersion on any change to this file
// change norm::kNormalizerVersion on change to normalizer output.
#include <array>
#include <bit>
#include <cstdint>

#include "core/mmap_file.hpp"
#include "core/types.hpp"

namespace chiasmos::index {

static_assert(std::endian::native == std::endian::little,
              "the index format is little-endian");

inline constexpr std::array<char, 8> kMagic{ 'C', 'H', 'I', 'A',
                                             'S', 'M', 'O', 'S' };
inline constexpr std::uint32_t kFormatVersion = 1;
inline constexpr std::size_t kSectionAlign = 64;

enum class Section : std::uint32_t {
  Strings,
  Fulls,
  Cores,
  Groups,
  Meta,
  kCount
};
inline constexpr std::size_t kSectionCount =
    static_cast<std::size_t>(Section::kCount);

struct SectionEntry {
  std::uint64_t offset = 0, count = 0;
};

struct Header {
  std::array<char, 8> magic{};
  std::uint32_t format_version = 0, normalizer_version = 0;
  std::uint64_t file_bytes = 0;
  std::array<SectionEntry, kSectionCount> sections{};
  // eg. "us/virginia" NUL padded
  std::array<char, 32> region{};
  // OSM extract timestamp NUL padded
  std::array<char, 32> data_timestamp{};
  std::array<char, 24> reserved{};
};
static_assert(sizeof(Header) == 192 && OnDisk<Header>);

// full street name as it appears on a way
// components used to calc agreement score against query
struct FullRec {
  // canonical string and display string in Strings
  std::uint32_t str_off = 0, disp_off = 0;
  std::uint16_t str_len = 0, disp_len = 0;
  std::uint32_t core = 0;
  // norm::Dir, Dir, TypePos, PostMod bits
  std::uint8_t pre_dir = 0, suf_dir = 0, type_pos = 0, post_mod = 0;
  // norm type id
  std::uint16_t type = 0;
  // norm::RouteNet
  std::uint8_t route = 0, pad = 0;
};
static_assert(sizeof(FullRec) == 24 && OnDisk<FullRec>);

// query key, full name without directionals, type, and modifier
struct CoreRec {
  std::uint32_t str_off = 0;
  std::uint16_t str_len = 0;
  std::uint8_t flags = 0, pad = 0;
};
static_assert(sizeof(CoreRec) == 8 && OnDisk<CoreRec>);
inline constexpr std::uint8_t kCoreIsRoute = 1;

// a junction of two names
// sorted by (core_a, core_b, x, y, full_a, full_b),
// so all junctions of a core pair are stored in one contiguous range
struct GroupRec {
  std::uint32_t core_a = 0, core_b = 0;
  std::uint32_t full_a = 0, full_b = 0;
  std::int32_t x = 0, y = 0;             // representative point
  std::int64_t osm_node = 0;             // the shared node
  std::uint8_t kind = 0;                 // JunctionKind
  std::uint8_t class_a = 0, class_b = 0; // highest RoadClass carrying each
  std::uint8_t name_kind_a = 0, name_kind_b = 0;
  std::uint8_t pad[3] = {};
};
static_assert(sizeof(GroupRec) == 40 && OnDisk<GroupRec>);

} // namespace chiasmos::index
