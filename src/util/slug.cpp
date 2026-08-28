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
  if (out.empty()) {
    return std::nullopt;
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
