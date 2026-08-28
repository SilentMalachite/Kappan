#include "render/engine.hpp"

#include "render/context.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/embedded_theme.hpp>

#include <inja/inja.hpp>

#include <format>
#include <map>
#include <string_view>
#include <utility>

namespace kappan::render {
namespace {

struct NamedTheme {
  std::string_view name;
  std::string_view source;
};

constexpr NamedTheme kEmbedded[] = {
    {"base.html", embedded::base_html},
    {"post.html", embedded::post_html},
    {"page.html", embedded::page_html},
};

[[nodiscard]] int inja_line(const inja::SourceLocation &location) {
  if (location.line == 0) {
    return 1;
  }
  return static_cast<int>(location.line);
}

[[nodiscard]] Result<void> install_template(inja::Environment &env,
                                            std::map<std::string, inja::Template> &layouts,
                                            const std::string &name, std::string_view source,
                                            const std::filesystem::path &where) {
  try {
    auto tmpl = env.parse(source);
    env.include_template(name, tmpl);
    layouts.insert_or_assign(name, std::move(tmpl));
    return {};
  } catch (const inja::InjaError &ex) {
    const int line = inja_line(ex.location);
    return tl::unexpected(make_error(ErrorCode::Template,
                                     std::format("{}:{} テンプレートを解析できません: {}",
                                                 util::to_generic_utf8(where), line, ex.message),
                                     where, line));
  }
}

} // namespace

struct Engine::Impl {
  Config config;
  inja::Environment env;
  std::map<std::string, inja::Template> layouts;
};

Engine::Engine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;
Engine::~Engine() = default;

Result<Engine> Engine::load(const Config &config) {
  auto impl = std::make_unique<Impl>();
  impl->config = config;
  impl->env.set_html_autoescape(false);
  impl->env.set_search_included_templates_in_files(false);

  for (const auto &item : kEmbedded) {
    const auto where = std::filesystem::path{"themes/default"} / std::string{item.name};
    auto installed =
        install_template(impl->env, impl->layouts, std::string{item.name}, item.source, where);
    if (!installed) {
      return tl::unexpected(installed.error());
    }
  }

  const auto override_dir = config.source_root / "templates";
  std::error_code ec;
  if (std::filesystem::is_directory(override_dir, ec)) {
    for (const auto &entry : std::filesystem::directory_iterator(override_dir, ec)) {
      if (ec) {
        return tl::unexpected(
            make_error(ErrorCode::Io,
                       std::format("{}: テンプレートを走査できません: {}",
                                   util::to_generic_utf8(override_dir), ec.message()),
                       override_dir));
      }
      if (!entry.is_regular_file() || entry.path().extension() != ".html") {
        continue;
      }
      auto text = util::read_utf8_file(entry.path());
      if (!text) {
        return tl::unexpected(text.error());
      }
      auto installed = install_template(
          impl->env, impl->layouts, util::to_utf8(entry.path().filename()), *text, entry.path());
      if (!installed) {
        return tl::unexpected(installed.error());
      }
    }
  }

  return Engine{std::move(impl)};
}

Result<RenderedPage> Engine::render(const Document &document) const {
  const auto filename = document.front_matter.layout + ".html";
  const auto found = impl_->layouts.find(filename);
  if (found == impl_->layouts.end()) {
    return tl::unexpected(make_error(ErrorCode::Template,
                                     std::format("{}: テンプレート '{}' がありません",
                                                 util::to_generic_utf8(document.source), filename),
                                     document.source));
  }

  try {
    const auto data = make_context(impl_->config, document);
    RenderedPage page;
    page.output_path = document.output_path;
    page.html = impl_->env.render(found->second, data);
    return page;
  } catch (const inja::InjaError &ex) {
    const int line = inja_line(ex.location);
    return tl::unexpected(
        make_error(ErrorCode::Template,
                   std::format("{}:{} テンプレート '{}' を適用できません: {}",
                               util::to_generic_utf8(document.source), line, filename, ex.message),
                   document.source, line));
  }
}

} // namespace kappan::render
