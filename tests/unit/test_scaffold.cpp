#include <kappan/embedded_theme.hpp>
#include <kappan/error.hpp>

#include "content/build.hpp"
#include "content/scaffold.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::string read_bytes(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("create_site writes every embedded template") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-new-site";
  std::filesystem::remove_all(dir);

  const auto created = kappan::content::create_site(dir);
  REQUIRE(created);
  REQUIRE(std::filesystem::exists(dir / "site.yaml"));
  const auto embedded_templates = std::to_array<std::pair<std::string_view, std::string_view>>({
      {"base.html", kappan::render::embedded::base_html},
      {"post.html", kappan::render::embedded::post_html},
      {"page.html", kappan::render::embedded::page_html},
      {"index.html", kappan::render::embedded::index_html},
      {"tag.html", kappan::render::embedded::tag_html},
      {"landing.html", kappan::render::embedded::landing_html},
  });
  for (const auto &[filename, embedded_bytes] : embedded_templates) {
    const auto template_path = dir / "templates" / std::string(filename);
    CAPTURE(filename);
    REQUIRE(std::filesystem::is_regular_file(template_path));
    REQUIRE(read_bytes(template_path) == embedded_bytes);
  }
  REQUIRE(read_bytes(dir / "site.yaml").find("活版サイト") != std::string::npos);

  const auto out = dir / "out";
  const auto result = kappan::content::build_site(dir, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 3);
  const auto post = read_bytes(out / kappan::util::from_utf8("posts") /
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
