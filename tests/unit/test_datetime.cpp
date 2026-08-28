#include "util/datetime.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

TEST_CASE("try_parse_iso_datetime accepts a calendar date") {
  const auto value = kappan::util::try_parse_iso_datetime("2026-01-01");
  REQUIRE(value);
  REQUIRE(*value == std::chrono::sys_days{std::chrono::year{2026} / 1 / 1});
}

TEST_CASE("try_parse_iso_datetime accepts a local datetime") {
  const auto value = kappan::util::try_parse_iso_datetime("2026-01-01T15:04:05");
  REQUIRE(value);
  const auto expected = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1} +
                        std::chrono::hours{15} + std::chrono::minutes{4} + std::chrono::seconds{5};
  REQUIRE(*value == expected);
}

TEST_CASE("format_iso_datetime prints a calendar date at midnight") {
  const auto value = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  REQUIRE(kappan::util::format_iso_datetime(value) == "2026-01-01");
}

TEST_CASE("format_display_date prints a Japanese calendar date") {
  const auto value = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  REQUIRE(kappan::util::format_display_date(value) == "2026年1月1日");
}

TEST_CASE("format_rfc822 prints English UTC without locale") {
  const auto midnight = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  REQUIRE(kappan::util::format_rfc822(midnight) == "Thu, 01 Jan 2026 00:00:00 +0000");
  const auto afternoon =
      midnight + std::chrono::hours{15} + std::chrono::minutes{4} + std::chrono::seconds{5};
  REQUIRE(kappan::util::format_rfc822(afternoon) == "Thu, 01 Jan 2026 15:04:05 +0000");
}

TEST_CASE("format_iso_datetime prints a local datetime") {
  const auto value = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1} +
                     std::chrono::hours{15} + std::chrono::minutes{4} + std::chrono::seconds{5};
  REQUIRE(kappan::util::format_iso_datetime(value) == "2026-01-01T15:04:05");
}

TEST_CASE("try_parse_iso_datetime rejects impossible dates") {
  REQUIRE_FALSE(kappan::util::try_parse_iso_datetime("2026-13-01"));
  REQUIRE_FALSE(kappan::util::try_parse_iso_datetime("2026-01-32"));
  REQUIRE_FALSE(kappan::util::try_parse_iso_datetime("not-a-date"));
}

TEST_CASE("split_dated_stem strips a valid date prefix") {
  const auto split = kappan::util::split_dated_stem("2026-01-01-こんにちは");
  REQUIRE(split.stem == "こんにちは");
  REQUIRE(split.date);
  REQUIRE(*split.date == std::chrono::sys_days{std::chrono::year{2026} / 1 / 1});
}

TEST_CASE("split_dated_stem leaves an invalid date prefix in the stem") {
  const auto split = kappan::util::split_dated_stem("2026-13-01-hello");
  REQUIRE(split.stem == "2026-13-01-hello");
  REQUIRE_FALSE(split.date);
}
