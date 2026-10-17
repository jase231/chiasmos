#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "core/error.hpp"

namespace chiasmos {

// restricts records that sit in the file, mainly to let us access mmaped bytes
// as Ts without UB or padding
template <class T>
concept OnDisk =
    std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T> &&
    std::has_unique_object_representations_v<T>;

class MappedFile {
public:
  MappedFile() = default;
  MappedFile(MappedFile&& o) noexcept
      : data_(std::exchange(o.data_, nullptr)),
        size_(std::exchange(o.size_, 0)) {}
  MappedFile(MappedFile const&) = delete;
  MappedFile& operator=(MappedFile o) noexcept;
  ~MappedFile();

  [[nodiscard]] static Result<MappedFile>
  open(std::filesystem::path const& path);
  [[nodiscard]] std::span<std::byte const> bytes() const {
    return { static_cast<std::byte const*>(data_), size_ };
  }

private:
  void* data_ = nullptr;
  std::size_t size_ = 0;
};

// count records of T starting at offset or error if out of bounds or is
// misaligned for T
template <OnDisk T>
[[nodiscard]] Result<std::span<T const>>
view_array(std::span<std::byte const> file, std::uint64_t offset,
           std::uint64_t count) {
  if (offset > file.size() || count > (file.size() - offset) / sizeof(T))
    return fail(Errc::Corrupt, "section outside the file");
  if (offset % alignof(T) != 0)
    return fail(Errc::Corrupt, "misaligned section");
  if (count == 0)
    return std::span<T const>{};

  return std::span<T const>(
      std::start_lifetime_as_array<T>(file.data() + offset,
                                      static_cast<std::size_t>(count)),
      static_cast<std::size_t>(count));
}

} // namespace chiasmos
