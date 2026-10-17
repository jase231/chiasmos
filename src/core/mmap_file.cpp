#include "core/mmap_file.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace chiasmos {

MappedFile& MappedFile::operator=(MappedFile o) noexcept {
  std::swap(data_, o.data_);
  std::swap(size_, o.size_);
  return *this;
}

MappedFile::~MappedFile() {
  if (data_ != nullptr)
    ::munmap(data_, size_);
}

Result<MappedFile> MappedFile::open(std::filesystem::path const& path) {
  int const fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return fail(Errc::Io, path.string() + ": " + std::strerror(errno));

  struct stat st{};
  if (::fstat(fd, &st) != 0 || st.st_size <= 0) {
    ::close(fd);
    return fail(Errc::Io, path.string() + ": empty or unreadable");
  }

  void* p = ::mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ,
                   MAP_PRIVATE, fd, 0);
  ::close(fd); // closes FD but mapping remains until dtor's munmap
  if (p == MAP_FAILED)
    return fail(Errc::Io, path.string() + ": mmap: " + std::strerror(errno));

  MappedFile f{};
  f.data_ = p;
  f.size_ = static_cast<std::size_t>(st.st_size);
  return f;
}

} // namespace chiasmos
