#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string_view>

namespace chiasmos {

inline constexpr std::uint32_t kNone32 = ~std::uint32_t{ 0 };

// coordinates use OSM's 1e-7 degree integer format
struct Coord {
  std::int32_t x = 0, y = 0; // lon, lat
  friend bool operator==(Coord const&, Coord const&) = default;
};
[[nodiscard]] inline Coord to_coord(double lon, double lat) {
  return { static_cast<std::int32_t>(std::llround(lon * 1e7)),
           static_cast<std::int32_t>(std::llround(lat * 1e7)) };
}
[[nodiscard]] constexpr double lon_deg(Coord c) { return c.x * 1e-7; }
[[nodiscard]] constexpr double lat_deg(Coord c) { return c.y * 1e-7; }

struct Bbox {
  Coord lo{}, hi{};
  [[nodiscard]] bool contains(Coord c) const {
    return c.x >= lo.x && c.x <= hi.x && c.y >= lo.y && c.y <= hi.y;
  }
};

// great-circle distance in metres
[[nodiscard]] inline double dist_m(Coord a, Coord b) {
  constexpr double kEarthRadiusM = 6371008.8, kRad = std::numbers::pi / 180.0;
  double const la = lat_deg(a) * kRad, lb = lat_deg(b) * kRad;
  double const s1 = std::sin((lb - la) / 2),
               s2 = std::sin((lon_deg(b) - lon_deg(a)) * kRad / 2);
  return 2 * kEarthRadiusM *
         std::asin(std::sqrt(s1 * s1 + std::cos(la) * std::cos(lb) * s2 * s2));
}

// importance of road classification defined in decreasing order
enum class RoadClass : std::uint8_t {
  Motorway,
  Trunk,
  Primary,
  Secondary,
  Tertiary,
  Unclassified,
  Residential,
  LivingStreet,
  Road,
  Busway,
  Service,
  Track,
  Pedestrian,
  Path
};

// how a way carries its name
enum class NameKind : std::uint8_t { Primary, Alt, Old };

enum class JunctionKind : std::uint8_t {
  Node,           // one shared OSM node
  Roundabout,     // a roundabout ring, treated as a junction
  NameTransition, // one road that changes name
  Concurrency,    // two names that refer to the same physical road
  Grouped // at query time, several junctions in close proximity are merged into
          // one and labeled Grouped
};

enum class Status : std::uint8_t {
  Ok,
  Ambiguous,
  NoStreetMatch,
  NoIntersection,
  NotAnIntersection,
  InvalidRequest
};

// helpers for converting from enum classes to string reprs
template <class E, std::size_t N>
[[nodiscard]] inline std::string_view
enum_name(std::array<std::string_view, N> const& names, E e) {
  return names[static_cast<std::size_t>(e)];
}
[[nodiscard]] inline std::string_view to_string(JunctionKind k) {
  return enum_name(std::array<std::string_view, 5>{ "node", "roundabout",
                                                    "name_transition",
                                                    "concurrency", "grouped" },
                   k);
}
[[nodiscard]] inline std::string_view to_string(Status s) {
  return enum_name(
      std::array<std::string_view, 6>{ "ok", "ambiguous", "no_street_match",
                                       "no_intersection", "not_an_intersection",
                                       "invalid_request" },
      s);
}

} // namespace chiasmos
