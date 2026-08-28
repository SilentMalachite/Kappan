#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace kappan::util {

[[nodiscard]] bool is_valid_utf8(std::string_view bytes);

[[nodiscard]] Result<std::string> read_utf8_file(const std::filesystem::path &path);

[[nodiscard]] Result<void> write_utf8_file(const std::filesystem::path &path,
                                           std::string_view content);

} // namespace kappan::util
