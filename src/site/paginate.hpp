#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace kappan::site {

struct Pagination {
  int page = 1;
  int pages = 1;
  std::string permalink;
  std::string prev;
  std::string next;
  std::vector<std::size_t> indices;
};

[[nodiscard]] std::vector<Pagination> paginate(const std::vector<std::size_t> &indices,
                                               int per_page);

} // namespace kappan::site
