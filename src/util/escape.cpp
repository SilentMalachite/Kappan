#include "util/escape.hpp"

namespace kappan::util {

std::string escape_markup(std::string_view text, std::string_view apostrophe) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    switch (static_cast<unsigned char>(ch)) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += apostrophe;
      break;
    default:
      // XML 1.0 の Char 生成規則が許さない制御文字は落とす。
      // 文字参照にしても不正なので、置換ではなく除去するしかない。
      // タブ・LF・CR は許される。0x7F も XML 1.0 では正当な文字なので残す。
      // 0x80 以上は UTF-8 の続きバイトなので絶対に触らない（日本語・絵文字を壊さない）。
      if (static_cast<unsigned char>(ch) < 0x20 && ch != '\t' && ch != '\n' && ch != '\r') {
        break;
      }
      out.push_back(ch);
      break;
    }
  }
  return out;
}

} // namespace kappan::util
