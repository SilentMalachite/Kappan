#include "util/slug.hpp"

#include "util/utf8.hpp"

namespace kappan::util {
namespace {

void append_utf8(std::string &out, char32_t cp) {
  if (cp <= 0x7F) {
    out.push_back(static_cast<char>(cp));
  } else if (cp <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

[[nodiscard]] bool is_ascii_alnum(char32_t cp) {
  return (cp >= U'0' && cp <= U'9') || (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z');
}

[[nodiscard]] bool is_space(char32_t cp) {
  return cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\u3000';
}

[[nodiscard]] bool is_reserved(char32_t cp) {
  return cp == 0 || cp == U'<' || cp == U'>' || cp == U':' || cp == U'"' || cp == U'/' ||
         cp == U'\\' || cp == U'|' || cp == U'?' || cp == U'*';
}

[[nodiscard]] bool is_windows_device_name(std::string_view slug) {
  const auto dot = slug.find('.');
  const auto base = slug.substr(0, dot);
  if (base == "con" || base == "prn" || base == "aux" || base == "nul") {
    return true;
  }
  return base.size() == 4 && (base.starts_with("com") || base.starts_with("lpt")) &&
         base.back() >= '1' && base.back() <= '9';
}

} // namespace

std::optional<std::string> try_slugify(std::string_view text) {
  std::string out;
  bool last_dash = false;
  auto rest = text;
  while (!rest.empty()) {
    const auto cp = next_codepoint(rest);
    if (!cp) {
      rest.remove_prefix(1);
      continue;
    }
    char32_t value = *cp;
    if (value >= U'A' && value <= U'Z') {
      value = value - U'A' + U'a';
    }
    if (is_ascii_alnum(value)) {
      out.push_back(static_cast<char>(value));
      last_dash = false;
      continue;
    }
    if (is_space(value) || is_reserved(value)) {
      if (!out.empty() && !last_dash) {
        out.push_back('-');
        last_dash = true;
      }
      continue;
    }
    append_utf8(out, value);
    last_dash = false;
  }
  while (!out.empty() && out.back() == '-') {
    out.pop_back();
  }
  while (!out.empty() && out.back() == '.') {
    out.pop_back();
  }
  if (out.empty()) {
    return std::nullopt;
  }
  // "." と ".." はパス成分として親を指し、出力先が --out の外へ逃げる。
  // '.' 自体は予約文字にしない（"v1.2" や "..記事" のような slug を壊さないため）。
  if (out.find_first_not_of('.') == std::string::npos) {
    return std::nullopt;
  }
  if (is_windows_device_name(out)) {
    out.insert(out.begin(), '_');
  }
  return out;
}

std::string slugify(std::string_view text) {
  if (auto slug = try_slugify(text)) {
    return std::move(*slug);
  }
  return std::string{kUntitledSlug};
}

} // namespace kappan::util
