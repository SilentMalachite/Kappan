#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace kappan::util {

inline constexpr std::string_view kUntitledSlug = "untitled";

[[nodiscard]] std::optional<std::string> try_slugify(std::string_view text);

[[nodiscard]] std::string slugify(std::string_view text);

} // namespace kappan::util
