#include "site/paginate.hpp"

#include <algorithm>
#include <format>

namespace kappan::site {

std::vector<Pagination> paginate(const std::vector<std::size_t> &indices, int per_page) {
  const auto total = indices.size();
  const std::size_t chunk =
      per_page <= 0 ? std::max<std::size_t>(total, 1) : static_cast<std::size_t>(per_page);
  const std::size_t page_count = total == 0 ? 1 : (total + chunk - 1) / chunk;

  std::vector<Pagination> pages;
  pages.reserve(page_count);
  for (std::size_t n = 1; n <= page_count; ++n) {
    Pagination page;
    page.page = static_cast<int>(n);
    page.pages = static_cast<int>(page_count);
    page.permalink = n == 1 ? std::string{"/"} : std::format("/page/{}/", n);
    if (n > 1) {
      page.prev = n == 2 ? std::string{"/"} : std::format("/page/{}/", n - 1);
    }
    if (n < page_count) {
      page.next = std::format("/page/{}/", n + 1);
    }
    const auto begin = (n - 1) * chunk;
    if (begin < total) {
      const auto end = std::min(begin + chunk, total);
      page.indices.assign(indices.begin() + static_cast<std::ptrdiff_t>(begin),
                          indices.begin() + static_cast<std::ptrdiff_t>(end));
    }
    pages.push_back(std::move(page));
  }
  return pages;
}

} // namespace kappan::site
