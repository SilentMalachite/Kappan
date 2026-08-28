#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace kappan::markdown {

[[nodiscard]] Result<std::string> to_html(std::string_view markdown,
                                          const std::filesystem::path &where);

} // namespace kappan::markdown
