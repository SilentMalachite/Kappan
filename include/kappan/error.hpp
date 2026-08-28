#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <tl/expected.hpp>

namespace kappan {

enum class ErrorCode {
  Io,
  Utf8,
  Config,
  FrontMatter,
  Markdown,
  Template,
  Path,
  Cli,
};

struct Error {
  ErrorCode code;
  std::string message;
  std::optional<std::filesystem::path> where;
  std::optional<int> line;
};

template <class T> using Result = tl::expected<T, Error>;

[[nodiscard]] inline Error make_error(ErrorCode code, std::string message,
                                      std::optional<std::filesystem::path> where = {},
                                      std::optional<int> line = {}) {
  return Error{code, std::move(message), std::move(where), line};
}

} // namespace kappan
