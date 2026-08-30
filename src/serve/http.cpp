#include "serve/http.hpp"

#include "output/write.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <httplib.h>

#include <atomic>
#include <cstdint>
#include <exception>
#include <format>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace kappan::serve {
namespace {

[[nodiscard]] Error path_error(std::string message) {
  return make_error(ErrorCode::Path, std::move(message));
}

[[nodiscard]] bool contains_char(std::string_view text, char needle) {
  return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] std::optional<unsigned> hex_nibble(char ch) {
  if (ch >= '0' && ch <= '9') {
    return static_cast<unsigned>(ch - '0');
  }
  if (ch >= 'a' && ch <= 'f') {
    return static_cast<unsigned>(ch - 'a' + 10);
  }
  if (ch >= 'A' && ch <= 'F') {
    return static_cast<unsigned>(ch - 'A' + 10);
  }
  return std::nullopt;
}

[[nodiscard]] Result<std::string> percent_decode(std::string_view encoded) {
  std::string out;
  out.reserve(encoded.size());
  for (std::size_t i = 0; i < encoded.size(); ++i) {
    if (encoded[i] != '%') {
      out.push_back(encoded[i]);
      continue;
    }
    if (i + 2 >= encoded.size()) {
      return tl::unexpected(path_error("リクエストパスの percent-encoding が途中で終わっています"));
    }
    const auto hi = hex_nibble(encoded[i + 1]);
    const auto lo = hex_nibble(encoded[i + 2]);
    if (!hi || !lo) {
      return tl::unexpected(
          path_error(std::format("リクエストパスの percent-encoding が無効です: '%{}{}'",
                                 encoded[i + 1], encoded[i + 2])));
    }
    out.push_back(static_cast<char>((*hi << 4) | *lo));
    i += 2;
  }
  return out;
}

// RFC 3986 の unreserved。パスセグメントはこれ以外をすべて percent-encode する。
[[nodiscard]] bool is_unreserved_byte(unsigned char ch) {
  return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
         ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

// RFC 3986 の query に現れてよいバイト（pchar / '/' / '?'）に '%' を加えたもの。
// '%' を通すのは、すでに percent-encode 済みのクエリを二重に encode しないため。
[[nodiscard]] bool is_query_byte(unsigned char ch) {
  return is_unreserved_byte(ch) || ch == '%' || ch == '/' || ch == '?' || ch == ':' || ch == '@' ||
         ch == '!' || ch == '$' || ch == '&' || ch == '\'' || ch == '(' || ch == ')' || ch == '*' ||
         ch == '+' || ch == ',' || ch == ';' || ch == '=';
}

[[nodiscard]] std::string percent_encode(std::string_view raw, bool (*allowed)(unsigned char)) {
  std::string out;
  out.reserve(raw.size());
  for (const char byte : raw) {
    const auto ch = static_cast<unsigned char>(byte);
    if (allowed(ch)) {
      out.push_back(static_cast<char>(ch));
    } else {
      out += std::format("%{:02X}", static_cast<unsigned>(ch));
    }
  }
  return out;
}

// 解決に使ったセグメントから組み立てる。空セグメントが落ちるので、先頭 '//' が
// protocol-relative URL として解釈される Location にならない。
[[nodiscard]] std::string make_redirect_location(const std::vector<std::string> &segments) {
  std::string out{"/"};
  for (const auto &segment : segments) {
    out += percent_encode(segment, is_unreserved_byte);
    out.push_back('/');
  }
  return out;
}

[[nodiscard]] bool is_windows_drive_segment(std::string_view segment) {
  if (segment.size() < 2 || segment[1] != ':') {
    return false;
  }
  const char letter = segment[0];
  return (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z');
}

[[nodiscard]] Result<std::vector<std::string>> split_segments(std::string_view decoded) {
  std::vector<std::string> segments;
  std::size_t start = 1;
  for (std::size_t i = 1; i <= decoded.size(); ++i) {
    if (i != decoded.size() && decoded[i] != '/') {
      continue;
    }
    const auto segment = decoded.substr(start, i - start);
    start = i + 1;
    if (segment.empty()) {
      continue;
    }
    if (segment == "." || segment == "..") {
      return tl::unexpected(path_error(
          std::format("{}: リクエストパスに '.' または '..' が含まれています", decoded)));
    }
    if (is_windows_drive_segment(segment)) {
      return tl::unexpected(path_error(
          std::format("{}: リクエストパスに Windows ドライブが含まれています", decoded)));
    }
    segments.emplace_back(segment);
  }
  return segments;
}

struct SplitTarget {
  std::string_view path_part{};
  std::string_view query_suffix{}; // 先頭の '?' を含む。クエリが空なら空。
};

[[nodiscard]] SplitTarget split_target(std::string_view raw_target) {
  const auto query_pos = raw_target.find('?');
  if (query_pos == std::string_view::npos) {
    return SplitTarget{.path_part = raw_target};
  }
  const auto suffix = raw_target.substr(query_pos);
  // '?' だけの target はクエリを持たない URL と同じ。Location に '?' を残さない。
  return SplitTarget{.path_part = raw_target.substr(0, query_pos),
                     .query_suffix = suffix.size() == 1 ? std::string_view{} : suffix};
}

struct ParsedTarget {
  std::string decoded;
  std::string query_suffix;
  std::vector<std::string> segments;
  std::filesystem::path relative;
  bool trailing_slash = false;
};

[[nodiscard]] Result<ParsedTarget> parse_request_target(std::string_view raw_target) {
  if (contains_char(raw_target, '\0') || contains_char(raw_target, '\\') ||
      contains_char(raw_target, '\r') || contains_char(raw_target, '\n')) {
    return tl::unexpected(
        path_error("リクエストパスに NUL、バックスラッシュ、CR または LF が含まれています"));
  }
  const auto [path_part, raw_query] = split_target(raw_target);
  auto decoded = percent_decode(path_part);
  if (!decoded) {
    return tl::unexpected(decoded.error());
  }
  if (contains_char(*decoded, '\0') || contains_char(*decoded, '\\')) {
    return tl::unexpected(
        path_error("リクエストパスに NUL またはバックスラッシュが含まれています"));
  }
  if (!util::is_valid_utf8(*decoded)) {
    return tl::unexpected(path_error("リクエストパスが UTF-8 として解釈できません"));
  }
  if (decoded->empty() || decoded->front() != '/') {
    return tl::unexpected(path_error("リクエストパスは '/' で始まる必要があります"));
  }

  auto segments = split_segments(*decoded);
  if (!segments) {
    return tl::unexpected(segments.error());
  }

  ParsedTarget parsed;
  parsed.trailing_slash = decoded->ends_with('/');
  parsed.decoded = std::move(*decoded);
  // Location ヘッダは field-value でなければ httplib に黙って捨てられる。制御文字や
  // 非 ASCII が素通りしないよう、クエリも percent-encode してから持ち回る。
  parsed.query_suffix = percent_encode(raw_query, is_query_byte);
  parsed.segments = std::move(*segments);
  for (const auto &segment : parsed.segments) {
    parsed.relative /= util::from_utf8(segment);
  }
  if (parsed.relative.has_root_path()) {
    return tl::unexpected(path_error(
        std::format("{}: リクエストパスが絶対パスとして解釈されました", parsed.decoded)));
  }
  return parsed;
}

[[nodiscard]] bool hides_marker(const std::filesystem::path &relative) {
  for (const auto &part : relative) {
    if (util::to_utf8(part) == output::kOutMarker) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool contains_dotdot(const std::filesystem::path &rel) {
  for (const auto &part : rel) {
    if (part == "..") {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Result<std::filesystem::path> weakly_abs(const std::filesystem::path &path) {
  std::error_code abs_ec;
  const auto absolute = std::filesystem::absolute(path, abs_ec);
  if (abs_ec) {
    return tl::unexpected(path_error(std::format("{}: パスを解決できません: {}",
                                                 util::to_generic_utf8(path), abs_ec.message())));
  }
  std::error_code canon_ec;
  const auto canonical = std::filesystem::weakly_canonical(absolute, canon_ec);
  if (canon_ec) {
    return tl::unexpected(path_error(std::format("{}: パスを解決できません: {}",
                                                 util::to_generic_utf8(path), canon_ec.message())));
  }
  return canonical;
}

[[nodiscard]] Result<void> ensure_inside(const std::filesystem::path &root,
                                         const std::filesystem::path &relative) {
  if (relative.empty() || relative.has_root_path() || contains_dotdot(relative)) {
    return tl::unexpected(path_error(
        std::format("{}: 生成ルートの外を参照しています", util::to_generic_utf8(relative))));
  }
  const auto root_abs = weakly_abs(root);
  if (!root_abs) {
    return tl::unexpected(root_abs.error());
  }
  const auto dest_abs = weakly_abs(root / relative);
  if (!dest_abs) {
    return tl::unexpected(dest_abs.error());
  }
  const auto rel = dest_abs->lexically_relative(*root_abs);
  if (rel.empty() || rel.has_root_path() || contains_dotdot(rel)) {
    return tl::unexpected(path_error(
        std::format("{}: 生成ルートの外を参照しています", util::to_generic_utf8(relative))));
  }
  return {};
}

[[nodiscard]] Result<bool> is_regular_inside(const std::filesystem::path &root,
                                             const std::filesystem::path &relative) {
  const auto inside = ensure_inside(root, relative);
  if (!inside) {
    return tl::unexpected(inside.error());
  }
  std::error_code ec;
  return std::filesystem::is_regular_file(root / relative, ec) && !ec;
}

[[nodiscard]] std::string ascii_lower_extension(const std::filesystem::path &path) {
  auto ext = util::to_utf8(path.extension());
  if (ext.starts_with('.')) {
    ext.erase(0, 1);
  }
  for (char &ch : ext) {
    if (ch >= 'A' && ch <= 'Z') {
      ch = static_cast<char>(ch - 'A' + 'a');
    }
  }
  return ext;
}

} // namespace

std::string content_type_for(const std::filesystem::path &path) {
  struct Mime {
    std::string_view ext;
    std::string_view type;
  };
  static constexpr Mime kMimes[] = {
      {"html", "text/html; charset=utf-8"},
      {"css", "text/css; charset=utf-8"},
      {"js", "text/javascript; charset=utf-8"},
      {"json", "application/json; charset=utf-8"},
      {"xml", "application/xml; charset=utf-8"},
      {"svg", "image/svg+xml; charset=utf-8"},
      {"txt", "text/plain; charset=utf-8"},
      {"png", "image/png"},
      {"jpg", "image/jpeg"},
      {"jpeg", "image/jpeg"},
      {"gif", "image/gif"},
      {"webp", "image/webp"},
      {"ico", "image/x-icon"},
      {"wasm", "application/wasm"},
  };
  const auto ext = ascii_lower_extension(path);
  for (const auto &mime : kMimes) {
    if (ext == mime.ext) {
      return std::string{mime.type};
    }
  }
  return "application/octet-stream";
}

Result<ResolvedRequest> resolve_request_path(const std::filesystem::path &root,
                                             std::string_view raw_target) {
  auto parsed = parse_request_target(raw_target);
  if (!parsed) {
    return tl::unexpected(parsed.error());
  }
  if (hides_marker(parsed->relative)) {
    return ResolvedRequest{.kind = ResolveKind::NotFound};
  }

  const auto index = parsed->relative / "index.html";
  if (parsed->trailing_slash || parsed->relative.empty()) {
    const auto servable = is_regular_inside(root, index);
    if (!servable) {
      return tl::unexpected(servable.error());
    }
    if (*servable) {
      return ResolvedRequest{.kind = ResolveKind::File, .file = index};
    }
    return ResolvedRequest{.kind = ResolveKind::NotFound};
  }

  const auto as_file = is_regular_inside(root, parsed->relative);
  if (!as_file) {
    return tl::unexpected(as_file.error());
  }
  if (*as_file) {
    return ResolvedRequest{.kind = ResolveKind::File, .file = parsed->relative};
  }

  const auto as_dir = is_regular_inside(root, index);
  if (!as_dir) {
    return tl::unexpected(as_dir.error());
  }
  if (*as_dir) {
    return ResolvedRequest{.kind = ResolveKind::Redirect,
                           .location =
                               make_redirect_location(parsed->segments) + parsed->query_suffix};
  }
  return ResolvedRequest{.kind = ResolveKind::NotFound};
}

namespace {

constexpr std::string_view kHtmlType = "text/html; charset=utf-8";
constexpr std::string_view kPlainType = "text/plain; charset=utf-8";
constexpr std::string_view kReloadPath = "/__kappan/reload";
constexpr std::string_view kBodyClose = "</body>";
constexpr std::string_view kPage400 =
    "<!DOCTYPE html><title>400</title><p>リクエストが正しくありません</p>";
constexpr std::string_view kPage404 =
    "<!DOCTYPE html><title>404</title><p>ページが見つかりません</p>";
constexpr std::string_view kPage405 =
    "<!DOCTYPE html><title>405</title><p>このメソッドは使えません</p>";
constexpr std::string_view kPage500 = "<!DOCTYPE html><title>500</title><p>配信できません</p>";

[[nodiscard]] bool is_reload_target(std::string_view raw_target) {
  auto decoded = percent_decode(split_target(raw_target).path_part);
  if (!decoded) {
    return false;
  }
  return *decoded == kReloadPath;
}

[[nodiscard]] std::string make_reload_script(std::uint64_t generation) {
  return std::string{"<script>(function(){var g=\""} + std::format("{}", generation) +
         "\";setInterval(function(){"
         "fetch('/__kappan/reload', {cache: 'no-store'})"
         ".then(function(r){return r.ok?r.text():Promise.reject();})"
         ".then(function(t){if(/^[0-9]+$/.test(t)&&t!==g)location.reload();})"
         ".catch(function(){});},250);})();</script>";
}

[[nodiscard]] std::string inject_reload_script(std::string html, std::uint64_t generation) {
  const auto script = make_reload_script(generation);
  const auto pos = html.rfind(kBodyClose);
  if (pos == std::string::npos) {
    html.append(script);
    return html;
  }
  html.insert(pos, script);
  return html;
}

void maybe_inject_reload(httplib::Response &res, std::uint64_t generation) {
  if (res.status != httplib::StatusCode::OK_200) {
    return;
  }
  if (!res.get_header_value("Content-Type").starts_with("text/html")) {
    return;
  }
  res.body = inject_reload_script(std::move(res.body), generation);
}

void handle_reload(GenerationStore &store, httplib::Response &res) {
  res.status = httplib::StatusCode::OK_200;
  res.set_header("Cache-Control", "no-store");
  res.set_content(std::format("{}", store.generation()), std::string{kPlainType});
}

void send_html(httplib::Response &res, int status, std::string_view body) {
  res.status = status;
  res.set_content(body.data(), body.size(), std::string{kHtmlType});
}

void send_file_body(httplib::Response &res, const ByteBuffer &bytes, const std::string &type) {
  res.status = httplib::StatusCode::OK_200;
  res.set_header("Cache-Control", "no-cache");
  if (bytes.empty()) {
    res.set_content(std::string{}, type);
    return;
  }
  const auto *ptr = reinterpret_cast<const char *>(bytes.data());
  if (type.starts_with("text/html")) {
    res.set_content(std::string(ptr, bytes.size()), type);
    return;
  }
  res.set_content(ptr, bytes.size(), type);
}

void handle_get(GenerationStore &store, const httplib::Request &req, httplib::Response &res,
                bool inject_reload) {
  auto lease = store.acquire_read();
  if (!lease) {
    send_html(res, httplib::StatusCode::InternalServerError_500, kPage500);
    return;
  }
  // req.path is already decoded; a second decode would turn %252e into Path/400.
  auto resolved = resolve_request_path(lease->root(), req.target);
  if (!resolved) {
    send_html(res, httplib::StatusCode::BadRequest_400, kPage400);
    return;
  }
  if (resolved->kind == ResolveKind::NotFound) {
    send_html(res, httplib::StatusCode::NotFound_404, kPage404);
    return;
  }
  if (resolved->kind == ResolveKind::Redirect) {
    res.set_redirect(resolved->location, httplib::StatusCode::MovedPermanently_301);
    res.set_header("Cache-Control", "no-cache");
    return;
  }
  auto bytes = lease->read_bytes(resolved->file);
  if (!bytes) {
    send_html(res, httplib::StatusCode::InternalServerError_500, kPage500);
    return;
  }
  send_file_body(res, *bytes, content_type_for(resolved->file));
  if (inject_reload) {
    maybe_inject_reload(res, lease->generation());
  }
}

[[nodiscard]] Error bind_error(const HttpServerOptions &options) {
  return make_error(ErrorCode::Io,
                    std::format("{}:{}: 待ち受けできません", options.host, options.port));
}

[[nodiscard]] Result<std::uint16_t> bind_port(httplib::Server &svr,
                                              const HttpServerOptions &options) {
  if (options.port == 0) {
    const int bound = svr.bind_to_any_port(options.host);
    if (bound <= 0 || bound > 65535) {
      return tl::unexpected(bind_error(options));
    }
    return static_cast<std::uint16_t>(bound);
  }
  if (!svr.bind_to_port(options.host, static_cast<int>(options.port))) {
    return tl::unexpected(bind_error(options));
  }
  return options.port;
}

[[nodiscard]] std::string describe_exception(std::exception_ptr captured) {
  if (!captured) {
    return "不明な例外";
  }
  try {
    std::rethrow_exception(captured);
  } catch (const std::exception &ex) {
    return ex.what();
  } catch (...) {
    return "不明な例外";
  }
}

void install_handlers(httplib::Server &svr, GenerationStore &store, bool inject_reload) {
  // Serve GET/HEAD here so long decoded paths are not limited by
  // CPPHTTPLIB_REGEX_ROUTE_PATH_MAX_LENGTH on RegexMatcher routes.
  svr.set_pre_routing_handler(
      [&store, inject_reload](const httplib::Request &req, httplib::Response &res) {
        if (req.method == "GET" || req.method == "HEAD") {
          if (inject_reload && is_reload_target(req.target)) {
            handle_reload(store, res);
            return httplib::Server::HandlerResponse::Handled;
          }
          handle_get(store, req, res, inject_reload);
          return httplib::Server::HandlerResponse::Handled;
        }
        res.set_header("Allow", "GET, HEAD");
        send_html(res, httplib::StatusCode::MethodNotAllowed_405, kPage405);
        return httplib::Server::HandlerResponse::Handled;
      });
  // 例外は 500 応答へ変換するが、原因を捨てない。捨てると、どのパスの配信で何が
  // 起きたのかを開発者が知る手段がなくなる。
  svr.set_exception_handler(
      [](const httplib::Request &req, httplib::Response &res, std::exception_ptr captured) {
        std::cerr << std::format("{}: 配信中に例外が発生しました: {}\n", req.target,
                                 describe_exception(captured));
        send_html(res, httplib::StatusCode::InternalServerError_500, kPage500);
      });
}

} // namespace

struct HttpServer::Impl {
  httplib::Server svr;
  std::thread listen_thread;
  std::atomic<bool> listen_ok{true};
  // listen_thread だけが書き、join 後にだけ読む。join が happens-before を作る。
  std::string listen_error;
  std::string host;
  std::uint16_t port = 0;
};

HttpServer::HttpServer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
HttpServer::HttpServer(HttpServer &&) noexcept = default;

HttpServer &HttpServer::operator=(HttpServer &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  request_stop();
  if (impl_ && impl_->listen_thread.joinable()) {
    impl_->listen_thread.join();
  }
  impl_ = std::move(other.impl_);
  return *this;
}

HttpServer::~HttpServer() {
  request_stop();
  if (impl_ && impl_->listen_thread.joinable()) {
    impl_->listen_thread.join();
  }
}

Result<HttpServer> HttpServer::start(GenerationStore &store, const HttpServerOptions &options) {
  auto impl = std::make_unique<Impl>();
  impl->svr.set_idle_interval(0, 50000);
  install_handlers(impl->svr, store, options.inject_reload);

  auto bound = bind_port(impl->svr, options);
  if (!bound) {
    return tl::unexpected(bound.error());
  }
  impl->host = options.host;
  impl->port = *bound;

  auto *svr = &impl->svr;
  auto *listen_ok = &impl->listen_ok;
  auto *listen_error = &impl->listen_error;
  impl->listen_thread = std::thread([svr, listen_ok, listen_error]() {
    // thread 境界を越えられない例外を失敗状態へ変換し、join 後に呼び出し側の Result で返す。
    try {
      listen_ok->store(svr->listen_after_bind());
    } catch (...) {
      *listen_error = describe_exception(std::current_exception());
      listen_ok->store(false);
      // stop() は最後に is_decommissioned を false へ戻す。wait_until_ready() が見る
      // のはこのフラグなので、decommission() を後から呼ばないと待ち側が抜けられない。
      svr->stop();
      svr->decommission();
    }
  });

  svr->wait_until_ready();
  if (!svr->is_running()) {
    svr->stop();
    if (impl->listen_thread.joinable()) {
      impl->listen_thread.join();
    }
    if (!impl->listen_error.empty()) {
      return tl::unexpected(
          make_error(ErrorCode::Io, std::format("{}:{}: 待ち受けを開始できません: {}", impl->host,
                                                impl->port, impl->listen_error)));
    }
    return tl::unexpected(bind_error(options));
  }
  return HttpServer{std::move(impl)};
}

std::uint16_t HttpServer::port() const { return impl_ ? impl_->port : std::uint16_t{0}; }

bool HttpServer::running() const { return impl_ && impl_->svr.is_running(); }

void HttpServer::request_stop() {
  if (impl_) {
    impl_->svr.stop();
  }
}

Result<void> HttpServer::wait() {
  if (impl_ && impl_->listen_thread.joinable()) {
    impl_->listen_thread.join();
  }
  if (impl_ && !impl_->listen_ok.load()) {
    if (!impl_->listen_error.empty()) {
      return tl::unexpected(
          make_error(ErrorCode::Io, std::format("{}:{}: HTTP サーバーの待ち受けが失敗しました: {}",
                                                impl_->host, impl_->port, impl_->listen_error)));
    }
    return tl::unexpected(
        make_error(ErrorCode::Io, std::format("{}:{}: HTTP サーバーの待ち受けが失敗しました",
                                              impl_->host, impl_->port)));
  }
  return {};
}

} // namespace kappan::serve
