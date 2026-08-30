#include <kappan/error.hpp>

#include "util/path.hpp"
#include "util/utf8.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path temp_file(const std::string &name) {
  auto path = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove(path);
  return path;
}

// 読み終えたらすぐ閉じる。Windows は開いたままのファイルを削除できないため、
// 呼び出し側の remove が「別のプロセスが使用中」で失敗する。
[[nodiscard]] std::vector<char> read_bytes(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("read_utf8_file keeps hiragana kanji and emoji") {
  const auto path = temp_file("kappan-utf8-ja.md");
  {
    std::ofstream out(path, std::ios::binary);
    out << "# こんにちは、世界 🐙\n";
  }

  const auto result = kappan::util::read_utf8_file(path);
  REQUIRE(result);
  REQUIRE(result->find("こんにちは、世界 🐙") != std::string::npos);
  std::filesystem::remove(path);
}

TEST_CASE("read_utf8_file strips UTF-8 BOM") {
  const auto path = temp_file("kappan-utf8-bom.md");
  {
    std::ofstream out(path, std::ios::binary);
    out << "\xEF\xBB\xBF"
        << "見出し\n";
  }

  const auto result = kappan::util::read_utf8_file(path);
  REQUIRE(result);
  REQUIRE_FALSE(result->starts_with("\xEF\xBB\xBF"));
  REQUIRE(result->starts_with("見出し"));
  std::filesystem::remove(path);
}

TEST_CASE("read_utf8_file normalizes CRLF to LF") {
  const auto path = temp_file("kappan-utf8-crlf.md");
  {
    std::ofstream out(path, std::ios::binary);
    out << "一行目\r\n二行目\r\n";
  }

  const auto result = kappan::util::read_utf8_file(path);
  REQUIRE(result);
  REQUIRE(*result == "一行目\n二行目\n");
  std::filesystem::remove(path);
}

TEST_CASE("read_utf8_file rejects invalid UTF-8 without throwing") {
  const auto path = temp_file("kappan-utf8-bad.md");
  {
    std::ofstream out(path, std::ios::binary);
    const unsigned char bytes[] = {0xC3, 0x28, '\n'}; // invalid 2-byte sequence
    out.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
  }

  const auto result = kappan::util::read_utf8_file(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Utf8);
  REQUIRE(result.error().where.has_value());
  REQUIRE(result.error().message.find("UTF-8") != std::string::npos);
  std::filesystem::remove(path);
}

TEST_CASE("from_utf8 and to_utf8 round-trip Japanese and emoji") {
  const auto path = kappan::util::from_utf8("記事🐙.md");
  REQUIRE(kappan::util::to_utf8(path).find("記事🐙") != std::string::npos);
}

TEST_CASE("write_utf8_file does not emit a BOM") {
  const auto path = temp_file("kappan-utf8-out.html");
  const auto written = kappan::util::write_utf8_file(path, "<p>日本語 🐙</p>\n");
  REQUIRE(written);

  const auto bytes = read_bytes(path);
  REQUIRE(bytes.size() >= 3);
  const bool has_bom = static_cast<unsigned char>(bytes[0]) == 0xEF &&
                       static_cast<unsigned char>(bytes[1]) == 0xBB &&
                       static_cast<unsigned char>(bytes[2]) == 0xBF;
  REQUIRE_FALSE(has_bom);
  const std::string text(bytes.begin(), bytes.end());
  REQUIRE(text.find("日本語 🐙") != std::string::npos);
  std::filesystem::remove(path);
}
