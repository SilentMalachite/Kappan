#include "render/engine.hpp"

#include "render/context.hpp"
#include "site/paginate.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/embedded_theme.hpp>

// inja は throw_parser_error() の直後に break を置くため C4702 が出る。
// C4702 はバックエンドの警告で、CMake が付ける /external:W0 の対象外になる。
// upstream のコードなのでこちらでは直せず、この include の間だけ抜く。
// 自分のコードの C4702 は有効のまま（AGENTS.md §5 の相談を経た措置）。
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <inja/inja.hpp>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <filesystem>
#include <format>
#include <map>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace kappan::render {
namespace {

struct NamedTheme {
  std::string_view name;
  std::string_view source;
};

constexpr NamedTheme kEmbedded[] = {
    {"base.html", embedded::base_html}, {"post.html", embedded::post_html},
    {"page.html", embedded::page_html}, {"index.html", embedded::index_html},
    {"tag.html", embedded::tag_html},   {"landing.html", embedded::landing_html},
};

[[nodiscard]] Error scan_error(const std::filesystem::path &dir, const std::error_code &ec) {
  return make_error(
      ErrorCode::Io,
      std::format("{}: テンプレートを走査できません: {}", util::to_generic_utf8(dir), ec.message()),
      dir);
}

[[nodiscard]] int inja_line(const inja::SourceLocation &location) {
  if (location.line == 0) {
    return 1;
  }
  return static_cast<int>(location.line);
}

[[nodiscard]] Result<RenderedPage>
render_named(inja::Environment &env, const std::map<std::string, inja::Template> &layouts,
             const std::string &filename, const std::filesystem::path &output_path,
             const nlohmann::json &data, const std::filesystem::path &where) {
  const auto found = layouts.find(filename);
  if (found == layouts.end()) {
    return tl::unexpected(make_error(
        ErrorCode::Template,
        std::format("{}: テンプレート '{}' がありません", util::to_generic_utf8(where), filename),
        where));
  }
  try {
    RenderedPage page;
    page.output_path = output_path;
    page.html = env.render(found->second, data);
    return page;
  } catch (const inja::InjaError &ex) {
    const int line = inja_line(ex.location);
    return tl::unexpected(
        make_error(ErrorCode::Template,
                   std::format("{}:{} テンプレート '{}' を適用できません: {}",
                               util::to_generic_utf8(where), line, filename, ex.message),
                   where, line));
  }
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
  inja::Environment env;
  std::map<std::string, inja::Template> layouts;
};

Engine::Engine(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;
Engine::~Engine() = default;

Result<Engine> Engine::load(const Config &config) {
  auto impl = std::make_unique<Impl>();
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
  // 種別の問い合わせは 1 回だけ。is_directory(dir, ec) の ec を捨てると、
  // templates/ 自体の解決失敗（ELOOP 等）を「上書きは無い」と読み違える。
  const auto dir_status = std::filesystem::status(override_dir, ec);
  if (!std::filesystem::status_known(dir_status)) {
    return tl::unexpected(scan_error(override_dir, ec));
  }
  if (std::filesystem::is_directory(dir_status)) {
    auto it = std::filesystem::directory_iterator(override_dir, ec);
    if (ec) {
      return tl::unexpected(scan_error(override_dir, ec));
    }
    // range-for の operator++ は投げるオーバーロードで、構築に失敗した場合は end と
    // 等しくなって本体が 1 度も回らない。increment(ec) を使い、その直後に ec を見る。
    const std::filesystem::directory_iterator end;
    while (it != end) {
      // 投げるオーバーロード（is_regular_file() など）だと、シンボリックリンクの
      // 解決失敗で filesystem_error が Result を貫通する。
      std::error_code type_ec;
      const auto entry_status = it->status(type_ec);
      if (!std::filesystem::status_known(entry_status)) {
        const auto detail = type_ec ? type_ec.message() : std::string{"種別が不明です"};
        return tl::unexpected(make_error(
            ErrorCode::Io,
            std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(it->path()), detail),
            it->path()));
      }
      if (std::filesystem::is_regular_file(entry_status) && it->path().extension() == ".html") {
        auto text = util::read_utf8_file(it->path());
        if (!text) {
          return tl::unexpected(text.error());
        }
        auto installed = install_template(impl->env, impl->layouts,
                                          util::to_utf8(it->path().filename()), *text, it->path());
        if (!installed) {
          return tl::unexpected(installed.error());
        }
      }
      it.increment(ec);
      if (ec) {
        return tl::unexpected(scan_error(override_dir, ec));
      }
    }
  }

  return Engine{std::move(impl)};
}

Result<RenderedPage> Engine::render(const Site &site, const Document &document) const {
  const auto pages = site::paginate(site.posts.indices, site.config.posts_per_page);
  const site::Pagination *pagination = nullptr;
  if (document.permalink == "/" && document.front_matter.layout == "index" && !pages.empty()) {
    pagination = &pages.front();
  }
  return render_named(impl_->env, impl_->layouts, document.front_matter.layout + ".html",
                      document.output_path, make_context(site, document, pagination),
                      document.source);
}

Result<RenderedPage> Engine::render_listing(const Site &site, int page_number) const {
  const auto pages = site::paginate(site.posts.indices, site.config.posts_per_page);
  if (page_number < 1 || page_number > static_cast<int>(pages.size())) {
    return tl::unexpected(
        make_error(ErrorCode::Path, std::format("一覧のページ {} はありません", page_number)));
  }
  const auto &pagination = pages[static_cast<std::size_t>(page_number) - 1];
  return render_named(
      impl_->env, impl_->layouts, "index.html", util::output_from_permalink(pagination.permalink),
      make_listing_context(site, pagination), site.config.source_root / "index.html");
}

Result<RenderedPage> Engine::render_tag(const Site &site, std::string_view tag_slug) const {
  const auto found = std::ranges::find(site.tags.terms, tag_slug, &TaxonomyTerm::slug);
  if (found == site.tags.terms.end()) {
    return tl::unexpected(
        make_error(ErrorCode::Path, std::format("タグ '{}' がありません", tag_slug)));
  }
  return render_named(impl_->env, impl_->layouts, "tag.html",
                      util::output_from_permalink(found->permalink), make_tag_context(site, *found),
                      site.config.source_root / "tags.html");
}

} // namespace kappan::render
