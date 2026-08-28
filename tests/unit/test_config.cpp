#include <kappan/config.hpp>
#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

} // namespace

TEST_CASE("load reads Japanese site.yaml and ignores unknown keys") {
  const auto path = fixtures_dir() / "site-ja" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->title == "活版ブログ");
  REQUIRE(result->url == "https://example.com");
  REQUIRE(result->language == "ja");
  REQUIRE(result->description.find("🐙") != std::string::npos);
}

TEST_CASE("load reports a missing title with a line number") {
  const auto path = fixtures_dir() / "site-bad-config" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  REQUIRE(result.error().message.find("title") != std::string::npos);
}

TEST_CASE("load reports broken YAML without throwing") {
  const auto path = fixtures_dir() / "site-broken-yaml" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line >= 1);
}

TEST_CASE("load reports a missing site.yaml") {
  const auto path = fixtures_dir() / "site-ja" / "missing.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  const bool mentions_file = result.error().message.find("site.yaml") != std::string::npos ||
                             result.error().message.find("missing.yaml") != std::string::npos;
  REQUIRE(mentions_file);
}

TEST_CASE("load accepts CRLF site.yaml") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-crlf.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: CRLFサイト\r\nlanguage: ja\r\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->title == "CRLFサイト");
  std::filesystem::remove(path);
}

TEST_CASE("load reads pagination posts_per_page") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-paginate.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: ページ\npagination:\n  posts_per_page: 2\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->posts_per_page == 2);
  std::filesystem::remove(path);
}

TEST_CASE("load rejects a negative posts_per_page") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-paginate-bad.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: ページ\npagination:\n  posts_per_page: -1\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove(path);
}

TEST_CASE("load rejects a sequence title") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-seq.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title:\n  - a\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove(path);
}
