#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace kappan::util {

[[nodiscard]] std::filesystem::path from_utf8(std::string_view utf8);

[[nodiscard]] std::string to_utf8(const std::filesystem::path &path);

[[nodiscard]] std::string to_generic_utf8(const std::filesystem::path &path);

[[nodiscard]] std::filesystem::path output_from_permalink(std::string_view permalink);

} // namespace kappan::util
