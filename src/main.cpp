#include <kappan/version.hpp>

#include "content/build.hpp"
#include "content/scaffold.hpp"
#include "serve/run.hpp"
#include "util/path.hpp"

#include <kappan/site.hpp>

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include <cstdint>
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
    bool force = false;
    build->add_flag("--force", force, "kappan の出力先でない非空ディレクトリでも消す");

    std::filesystem::path serve_source;
    std::string serve_host{"127.0.0.1"};
    std::uint16_t serve_port = 8080;
    bool serve_drafts = false;
    bool serve_watch = false;
    auto *serve = app.add_subcommand("serve", "生成結果をローカルで配信する");
    serve->add_option("--source", serve_source, "サイト根ディレクトリ（site.yaml）")->required();
    serve->add_option("--host", serve_host, "待ち受けホスト")->capture_default_str();
    serve->add_option("--port", serve_port, "待ち受けポート")
        ->capture_default_str()
        ->check(CLI::Range(1, 65535));
    serve->add_flag("--drafts", serve_drafts, "下書きを含める");
    serve->add_flag("--watch", serve_watch, "ソースの変更を監視して再生成する");

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
      const auto out_policy =
          force ? kappan::output::OutDirPolicy::Force : kappan::output::OutDirPolicy::Refuse;
      const auto result = kappan::content::build_site(source, out, drafts, out_policy);
      for (const auto &error : result.errors) {
        spdlog::error("{}", error.message);
      }
      if (!result.ok()) {
        return EXIT_FAILURE;
      }
      spdlog::debug("wrote {} pages to {}", result.pages_written, kappan::util::to_utf8(out));
    }
    if (serve->parsed()) {
      const kappan::serve::ServeOptions options{
          .source = serve_source,
          .host = serve_host,
          .port = serve_port,
          .drafts = serve_drafts ? kappan::DraftPolicy::Include : kappan::DraftPolicy::Exclude,
          .watch = serve_watch,
      };
      const auto result = kappan::serve::run(options);
      if (!result) {
        spdlog::error("{}", result.error().message);
        return EXIT_FAILURE;
      }
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
