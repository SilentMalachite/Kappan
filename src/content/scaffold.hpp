#pragma once

#include <kappan/error.hpp>

#include <filesystem>

namespace kappan::content {

[[nodiscard]] Result<void> create_site(const std::filesystem::path &dir);

} // namespace kappan::content
