#include <kappan/version.hpp>

#include "content/convert.hpp"
#include "util/path.hpp"

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
    auto *build = app.add_subcommand("build", "Markdown 1 ファイルを HTML に変換する");
    build->add_option("--source", source, "入力 Markdown ファイル")->required();
    build->add_option("--out", out, "出力ディレクトリ")->required();

    CLI11_PARSE(app, argc, argv);
    if (verbose) {
      spdlog::set_level(spdlog::level::debug);
    }
    if (build->parsed()) {
      const auto result = kappan::content::convert_markdown_file(source, out);
      if (!result) {
        spdlog::error("{}", result.error().message);
        return EXIT_FAILURE;
      }
      spdlog::debug("converted {} -> {}", kappan::util::to_utf8(source),
                    kappan::util::to_utf8(out));
    }
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << std::format("{}\n", ex.what());
    return EXIT_FAILURE;
  }
}
