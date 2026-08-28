#include "content/build.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>
#include <string>
#include <vector>

namespace {

std::filesystem::path repo_root() {
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string read_all(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::filesystem::path> list_files(const std::filesystem::path &root) {
  std::vector<std::filesystem::path> files;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
    if (entry.is_regular_file()) {
      files.push_back(std::filesystem::relative(entry.path(), root));
    }
  }
  std::ranges::sort(files);
  return files;
}

} // namespace

TEST_CASE("golden blog-ja matches examples/blog") {
  const auto source = repo_root() / "examples" / "blog";
  const auto expected = std::filesystem::path(__FILE__).parent_path() / "blog-ja" / "expected";
  const auto out = std::filesystem::temp_directory_path() / "kappan-golden-single-post";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());

  const auto expected_files = list_files(expected);
  const auto actual_files = list_files(out);
  REQUIRE(actual_files == expected_files);
  for (const auto &rel : expected_files) {
    const auto got = read_all(out / rel);
    const auto want = read_all(expected / rel);
    REQUIRE_FALSE(got.starts_with("\xEF\xBB\xBF"));
    REQUIRE(got == want);
  }

  std::filesystem::remove_all(out);
}
