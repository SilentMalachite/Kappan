#pragma once

#include <string>
#include <string_view>

namespace kappan::output {

[[nodiscard]] std::string xml_escape(std::string_view text);

[[nodiscard]] std::string join_url(std::string_view base_url, std::string_view permalink);

} // namespace kappan::output
