#include <kappan/config.hpp>
#include <kappan/document.hpp>
#include <kappan/error.hpp>

#include "content/parse.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

kappan::Config test_config(const std::filesystem::path &root) {
  kappan::Config config;
  config.title = "テスト";
  config.source_root = root;
  config.content_dir = root / "content";
  return config;
}

} // namespace

TEST_CASE("parse_document reads Japanese front matter and tags") {
  const auto root = fixtures_dir() / "site-ja";
  const auto source =
      root / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md");
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "こんにちは");
  REQUIRE(result->front_matter.layout == "post");
  REQUIRE(result->front_matter.slug == "こんにちは");
  REQUIRE(result->front_matter.tags.size() == 2);
  REQUIRE(result->front_matter.tags[0] == "日本語");
  REQUIRE(result->permalink == "/posts/" + result->front_matter.slug + "/");
  REQUIRE(result->body_html.find("最初の記事です") != std::string::npos);
  REQUIRE(result->front_matter.date);
  REQUIRE(*result->front_matter.date == std::chrono::sys_days{std::chrono::year{2026} / 1 / 1});
}

TEST_CASE("parse_document defaults when front matter is absent") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-none";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("説明.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "# 見出しだけ\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "説明");
  REQUIRE(result->front_matter.layout == "page");
  REQUIRE(result->permalink == "/説明/");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reports a bad date with a file line") {
  const auto root = fixtures_dir() / "site-bad-fm";
  const auto source = root / "content" / "posts" / "2026-01-01-hello.md";
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line == 3);
  REQUIRE(result.error().message.find("date") != std::string::npos);
  REQUIRE(result.error().message.find("2026-13-01") != std::string::npos);
}

TEST_CASE("parse_document reports broken YAML without throwing") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-broken";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "broken.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: [\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document accepts CRLF front matter") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-crlf";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("改行.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\r\ntitle: 改行\r\n---\r\n本文\r\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "改行");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document maps index.md to the site root") {
  const auto root = fixtures_dir() / "site-ja";
  const auto source = root / "content" / "index.md";
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->permalink == "/");
  REQUIRE(result->output_path == std::filesystem::path{"index.html"});
}
