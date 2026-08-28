#include <kappan/error.hpp>

#include "markdown/cmark.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string_view>

TEST_CASE("to_html preserves Japanese heading and emoji") {
  constexpr std::string_view md = "# こんにちは、世界 🐙\n";
  const auto html = kappan::markdown::to_html(md, "ja.md");
  REQUIRE(html);
  REQUIRE(html->find("こんにちは、世界 🐙") != std::string::npos);
  REQUIRE(html->find("<h1>") != std::string::npos);
}

TEST_CASE("to_html keeps mixed fullwidth and halfwidth ASCII") {
  constexpr std::string_view md = "全角ＡＢＣと半角ABC\n";
  const auto html = kappan::markdown::to_html(md, "mixed.md");
  REQUIRE(html);
  REQUIRE(html->find("全角ＡＢＣと半角ABC") != std::string::npos);
}

TEST_CASE("to_html renders GFM tables") {
  constexpr std::string_view md = "| 列 | 値 |\n| --- | --- |\n| 日本語 | 表 |\n";
  const auto html = kappan::markdown::to_html(md, "table.md");
  REQUIRE(html);
  REQUIRE(html->find("<table>") != std::string::npos);
  REQUIRE(html->find("日本語") != std::string::npos);
}

TEST_CASE("to_html renders GFM strikethrough") {
  constexpr std::string_view md = "~~打ち消し~~\n";
  const auto html = kappan::markdown::to_html(md, "strike.md");
  REQUIRE(html);
  REQUIRE(html->find("<del>") != std::string::npos);
  REQUIRE(html->find("打ち消し") != std::string::npos);
}

TEST_CASE("to_html output has no UTF-8 BOM") {
  const auto html = kappan::markdown::to_html("# 見出し\n", "bom.md");
  REQUIRE(html);
  REQUIRE_FALSE(html->starts_with("\xEF\xBB\xBF"));
}
