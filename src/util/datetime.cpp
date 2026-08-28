#include "util/datetime.hpp"

namespace kappan::util {
namespace {

[[nodiscard]] bool is_digit(char c) { return c >= '0' && c <= '9'; }

[[nodiscard]] std::optional<int> parse_digits(std::string_view text) {
  if (text.empty()) {
    return std::nullopt;
  }
  int value = 0;
  for (char c : text) {
    if (!is_digit(c)) {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  return value;
}

} // namespace

std::optional<std::chrono::sys_seconds> try_parse_iso_datetime(std::string_view text) {
  if (text.size() < 10 || text[4] != '-' || text[7] != '-') {
    return std::nullopt;
  }
  const auto year = parse_digits(text.substr(0, 4));
  const auto month = parse_digits(text.substr(5, 2));
  const auto day = parse_digits(text.substr(8, 2));
  if (!year || !month || !day) {
    return std::nullopt;
  }
  const std::chrono::year_month_day ymd{std::chrono::year{*year},
                                        std::chrono::month{static_cast<unsigned>(*month)},
                                        std::chrono::day{static_cast<unsigned>(*day)}};
  if (!ymd.ok()) {
    return std::nullopt;
  }
  std::chrono::sys_seconds tp{std::chrono::sys_days{ymd}};
  if (text.size() == 10) {
    return tp;
  }
  std::string_view rest = text.substr(10);
  if (rest.ends_with('Z')) {
    rest.remove_suffix(1);
  }
  if (rest.size() != 9 || rest[0] != 'T' || rest[3] != ':' || rest[6] != ':') {
    return std::nullopt;
  }
  const auto hour = parse_digits(rest.substr(1, 2));
  const auto minute = parse_digits(rest.substr(4, 2));
  const auto second = parse_digits(rest.substr(7, 2));
  if (!hour || !minute || !second || *hour > 23 || *minute > 59 || *second > 60) {
    return std::nullopt;
  }
  return tp + std::chrono::hours{*hour} + std::chrono::minutes{*minute} +
         std::chrono::seconds{*second};
}

DatedStem split_dated_stem(std::string_view stem) {
  DatedStem result;
  result.stem = std::string{stem};
  if (stem.size() < 12 || stem[4] != '-' || stem[7] != '-' || stem[10] != '-') {
    return result;
  }
  const auto date = try_parse_iso_datetime(stem.substr(0, 10));
  if (!date) {
    return result;
  }
  result.date = date;
  result.stem = std::string{stem.substr(11)};
  if (result.stem.empty()) {
    result.stem = std::string{stem};
    result.date.reset();
  }
  return result;
}

} // namespace kappan::util
