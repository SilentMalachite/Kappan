#pragma once

#include <string>
#include <string_view>

namespace kappan::render {

[[nodiscard]] std::string html_escape(std::string_view text);

} // namespace kappan::render
