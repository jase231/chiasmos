#include "build/road_filter.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace chiasmos::build {
namespace {

using sv = std::string_view;

// whether a way of this class should be kept
enum class Keep : std::uint8_t { Always, Named, NamedOrRoundabout, NamedPath };

struct Rule {
  sv highway;
  RoadClass cls;
  Keep keep;
  bool has_link;
};

constexpr std::array kRules{
  Rule{ "motorway", RoadClass::Motorway, Keep::Always, true },
  Rule{ "trunk", RoadClass::Trunk, Keep::Always, true },
  Rule{ "primary", RoadClass::Primary, Keep::Always, true },
  Rule{ "secondary", RoadClass::Secondary, Keep::Always, true },
  Rule{ "tertiary", RoadClass::Tertiary, Keep::Always, true },
  Rule{ "unclassified", RoadClass::Unclassified, Keep::Always, false },
  Rule{ "residential", RoadClass::Residential, Keep::Always, false },
  Rule{ "living_street", RoadClass::LivingStreet, Keep::Always, false },
  Rule{ "road", RoadClass::Road, Keep::Always, false },
  Rule{ "busway", RoadClass::Busway, Keep::Always, false },
  Rule{ "service", RoadClass::Service, Keep::NamedOrRoundabout, false },
  Rule{ "track", RoadClass::Track, Keep::Named, false },
  Rule{ "pedestrian", RoadClass::Pedestrian, Keep::Named, false },
  Rule{ "footway", RoadClass::Path, Keep::NamedPath, false },
  Rule{ "cycleway", RoadClass::Path, Keep::NamedPath, false },
  Rule{ "path", RoadClass::Path, Keep::NamedPath, false },
  Rule{ "bridleway", RoadClass::Path, Keep::NamedPath, false },
  Rule{ "steps", RoadClass::Path, Keep::NamedPath, false },
};

sv value(osmium::TagList const& tags, char const* key) {
  char const* v = tags.get_value_by_key(key);
  return v == nullptr ? sv{} : sv{ v };
}

} // namespace

std::optional<RoadAttrs> classify_road(osmium::TagList const& tags,
                                       bool include_paths) {
  sv highway = value(tags, "highway");
  if (highway.empty() || value(tags, "area") == "yes")
    return std::nullopt;
  bool const link = highway.ends_with("_link");
  if (link)
    highway.remove_suffix(5);
  auto const rule = std::ranges::find(kRules, highway, &Rule::highway);
  if (rule == kRules.end() || (link && !rule->has_link))
    return std::nullopt;

  sv const junction = value(tags, "junction");
  bool const roundabout = junction == "roundabout" || junction == "circular";
  bool const named = !value(tags, "name").empty();
  switch (rule->keep) {
  case Keep::Always:
    break;
  case Keep::Named:
    if (!named)
      return std::nullopt;
    break;
  case Keep::NamedOrRoundabout:
    if (!named && !roundabout)
      return std::nullopt;
    break;
  case Keep::NamedPath: {
    sv const footway = value(
        tags,
        "footway"); // sidewalks and crossings are map furniture, not streets
    if (!include_paths || !named || footway == "sidewalk" ||
        footway == "crossing")
      return std::nullopt;
    break;
  }
  }
  return RoadAttrs{ rule->cls, roundabout };
}

} // end namespace chiasmos::build
