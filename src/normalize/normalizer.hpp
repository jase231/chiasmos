#pragma once
// used by both query and index build
// increment kNormalizerVersion on update
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace chiasmos::norm {

inline constexpr std::uint32_t kNormalizerVersion = 1;

enum class Dir : std::uint8_t { None, N, S, E, W, NE, NW, SE, SW };
enum class TypePos : std::uint8_t { None, Prefix, Suffix };
// numbered route networks
enum class RouteNet : std::uint8_t { None, Interstate, US, State, County, Hwy };
enum PostMod : std::uint8_t {
  EXT = 1,
  BUS = 2,
  BYP = 4,
  ALT = 8,
  CONN = 16,
  TRUCK = 32,
  SPUR = 64,
  EXPR = 128
};

struct Components {
  // routes' suffix is travel dir
  Dir pre_dir{}, suf_dir{};
  std::uint16_t type = 0;
  TypePos type_pos{};
  std::uint8_t post_mod = 0;
  RouteNet route{};
  friend bool operator==(Components const&, Components const&) = default;
};

struct Normalized {
  // canonical full name
  std::string full;
  // full name minus directionals, street type and modifier
  std::string core;
  Components comp;

  [[nodiscard]] bool is_route() const { return comp.route != RouteNet::None; }
  [[nodiscard]] bool empty() const { return full.empty(); }
  void clear() { full.clear(), core.clear(), comp = {}; }
};

// out is cleared and can be reused
void normalize_street(std::string_view raw, Normalized& out);

[[nodiscard]] std::string_view to_string(Dir d);
[[nodiscard]] std::string_view type_name(std::uint16_t type);

// candidate core types for queries
// priors: Exact 1.0, RouteNetwork 0.9, BareNumber 0.8, TrimmedSuffix 0.85
enum class AltKind : std::uint8_t {
  Exact,
  RouteNetwork,
  BareNumber,
  TrimmedSuffix
};
struct CoreAlt {
  std::string core;
  float prior = 1.0f;
  AltKind kind = AltKind::Exact;
  std::uint16_t type = 0;
};
void query_core_alternatives(Normalized const& n, std::vector<CoreAlt>& out);

// free-form intersection query, eg. "Main St & Broad St" split in two
// valid seperators: "&", "@", "/", "--", "xst"
// any trailing text, like "City, State" is stored as locality (currently
// unused)
enum class Form : std::uint8_t { Empty, Intersection, SingleStreet };
struct ParsedQuery {
  Form form = Form::Empty;
  std::string street1, street2, locality;
};
[[nodiscard]] ParsedQuery parse_freeform(std::string_view q);

} // namespace chiasmos::norm
