#include "util/url.hpp"

namespace kappan::util {

std::string join_url(std::string_view base_url, std::string_view permalink) {
  std::string base{base_url};
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (permalink.empty()) {
    return base + "/";
  }
  return base + std::string{permalink};
}

} // namespace kappan::util
