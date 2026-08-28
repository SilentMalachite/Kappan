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

// sitemap の <lastmod> 用。W3C Datetime に従い、日時形式にはタイムゾーン指定子を必ず付ける。
// 日付のみ（complete date 形式）は TZD 不要。テンプレート変数 date が使う
// format_iso_datetime とは用途が違うので分けてある（ADR-0008）。
[[nodiscard]] std::string format_w3c_datetime(std::chrono::sys_seconds tp);

[[nodiscard]] std::string format_display_date(std::chrono::sys_seconds tp);

[[nodiscard]] std::string format_rfc822(std::chrono::sys_seconds tp);

[[nodiscard]] DatedStem split_dated_stem(std::string_view stem);

} // namespace kappan::util
