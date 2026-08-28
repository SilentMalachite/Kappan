#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <vector>

namespace kappan::content {

struct BuildResult {
  int pages_written = 0;
  std::vector<Error> errors;

  [[nodiscard]] bool ok() const { return errors.empty(); }
};

[[nodiscard]] BuildResult build_site(const std::filesystem::path &source,
                                     const std::filesystem::path &out_dir);

} // namespace kappan::content
