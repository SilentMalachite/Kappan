#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <vector>

namespace kappan::content {

[[nodiscard]] Result<std::vector<std::filesystem::path>>
scan_markdown(const std::filesystem::path &content_dir);

} // namespace kappan::content
