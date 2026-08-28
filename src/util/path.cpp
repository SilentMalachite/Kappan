#include "util/path.hpp"

#include <algorithm>
#include <ranges>
#include <string_view>

namespace kappan::util {
namespace {

[[nodiscard]] std::u8string to_u8string(std::string_view bytes) {
  std::u8string out(bytes.size(), char8_t{0});
  std::ranges::transform(bytes, out.begin(), [](char c) {
    return static_cast<char8_t>(static_cast<unsigned char>(c));
  });
  return out;
}

[[nodiscard]] std::string from_u8string(const std::u8string &bytes) {
  std::string out(bytes.size(), '\0');
  std::ranges::transform(bytes, out.begin(), [](char8_t c) { return static_cast<char>(c); });
  return out;
}

} // namespace

std::filesystem::path from_utf8(std::string_view utf8) {
  return std::filesystem::path{to_u8string(utf8)};
}

std::string to_utf8(const std::filesystem::path &path) { return from_u8string(path.u8string()); }

std::string to_generic_utf8(const std::filesystem::path &path) {
  return from_u8string(path.generic_u8string());
}

std::filesystem::path output_from_permalink(std::string_view permalink) {
  if (permalink == "/") {
    return std::filesystem::path{"index.html"};
  }
  if (permalink.starts_with('/')) {
    permalink.remove_prefix(1);
  }
  if (permalink.ends_with('/')) {
    permalink.remove_suffix(1);
  }
  std::filesystem::path out;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= permalink.size(); ++i) {
    if (i == permalink.size() || permalink[i] == '/') {
      out /= from_utf8(permalink.substr(start, i - start));
      start = i + 1;
    }
  }
  return out / "index.html";
}

} // namespace kappan::util
