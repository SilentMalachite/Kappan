#include "util/url.hpp"

namespace kappan::util {

std::string join_url(std::string_view base_url, std::string_view permalink) {
  const auto suffix_start = base_url.find_first_of("?#");
  const auto suffix =
      suffix_start == std::string_view::npos ? std::string_view{} : base_url.substr(suffix_start);
  std::string base{base_url.substr(0, suffix_start)};
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (permalink.empty()) {
    base += '/';
  } else {
    base.append(permalink);
  }
  base.append(suffix);
  return base;
}

} // namespace kappan::util
