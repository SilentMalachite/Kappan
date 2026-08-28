#pragma once

#include <string>
#include <string_view>

namespace kappan::util {

[[nodiscard]] std::string slugify(std::string_view text);

} // namespace kappan::util
