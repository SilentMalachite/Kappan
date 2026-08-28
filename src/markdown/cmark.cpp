#include "markdown/cmark.hpp"

#include "util/path.hpp"

#include <cmark-gfm-core-extensions.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm.h>

#include <format>
#include <memory>
#include <string>

namespace kappan::markdown {
namespace {

constexpr int kCmarkOptions =
    CMARK_OPT_UNSAFE | CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE | CMARK_OPT_GITHUB_PRE_LANG;

struct ParserDeleter {
  void operator()(cmark_parser *parser) const {
    if (parser != nullptr) {
      cmark_parser_free(parser);
    }
  }
};

struct NodeDeleter {
  void operator()(cmark_node *node) const {
    if (node != nullptr) {
      cmark_node_free(node);
    }
  }
};

struct CmarkStringDeleter {
  void operator()(char *text) const {
    if (text != nullptr) {
      cmark_get_default_mem_allocator()->free(text);
    }
  }
};

[[nodiscard]] Result<std::unique_ptr<cmark_parser, ParserDeleter>>
make_parser(const std::filesystem::path &where) {
  cmark_gfm_core_extensions_ensure_registered();
  std::unique_ptr<cmark_parser, ParserDeleter> parser{cmark_parser_new(kCmarkOptions)};
  if (!parser) {
    return tl::unexpected(make_error(
        ErrorCode::Markdown,
        std::format("{}: Markdown パーサを初期化できません", util::to_utf8(where)), where));
  }
  constexpr const char *kExtensions[] = {"table", "strikethrough", "autolink", "tasklist",
                                         "tagfilter"};
  for (const char *name : kExtensions) {
    cmark_syntax_extension *ext = cmark_find_syntax_extension(name);
    if (ext == nullptr || cmark_parser_attach_syntax_extension(parser.get(), ext) == 0) {
      return tl::unexpected(make_error(
          ErrorCode::Markdown,
          std::format("{}: GFM 拡張 '{}' を有効にできません", util::to_utf8(where), name), where));
    }
  }
  return parser;
}

} // namespace

Result<std::string> to_html(std::string_view markdown, const std::filesystem::path &where) {
  auto parser = make_parser(where);
  if (!parser) {
    return tl::unexpected(parser.error());
  }
  const char *data = markdown.empty() ? "" : markdown.data();
  cmark_parser_feed(parser->get(), data, markdown.size());
  std::unique_ptr<cmark_node, NodeDeleter> document{cmark_parser_finish(parser->get())};
  if (!document) {
    return tl::unexpected(
        make_error(ErrorCode::Markdown,
                   std::format("{}: Markdown の解析に失敗しました", util::to_utf8(where)), where));
  }
  std::unique_ptr<char, CmarkStringDeleter> rendered{cmark_render_html(
      document.get(), kCmarkOptions, cmark_parser_get_syntax_extensions(parser->get()))};
  if (!rendered) {
    return tl::unexpected(
        make_error(ErrorCode::Markdown,
                   std::format("{}: HTML の生成に失敗しました", util::to_utf8(where)), where));
  }
  return std::string{rendered.get()};
}

} // namespace kappan::markdown
