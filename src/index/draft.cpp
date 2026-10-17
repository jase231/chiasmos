#include "index/draft.hpp"

#include <algorithm>
#include <fstream>
#include <numeric>
#include <tuple>

#include "index/format.hpp"

namespace chiasmos::index {

Draft::FullId Draft::add_full(norm::Normalized const& n,
                              std::string_view display) {
  auto const [it, inserted] =
      ids_.try_emplace(n.full, static_cast<FullId>(fulls_.size()));
  if (inserted)
    fulls_.push_back(
        { n.full, n.core,
          std::string(display.empty() ? std::string_view(n.full) : display),
          n.comp });
  return it->second;
}

namespace {

template <OnDisk T> void put(std::ofstream& f, std::span<T const> v) {
  f.write(reinterpret_cast<char const*>(v.data()),
          static_cast<std::streamsize>(v.size_bytes()));
}

class Blob {
public:
  bool add(std::string_view s, std::uint32_t& off, std::uint16_t& len) {
    if (s.size() > 0xFFFF || data_.size() + s.size() > 0xFFFFFFFFu)
      return false;
    off = static_cast<std::uint32_t>(data_.size());
    len = static_cast<std::uint16_t>(s.size());
    data_ += s;
    return true;
  }
  [[nodiscard]] std::string const& data() const { return data_; }

private:
  std::string data_;
};

} // namespace

Result<void> Draft::write(std::filesystem::path const& out,
                          Meta const& meta) const {
  if (meta.region.size() >= sizeof(Header::region) ||
      meta.data_timestamp.size() >= sizeof(Header::data_timestamp))
    return fail(Errc::InvalidArgument,
                "region or data_timestamp longer than 31 bytes");

  // distinct sorted core strings
  std::vector<std::string_view> core_strs;
  core_strs.reserve(fulls_.size());
  for (Entry const& e : fulls_)
    core_strs.push_back(e.core);
  std::ranges::sort(core_strs);
  core_strs.erase(std::ranges::unique(core_strs).begin(), core_strs.end());
  auto const core_of = [&](Entry const& e) {
    return static_cast<std::uint32_t>(
        std::ranges::lower_bound(core_strs, std::string_view(e.core)) -
        core_strs.begin());
  };

  // unique full names ordered by (core id, full string)
  std::vector<std::uint32_t> core_id(fulls_.size()), order(fulls_.size()),
      new_id(fulls_.size());
  for (std::size_t i = 0; i < fulls_.size(); ++i)
    core_id[i] = core_of(fulls_[i]);
  std::iota(order.begin(), order.end(), 0u);
  std::ranges::sort(order, [&](std::uint32_t a, std::uint32_t b) {
    return std::tie(core_id[a], fulls_[a].full) <
           std::tie(core_id[b], fulls_[b].full);
  });
  for (std::size_t r = 0; r < order.size(); ++r)
    new_id[order[r]] = static_cast<std::uint32_t>(r);

  Blob blob;
  std::vector<CoreRec> cores(core_strs.size());
  for (std::size_t i = 0; i < cores.size(); ++i) {
    if (!blob.add(core_strs[i], cores[i].str_off, cores[i].str_len))
      return fail(Errc::InvalidArgument,
                  "core name too long: " + std::string(core_strs[i]));
    cores[i].flags = core_strs[i].starts_with('#') ? kCoreIsRoute : 0;
  }
  std::vector<FullRec> fulls(fulls_.size());
  for (std::size_t r = 0; r < order.size(); ++r) {
    Entry const& e = fulls_[order[r]];
    FullRec& f = fulls[r];
    if (!blob.add(e.full, f.str_off, f.str_len) ||
        !blob.add(e.display, f.disp_off, f.disp_len))
      return fail(Errc::InvalidArgument, "name too long: " + e.full);
    f.core = core_id[order[r]];
    f.pre_dir = static_cast<std::uint8_t>(e.comp.pre_dir);
    f.suf_dir = static_cast<std::uint8_t>(e.comp.suf_dir);
    f.type_pos = static_cast<std::uint8_t>(e.comp.type_pos);
    f.post_mod = e.comp.post_mod;
    f.type = e.comp.type;
    f.route = static_cast<std::uint8_t>(e.comp.route);
  }

  // junctions are written to GroupRecs with fields strictly ordered
  std::vector<GroupRec> groups;
  groups.reserve(junctions_.size());
  for (Junction const& j : junctions_) {
    GroupRec g;
    auto const side_a = std::tuple(fulls[new_id[j.a]].core, new_id[j.a]),
               side_b = std::tuple(fulls[new_id[j.b]].core, new_id[j.b]);
    bool const swap = side_b < side_a;
    FullId const fa = swap ? j.b : j.a, fb = swap ? j.a : j.b;
    g.full_a = new_id[fa], g.full_b = new_id[fb];
    g.core_a = fulls[g.full_a].core, g.core_b = fulls[g.full_b].core;
    g.x = j.rep.x, g.y = j.rep.y;
    g.osm_node = j.osm_node;
    g.kind = static_cast<std::uint8_t>(j.kind);
    g.class_a = static_cast<std::uint8_t>(swap ? j.class_b : j.class_a),
    g.class_b = static_cast<std::uint8_t>(swap ? j.class_a : j.class_b);
    g.name_kind_a =
        static_cast<std::uint8_t>(swap ? j.name_kind_b : j.name_kind_a),
    g.name_kind_b =
        static_cast<std::uint8_t>(swap ? j.name_kind_a : j.name_kind_b);
    groups.push_back(g);
  }
  std::ranges::sort(groups, [](GroupRec const& l, GroupRec const& r) {
    return std::tie(l.core_a, l.core_b, l.x, l.y, l.full_a, l.full_b,
                    l.osm_node, l.kind) < std::tie(r.core_a, r.core_b, r.x, r.y,
                                                   r.full_a, r.full_b,
                                                   r.osm_node, r.kind);
  });

  // index layout
  // header then everything else on 64-bte boundries
  Header h;
  h.magic = kMagic;
  h.format_version = kFormatVersion;
  h.normalizer_version = norm::kNormalizerVersion;
  std::ranges::copy(meta.region, h.region.begin());
  std::ranges::copy(meta.data_timestamp, h.data_timestamp.begin());
  std::uint64_t cursor = sizeof(Header);
  auto const place = [&](Section id, std::uint64_t count,
                         std::uint64_t elem_size) {
    cursor = (cursor + kSectionAlign - 1) / kSectionAlign * kSectionAlign;
    h.sections[static_cast<std::size_t>(id)] = { cursor, count };
    cursor += count * elem_size;
  };
  place(Section::Strings, blob.data().size(), 1);
  place(Section::Fulls, fulls.size(), sizeof(FullRec));
  place(Section::Cores, cores.size(), sizeof(CoreRec));
  place(Section::Groups, groups.size(), sizeof(GroupRec));
  place(Section::Meta, meta.json.size(), 1);
  h.file_bytes = cursor;

  auto tmp = out;
  tmp += ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f)
      return fail(Errc::Io, tmp.string() + ": cannot open for writing");
    auto const pad_to = [&](std::uint64_t offset) {
      static constexpr char kZeros[kSectionAlign] = {};
      for (auto here = static_cast<std::uint64_t>(f.tellp()); here < offset;
           here = static_cast<std::uint64_t>(f.tellp()))
        f.write(kZeros, static_cast<std::streamsize>(std::min<std::uint64_t>(
                            offset - here, sizeof kZeros)));
    };
    auto const at = [&](Section id) {
      return h.sections[static_cast<std::size_t>(id)].offset;
    };
    put<Header>(f, { &h, 1 });
    pad_to(at(Section::Strings)),
        f.write(blob.data().data(),
                static_cast<std::streamsize>(blob.data().size()));
    pad_to(at(Section::Fulls)), put<FullRec>(f, fulls);
    pad_to(at(Section::Cores)), put<CoreRec>(f, cores);
    pad_to(at(Section::Groups)), put<GroupRec>(f, groups);
    pad_to(at(Section::Meta)),
        f.write(meta.json.data(),
                static_cast<std::streamsize>(meta.json.size()));
    f.flush();
    if (!f)
      return fail(Errc::Io, tmp.string() + ": write failed");
  }
  std::error_code ec;
  std::filesystem::rename(tmp, out, ec);
  if (ec)
    return fail(Errc::Io, out.string() + ": " + ec.message());
  return {};
}

} // namespace chiasmos::index
