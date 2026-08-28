#include <kappan/error.hpp>

#include "content/build.hpp"
#include "content/scaffold.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::string read_all(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("create_site writes a Japanese sample that builds") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-new-site";
  std::filesystem::remove_all(dir);

  const auto created = kappan::content::create_site(dir);
  REQUIRE(created);
  REQUIRE(std::filesystem::exists(dir / "site.yaml"));
  REQUIRE(std::filesystem::exists(dir / "templates" / "post.html"));
  REQUIRE(read_all(dir / "site.yaml").find("活版サイト") != std::string::npos);

  const auto out = dir / "out";
  const auto result = kappan::content::build_site(dir, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 2);
  const auto post = read_all(out / kappan::util::from_utf8("posts") /
                             kappan::util::from_utf8("こんにちは") / "index.html");
  REQUIRE(post.find("<html lang=\"ja\">") != std::string::npos);
  REQUIRE(post.find("最初の記事です") != std::string::npos);

  std::filesystem::remove_all(dir);
}

TEST_CASE("create_site rejects a non-empty directory") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-new-occupied";
  std::filesystem::create_directories(dir);
  {
    std::ofstream out(dir / "already.txt", std::ios::binary);
    out << "nope\n";
  }
  const auto created = kappan::content::create_site(dir);
  REQUIRE_FALSE(created);
  REQUIRE(created.error().code == kappan::ErrorCode::Cli);
  std::filesystem::remove_all(dir);
}
