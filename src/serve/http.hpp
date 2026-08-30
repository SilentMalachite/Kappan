#pragma once

#include "serve/publish.hpp"

#include <kappan/error.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace kappan::serve {

enum class ResolveKind { File, Redirect, NotFound };

struct ResolvedRequest {
  ResolveKind kind = ResolveKind::NotFound;
  std::filesystem::path file{};
  std::string location{};
};

[[nodiscard]] Result<ResolvedRequest> resolve_request_path(const std::filesystem::path &root,
                                                           std::string_view raw_target);
[[nodiscard]] std::string content_type_for(const std::filesystem::path &path);

struct HttpServerOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8080;
  bool inject_reload = false;
};

class HttpServer {
public:
  HttpServer(HttpServer &&) noexcept;
  HttpServer &operator=(HttpServer &&) noexcept;
  ~HttpServer();
  HttpServer(const HttpServer &) = delete;
  HttpServer &operator=(const HttpServer &) = delete;

  [[nodiscard]] static Result<HttpServer> start(GenerationStore &store,
                                                const HttpServerOptions &options);
  [[nodiscard]] std::uint16_t port() const;
  [[nodiscard]] bool running() const;
  void request_stop();
  [[nodiscard]] Result<void> wait();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit HttpServer(std::unique_ptr<Impl> impl);
};

} // namespace kappan::serve
