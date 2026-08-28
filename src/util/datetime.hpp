#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace kappan::util {

struct DatedStem {
  std::optional<std::chrono::sys_seconds> date;
  std::string stem;
};

[[nodiscard]] std::optional<std::chrono::sys_seconds> try_parse_iso_datetime(std::string_view text);

[[nodiscard]] std::string format_iso_datetime(std::chrono::sys_seconds tp);

[[nodiscard]] std::string format_display_date(std::chrono::sys_seconds tp);

[[nodiscard]] std::string format_rfc822(std::chrono::sys_seconds tp);

[[nodiscard]] DatedStem split_dated_stem(std::string_view stem);

} // namespace kappan::util
