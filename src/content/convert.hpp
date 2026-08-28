#pragma once

#include <kappan/error.hpp>

#include <filesystem>

namespace kappan::content {

[[nodiscard]] Result<void> convert_markdown_file(const std::filesystem::path &source,
                                                 const std::filesystem::path &out_dir);

} // namespace kappan::content
