#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>

#include "index/format.hpp"

namespace chiasmos::index {

class Index {
public:
  // checks magic, versions, and file size
  [[nodiscard]] static Result<Index> open(std::filesystem::path const& path);
  // full validation scan used sed by `inspect --verify`
  [[nodiscard]] Result<void> validate() const;

  [[nodiscard]] std::span<FullRec const> fulls() const { return fulls_; }
  [[nodiscard]] std::span<CoreRec const> cores() const { return cores_; }
  [[nodiscard]] std::span<GroupRec const> groups() const { return groups_; }

  [[nodiscard]] std::string_view full_str(std::uint32_t id) const {
    return str(fulls_[id].str_off, fulls_[id].str_len);
  }
  [[nodiscard]] std::string_view full_display(std::uint32_t id) const {
    return str(fulls_[id].disp_off, fulls_[id].disp_len);
  }
  [[nodiscard]] std::string_view core_str(std::uint32_t id) const {
    return str(cores_[id].str_off, cores_[id].str_len);
  }

  // binary search over the cores
  [[nodiscard]] std::optional<std::uint32_t>
  find_core(std::string_view core) const;
  // all junctions of two cores
  [[nodiscard]] std::span<GroupRec const>
  pair_groups(std::uint32_t core_a, std::uint32_t core_b) const;

  [[nodiscard]] Header const& header() const { return header_; }
  [[nodiscard]] std::string_view region() const { return text(header_.region); }
  [[nodiscard]] std::string_view data_timestamp() const {
    return text(header_.data_timestamp);
  }
  [[nodiscard]] std::string_view meta_json() const {
    return { reinterpret_cast<char const*>(meta_.data()), meta_.size() };
  }

private:
  [[nodiscard]] std::string_view str(std::uint32_t off,
                                     std::uint16_t len) const {
    return strings_.substr(off, len);
  }
  [[nodiscard]] static std::string_view text(std::array<char, 32> const& a) {
    return { a.data(), std::string_view(a.data(), a.size()).find('\0') };
  }

  MappedFile file_;
  Header header_;
  std::string_view strings_;
  std::span<std::byte const> meta_;
  std::span<FullRec const> fulls_;
  std::span<CoreRec const> cores_;
  std::span<GroupRec const> groups_;
};

} // namespace chiasmos::index
