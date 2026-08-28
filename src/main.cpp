#include <kappan/version.hpp>

#include "content/build.hpp"
#include "content/scaffold.hpp"
#include "util/path.hpp"

#include <kappan/site.hpp>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>

int main(int argc, char **argv) {
  try {
    CLI::App app{"Kappan — static site generator"};
    app.set_version_flag("--version,-v", kappan::version_string());
    bool verbose = false;
    app.add_flag("--verbose", verbose, "詳細ログを出す");

    std::filesystem::path source;
    std::filesystem::path out;
    auto *build = app.add_subcommand("build", "サイトを HTML に変換する");
    build->add_option("--source", source, "サイト根ディレクトリ（site.yaml）")->required();
    build->add_option("--out", out, "出力ディレクトリ")->required();
    bool include_drafts = false;
    build->add_flag("--drafts", include_drafts, "下書きを含める");

    std::filesystem::path new_dir;
    auto *new_cmd = app.add_subcommand("new", "サイトの骨格を作る");
    new_cmd->add_option("dir", new_dir, "作成するディレクトリ")->required();

    CLI11_PARSE(app, argc, argv);
    if (verbose) {
      spdlog::set_level(spdlog::level::debug);
    }
    if (build->parsed()) {
      const auto drafts =
          include_drafts ? kappan::DraftPolicy::Include : kappan::DraftPolicy::Exclude;
      const auto result = kappan::content::build_site(source, out, drafts);
      for (const auto &error : result.errors) {
        spdlog::error("{}", error.message);
      }
      if (!result.ok()) {
        return EXIT_FAILURE;
      }
      spdlog::debug("wrote {} pages to {}", result.pages_written, kappan::util::to_utf8(out));
    }
    if (new_cmd->parsed()) {
      const auto created = kappan::content::create_site(new_dir);
      if (!created) {
        spdlog::error("{}", created.error().message);
        return EXIT_FAILURE;
      }
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << std::format("{}\n", ex.what());
    return EXIT_FAILURE;
  }
}
