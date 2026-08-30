#pragma once

#include <string>
#include <string_view>

namespace kappan::util {

[[nodiscard]] std::string join_url(std::string_view base_url, std::string_view permalink);

} // namespace kappan::util
