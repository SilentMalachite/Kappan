#include <kappan/error.hpp>

#include "content/build.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

std::string read_all(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("build_site writes pretty URLs for a Japanese blog") {
  const auto source = fixtures_dir() / "site-ja";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-ja-out";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 3);

  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("ホーム 🐙") != std::string::npos);
  REQUIRE_FALSE(home.starts_with("\xEF\xBB\xBF"));

  const auto about = read_all(out / "about" / "index.html");
  REQUIRE(about.find("概要の本文です") != std::string::npos);

  const auto post = read_all(out / kappan::util::from_utf8("posts") /
                             kappan::util::from_utf8("こんにちは") / "index.html");
  REQUIRE(post.find("最初の記事です") != std::string::npos);

  std::filesystem::remove_all(out);
}

TEST_CASE("build_site keeps good pages when one front matter is broken") {
  const auto source = fixtures_dir() / "site-bad-fm";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-partial-out";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.pages_written == 1);
  REQUIRE(result.errors.size() >= 1);
  REQUIRE(result.errors.front().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(std::filesystem::exists(out / "ok" / "index.html"));
  REQUIRE_FALSE(std::filesystem::exists(out / "posts" / "hello" / "index.html"));

  std::filesystem::remove_all(out);
}

TEST_CASE("build_site rejects a Markdown file as --source") {
  const auto file = fixtures_dir() / "ja_emoji.md";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-file-out";
  const auto result = kappan::content::build_site(file, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.size() == 1);
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Cli);
  REQUIRE(result.errors.front().message.find("サイトの根ディレクトリ") != std::string::npos);
}

TEST_CASE("build_site reports a missing site.yaml") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-empty-site";
  std::filesystem::create_directories(source);
  const auto result = kappan::content::build_site(source, source / "out");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Config);
  std::filesystem::remove_all(source);
}
