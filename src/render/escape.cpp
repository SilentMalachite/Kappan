#include "render/escape.hpp"

namespace kappan::render {

std::string html_escape(std::string_view text) {
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
      out += "&#39;";
      break;
    default:
      out.push_back(ch);
      break;
    }
  }
  return out;
}

} // namespace kappan::render
