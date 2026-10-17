#pragma once

#include <expected>
#include <string>
#include <utility>

namespace chiasmos {

enum class Errc { InvalidArgument, Io, Corrupt, VersionMismatch, Internal };

struct Error {
  Errc code = Errc::Internal;
  std::string msg;
};

template <class T> using Result = std::expected<T, Error>;

[[nodiscard]] inline std::unexpected<Error> fail(Errc code, std::string msg) {
  return std::unexpected(Error{ code, std::move(msg) });
}

} // namespace chiasmos
