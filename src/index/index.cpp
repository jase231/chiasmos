#include "index/index.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <ranges>

#include "normalize/normalizer.hpp"

namespace chiasmos::index {

Result<Index> Index::open(std::filesystem::path const& path) {
  auto file = MappedFile::open(path);
  if (!file)
    return std::unexpected(file.error());
  auto const bytes = file->bytes();
  if (bytes.size() < sizeof(Header))
    return fail(Errc::Corrupt, path.string() + ": too small to be an index");

  Index ix;
  std::memcpy(&ix.header_, bytes.data(), sizeof(Header));
  Header const& h = ix.header_;
  if (h.magic != kMagic)
    return fail(Errc::Corrupt, path.string() + ": not a chiasmos index");
  if (h.format_version != kFormatVersion)
    return fail(
        Errc::VersionMismatch,
        std::format("{}: format version {} (expected {}): rebuild the index",
                    path.string(), h.format_version, kFormatVersion));
  if (h.normalizer_version != norm::kNormalizerVersion)
    return fail(
        Errc::VersionMismatch,
        std::format(
            "{}: normalizer version {} (expected {}): rebuild the index",
            path.string(), h.normalizer_version, norm::kNormalizerVersion));
  if (h.file_bytes != bytes.size())
    return fail(Errc::Corrupt, path.string() + ": truncated");

  auto const& s = h.sections;
  auto const section = [&](Section id) -> SectionEntry const& {
    return s[static_cast<std::size_t>(id)];
  };
  auto const view = [&]<OnDisk T>(Section id) {
    return view_array<T>(bytes, section(id).offset, section(id).count);
  };

  auto strings = view.operator()<char>(Section::Strings);
  auto meta = view.operator()<std::byte>(Section::Meta);
  auto fulls = view.operator()<FullRec>(Section::Fulls);
  auto cores = view.operator()<CoreRec>(Section::Cores);
  auto groups = view.operator()<GroupRec>(Section::Groups);
  for (Error const* e :
       { strings ? nullptr : &strings.error(), meta ? nullptr : &meta.error(),
         fulls ? nullptr : &fulls.error(), cores ? nullptr : &cores.error(),
         groups ? nullptr : &groups.error() })
    if (e != nullptr)
      return std::unexpected(*e);

  ix.strings_ = std::string_view(strings->data(), strings->size());
  ix.meta_ = *meta;
  ix.fulls_ = *fulls;
  ix.cores_ = *cores;
  ix.groups_ = *groups;
  ix.file_ = std::move(*file);
  return ix;
}

std::optional<std::uint32_t> Index::find_core(std::string_view core) const {
  auto const it =
      std::ranges::lower_bound(cores_, core, {}, [&](CoreRec const& c) {
        return str(c.str_off, c.str_len);
      });
  if (it == cores_.end() || str(it->str_off, it->str_len) != core)
    return std::nullopt;
  return static_cast<std::uint32_t>(it - cores_.begin());
}

std::span<GroupRec const> Index::pair_groups(std::uint32_t core_a,
                                             std::uint32_t core_b) const {
  auto const key = std::minmax(core_a, core_b);
  auto const r =
      std::ranges::equal_range(groups_, key, {}, [](GroupRec const& g) {
        return std::pair(g.core_a, g.core_b);
      });
  return { r.begin(), r.end() };
}

Result<void> Index::validate() const {
  auto const bad = [](std::string what) -> Result<void> {
    return fail(Errc::Corrupt, std::move(what));
  };
  auto const in_strings = [&](std::uint32_t off, std::uint16_t len) {
    return std::uint64_t{ off } + len <= strings_.size();
  };
  for (std::size_t i = 0; i < cores_.size(); ++i) {
    if (!in_strings(cores_[i].str_off, cores_[i].str_len))
      return bad(std::format("core {}: string out of range", i));
    if (i > 0 && !(core_str(static_cast<std::uint32_t>(i - 1)) <
                   core_str(static_cast<std::uint32_t>(i))))
      return bad(std::format("core {}: not strictly sorted", i));
  }
  for (std::size_t i = 0; i < fulls_.size(); ++i) {
    FullRec const& f = fulls_[i];
    if (!in_strings(f.str_off, f.str_len) ||
        !in_strings(f.disp_off, f.disp_len))
      return bad(std::format("full {}: string out of range", i));
    if (f.core >= cores_.size())
      return bad(std::format("full {}: core out of range", i));
  }
  for (std::size_t i = 0; i < groups_.size(); ++i) {
    GroupRec const& g = groups_[i];
    if (g.full_a >= fulls_.size() || g.full_b >= fulls_.size())
      return bad(std::format("group {}: full out of range", i));
    if (fulls_[g.full_a].core != g.core_a ||
        fulls_[g.full_b].core != g.core_b || g.core_a > g.core_b)
      return bad(std::format("group {}: cores inconsistent with fulls", i));
    if (i > 0 && std::pair(groups_[i - 1].core_a, groups_[i - 1].core_b) >
                     std::pair(g.core_a, g.core_b))
      return bad(std::format("group {}: not sorted", i));
  }
  return {};
}

} // namespace chiasmos::index
