#include "util/utf8.hpp"

#include "util/path.hpp"

#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <string>

namespace kappan::util {
namespace {

[[nodiscard]] std::string_view strip_bom(std::string_view bytes) {
  constexpr unsigned char kBom[] = {0xEF, 0xBB, 0xBF};
  if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == kBom[0] &&
      static_cast<unsigned char>(bytes[1]) == kBom[1] &&
      static_cast<unsigned char>(bytes[2]) == kBom[2]) {
    bytes.remove_prefix(3);
  }
  return bytes;
}

[[nodiscard]] std::string normalize_newlines(std::string_view bytes) {
  std::string out;
  out.reserve(bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (bytes[i] == '\r') {
      out.push_back('\n');
      if (i + 1 < bytes.size() && bytes[i + 1] == '\n') {
        ++i;
      }
    } else {
      out.push_back(bytes[i]);
    }
  }
  return out;
}

} // namespace

bool is_valid_utf8(std::string_view bytes) {
  const auto *p = reinterpret_cast<const unsigned char *>(bytes.data());
  const auto *const end = p + bytes.size();
  while (p < end) {
    if (*p <= 0x7F) {
      ++p;
      continue;
    }
    int extra = 0;
    std::uint32_t cp = 0;
    if ((*p & 0xE0) == 0xC0) {
      extra = 1;
      cp = *p & 0x1FU;
    } else if ((*p & 0xF0) == 0xE0) {
      extra = 2;
      cp = *p & 0x0FU;
    } else if ((*p & 0xF8) == 0xF0) {
      extra = 3;
      cp = *p & 0x07U;
    } else {
      return false;
    }
    ++p;
    for (int i = 0; i < extra; ++i) {
      if (p >= end || (*p & 0xC0) != 0x80) {
        return false;
      }
      cp = (cp << 6) | (*p & 0x3FU);
      ++p;
    }
    const std::uint32_t mins[] = {0, 0x80, 0x800, 0x10000};
    if (cp < mins[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
      return false;
    }
  }
  return true;
}

Result<std::string> read_utf8_file(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("{}: ファイルを読み込めません", to_utf8(path)), path));
  }
  const std::string raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  const auto without_bom = strip_bom(raw);
  if (!is_valid_utf8(without_bom)) {
    return tl::unexpected(make_error(
        ErrorCode::Utf8, std::format("{}: UTF-8 として解釈できません", to_utf8(path)), path));
  }
  return normalize_newlines(without_bom);
}

Result<void> write_utf8_file(const std::filesystem::path &path, std::string_view content) {
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return tl::unexpected(make_error(
          ErrorCode::Io, std::format("{}: 出力先を作成できません: {}", to_utf8(path), ec.message()),
          path));
    }
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("{}: ファイルを書き込めません", to_utf8(path)), path));
  }
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!out) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("{}: ファイルを書き込めません", to_utf8(path)), path));
  }
  return {};
}

} // namespace kappan::util
