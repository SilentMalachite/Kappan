#include <kappan/error.hpp>

#include "content/convert.hpp"
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

TEST_CASE("convert_markdown_file writes HTML next to Japanese content") {
  const auto source = fixtures_dir() / "ja_emoji.md";
  const auto out_dir = std::filesystem::temp_directory_path() / "kappan-out-ja";
  std::filesystem::remove_all(out_dir);

  const auto result = kappan::content::convert_markdown_file(source, out_dir);
  REQUIRE(result);

  const auto html_path = out_dir / "ja_emoji.html";
  REQUIRE(std::filesystem::exists(html_path));
  const auto html = read_all(html_path);
  REQUIRE_FALSE(html.starts_with("\xEF\xBB\xBF"));
  REQUIRE(html.find("こんにちは、世界 🐙") != std::string::npos);
  REQUIRE(html.find("<table>") != std::string::npos);
  REQUIRE(html.find("<del>") != std::string::npos);
  std::filesystem::remove_all(out_dir);
}

TEST_CASE("convert_markdown_file accepts a Japanese source filename") {
  const auto source = fixtures_dir() / kappan::util::from_utf8("日本語ファイル.md");
  const auto out_dir = std::filesystem::temp_directory_path() / "kappan-out-name";
  std::filesystem::remove_all(out_dir);

  const auto result = kappan::content::convert_markdown_file(source, out_dir);
  REQUIRE(result);

  const auto html_path = out_dir / kappan::util::from_utf8("日本語ファイル.html");
  REQUIRE(std::filesystem::exists(html_path));
  REQUIRE(read_all(html_path).find("日本語ファイル名") != std::string::npos);
  std::filesystem::remove_all(out_dir);
}

TEST_CASE("convert_markdown_file reports a missing source path") {
  const auto missing = fixtures_dir() / kappan::util::from_utf8("存在しない.md");
  const auto out_dir = std::filesystem::temp_directory_path() / "kappan-out-missing";
  const auto result = kappan::content::convert_markdown_file(missing, out_dir);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Io);
  REQUIRE(result.error().message.find("存在しない.md") != std::string::npos);
}

TEST_CASE("convert_markdown_file rejects a directory as --source") {
  const auto result = kappan::content::convert_markdown_file(
      fixtures_dir(), std::filesystem::temp_directory_path());
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Cli);
}
