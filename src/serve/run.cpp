#include "serve/run.hpp"

#include "serve/http.hpp"
#include "serve/publish.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <csignal>
#include <optional>
#include <thread>
#include <utility>

namespace kappan::serve {
namespace {

volatile std::sig_atomic_t g_stop_requested = 0;

void on_stop_signal(int) { g_stop_requested = 1; }

[[nodiscard]] Error publish_failure(const PublishAttempt &attempt) {
  for (const auto &error : attempt.errors) {
    spdlog::error("{}", error.message);
  }
  if (!attempt.errors.empty()) {
    return attempt.errors.front();
  }
  if (attempt.status == PublishStatus::SourceChanged) {
    return make_error(ErrorCode::Io, "初回のサイト生成中にソースが変わりました");
  }
  return make_error(ErrorCode::Io, "初回のサイト生成に失敗しました");
}

class SignalGuard {
public:
  SignalGuard() {
    g_stop_requested = 0;
    previous_int_ = std::signal(SIGINT, on_stop_signal);
#ifndef _WIN32
    previous_term_ = std::signal(SIGTERM, on_stop_signal);
#endif
  }
  SignalGuard(const SignalGuard &) = delete;
  SignalGuard &operator=(const SignalGuard &) = delete;
  ~SignalGuard() {
    std::signal(SIGINT, previous_int_);
#ifndef _WIN32
    std::signal(SIGTERM, previous_term_);
#endif
  }

private:
  using Handler = void (*)(int);
  Handler previous_int_ = SIG_DFL;
#ifndef _WIN32
  Handler previous_term_ = SIG_DFL;
#endif
};

} // namespace

struct ServeSession::Impl {
  GenerationStore store;
  std::optional<HttpServer> server;

  explicit Impl(GenerationStore created) : store(std::move(created)) {}
};

ServeSession::ServeSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ServeSession::ServeSession(ServeSession &&) noexcept = default;
ServeSession &ServeSession::operator=(ServeSession &&) noexcept = default;
ServeSession::~ServeSession() = default;

Result<ServeSession> ServeSession::start(const ServeOptions &options) {
  auto created = GenerationStore::create();
  if (!created) {
    return tl::unexpected(created.error());
  }

  auto impl = std::make_unique<Impl>(std::move(*created));
  const auto attempt = impl->store.publish({.source = options.source, .drafts = options.drafts});
  if (!attempt.ok()) {
    return tl::unexpected(publish_failure(attempt));
  }

  auto server = HttpServer::start(impl->store, {.host = options.host, .port = options.port});
  if (!server) {
    return tl::unexpected(server.error());
  }
  impl->server.emplace(std::move(*server));
  return ServeSession{std::move(impl)};
}

std::uint16_t ServeSession::port() const {
  if (!impl_ || !impl_->server) {
    return 0;
  }
  return impl_->server->port();
}

bool ServeSession::running() const { return impl_ && impl_->server && impl_->server->running(); }

void ServeSession::request_stop() {
  if (impl_ && impl_->server) {
    impl_->server->request_stop();
  }
}

Result<void> ServeSession::wait() {
  if (!impl_ || !impl_->server) {
    return {};
  }
  return impl_->server->wait();
}

Result<void> run(const ServeOptions &options) {
  const SignalGuard signals;
  auto session = ServeSession::start(options);
  if (!session) {
    return tl::unexpected(session.error());
  }
  spdlog::info("{}:{} で配信しています", options.host, session->port());
  while (g_stop_requested == 0 && session->running()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  session->request_stop();
  return session->wait();
}

} // namespace kappan::serve
