#include <array>
#include <filesystem>
#include <print>

#include "cli/commands.hpp"
#include "index/index.hpp"

namespace chiasmos::cli {

int run_inspect(InspectOptions const& o) {
  auto const ix = index::Index::open(o.index);
  if (!ix)
    return std::println(stderr, "{}", ix.error().msg), 1;
  std::println("file        {} ({:.1f} MiB)", o.index.string(),
               static_cast<double>(std::filesystem::file_size(o.index)) /
                   (1 << 20));
  std::println("region      {}", ix->region());
  std::println("data        {}", ix->data_timestamp());
  std::println("versions    format {}, normalizer {}",
               ix->header().format_version, ix->header().normalizer_version);
  std::println("names       {} full names, {} cores", ix->fulls().size(),
               ix->cores().size());
  std::println("junctions   {}", ix->groups().size());
  std::array<std::size_t, 5> by_kind{};
  for (auto const& g : ix->groups())
    ++by_kind[std::min<std::size_t>(g.kind, by_kind.size() - 1)];
  for (std::size_t k = 0; k < by_kind.size(); ++k)
    if (by_kind[k] != 0)
      std::println("  {:<16}{}", to_string(static_cast<JunctionKind>(k)),
                   by_kind[k]);
  if (o.verify) {
    auto const ok = ix->validate();
    if (!ok)
      return std::println(stderr, "verify FAILED: {}", ok.error().msg), 1;
    std::println("verify      ok");
  }
  return 0;
}

} // namespace chiasmos::cli
