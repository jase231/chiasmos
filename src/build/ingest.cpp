#include "build/ingest.hpp"

#include <algorithm>
#include <array>
#include <ranges>
#include <string_view>
#include <unordered_map>

#include <osmium/handler.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/index.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/io/reader_iterator.hpp>
#include <osmium/osm/way.hpp>
#include <osmium/visitor.hpp>

namespace chiasmos::build {
namespace {

using sv = std::string_view;

struct NameTag {
  sv key;
  NameKind kind;
};
constexpr std::array kNameTags{
  NameTag{ "name", NameKind::Primary },
  NameTag{ "ref", NameKind::Primary },
  NameTag{ "unsigned_ref", NameKind::Primary },
  NameTag{ "alt_name", NameKind::Alt },
  NameTag{ "alt_name_1", NameKind::Alt },
  NameTag{ "name_1", NameKind::Alt },
  NameTag{ "official_name", NameKind::Alt },
  NameTag{ "short_name", NameKind::Alt },
  NameTag{ "loc_name", NameKind::Alt },
  NameTag{ "reg_name", NameKind::Alt },
  NameTag{ "nat_name", NameKind::Alt },
  NameTag{ "old_name", NameKind::Old },
  NameTag{ "old_ref", NameKind::Old },
};
constexpr std::size_t kMaxNameBytes = 60;

sv trim(sv s) {
  auto const first = s.find_first_not_of(' ');
  return first == sv::npos
             ? sv{}
             : s.substr(first, s.find_last_not_of(' ') - first + 1);
}

class RoadCollector : public osmium::handler::Handler {
public:
  RoadCollector(RoadWays& out, bool include_paths)
      : out_(out), include_paths_(include_paths) {
    out_.node_off.push_back(0);
    out_.name_off.push_back(0);
  }

  // write every node of a given way into the output
  void way(osmium::Way const& way) {
    auto const attrs = classify_road(way.tags(), include_paths_);
    if (!attrs || way.nodes().size() < 2)
      return; // skip non-road ways
    out_.osm_id.push_back(way.id());
    out_.attrs.push_back(*attrs);
    for (osmium::NodeRef const& ref : way.nodes())
      out_.nodes.push_back(
          { ref.ref(), { ref.location().x(), ref.location().y() } });
    out_.node_off.push_back(out_.nodes.size());
    for (osmium::Tag const& tag : way.tags())
      collect_names(tag);
    out_.name_off.push_back(out_.names.size());
  }

private:
  void collect_names(osmium::Tag const& tag) {
    auto const name_tag =
        std::ranges::find(kNameTags, sv{ tag.key() }, &NameTag::key);
    if (name_tag == kNameTags.end())
      return;
    for (auto const piece : sv{ tag.value() } | std::views::split(';')) {
      sv const part = trim(sv{ piece });
      if (!part.empty() && part.size() <= kMaxNameBytes && !part.contains(','))
        out_.names.push_back({ intern(part), name_tag->kind });
    }
  }

  std::uint32_t intern(sv s) {
    if (auto const it = ids_.find(s); it != ids_.end())
      return it->second;
    auto const id = static_cast<std::uint32_t>(out_.strings.size());
    ids_.emplace(sv{ out_.strings.emplace_back(s) }, id);
    return id;
  }

  RoadWays& out_;
  bool include_paths_;
  std::unordered_map<sv, std::uint32_t> ids_;
};

} // namespace

Result<Ingested> ingest_roads(std::filesystem::path const& pbf,
                              Options const& opt) {
  try {
    osmium::io::Reader reader{ pbf.string(),
                               // skip non-node, non-way entities
                               osmium::osm_entity_bits::node |
                                   osmium::osm_entity_bits::way,
                               osmium::io::read_meta::no };
    Ingested result;
    osmium::io::Header const header = reader.header();
    result.data_timestamp = header.get("osmosis_replication_timestamp");
    if (result.data_timestamp.empty())
      result.data_timestamp = header.get("timestamp");

    // cache every node in the extract
    osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type,
                                       osmium::Location>
        locations;
    osmium::handler::NodeLocationsForWays<decltype(locations)> add_locations{
      locations
    };

    RoadCollector collector{ result.ways, opt.include_paths };
    // visit every entity produced by reader and calls the handlers on entity
    osmium::apply(reader, add_locations, collector);
    reader.close();
    return result;
  } catch (osmium::not_found const& e) {
    return fail(Errc::Corrupt,
                pbf.string() + ": " + e.what() +
                    " (the PBF must hold every node its ways use, listed "
                    "before the ways. Use Geofabrik extracts.)");
  } catch (std::exception const& e) {
    return fail(Errc::Io, pbf.string() + ": " + e.what());
  }
}

} // end namespace chiasmos::build
