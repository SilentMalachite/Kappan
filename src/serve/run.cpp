#include "serve/run.hpp"

#include "serve/http.hpp"
#include "serve/publish.hpp"
#include "serve/watch.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
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

void wait_poll_interval(std::stop_token stop, std::chrono::milliseconds interval) {
  if (interval <= std::chrono::milliseconds{0}) {
    std::this_thread::yield();
    return;
  }
  auto remaining = interval;
  while (remaining > std::chrono::milliseconds{0} && !stop.stop_requested()) {
    const auto step = std::min(remaining, std::chrono::milliseconds{50});
    std::this_thread::sleep_for(step);
    remaining -= step;
  }
}

void poll_watch(GenerationStore &store, WatchState &state, WatchDebounce &debounce,
                const ServeOptions &options) {
  auto snap = snapshot_source(options.source);
  if (!snap) {
    spdlog::error("{}", snap.error().message);
    return;
  }
  state.observe(*snap);
  if (!state.should_attempt()) {
    return;
  }
  if (!debounce.quiet_elapsed(std::chrono::steady_clock::now(), *snap)) {
    return;
  }
  const auto attempt = state.begin_attempt();
  apply_watch_attempt(state, store, attempt, {.source = options.source, .drafts = options.drafts});
}

void run_watch_loop(std::stop_token stop, GenerationStore &store, SourceSnapshot published,
                    ServeOptions options) {
  WatchDebounce debounce{options.quiet_period};
  (void)debounce.quiet_elapsed(std::chrono::steady_clock::now(), published);
  WatchState state{std::move(published)};
  while (!stop.stop_requested()) {
    poll_watch(store, state, debounce, options);
    wait_poll_interval(stop, options.poll_interval);
  }
}

} // namespace

struct ServeSession::Impl {
  GenerationStore store;
  std::optional<HttpServer> server;
  std::jthread watch;

  explicit Impl(GenerationStore created) : store(std::move(created)) {}
  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

  ~Impl() {
    request_stop();
    (void)join();
  }

  void request_stop() {
    if (server) {
      server->request_stop();
    }
    if (watch.joinable()) {
      watch.request_stop();
    }
  }

  Result<void> join() {
    Result<void> http_result{};
    if (server) {
      http_result = server->wait();
    }
    if (watch.joinable()) {
      watch.join();
    }
    return http_result;
  }
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

  auto server = HttpServer::start(
      impl->store, {.host = options.host, .port = options.port, .inject_reload = options.watch});
  if (!server) {
    return tl::unexpected(server.error());
  }
  impl->server.emplace(std::move(*server));

  if (options.watch) {
    auto *store = &impl->store;
    impl->watch =
        std::jthread([store, published = attempt.snapshot, options](std::stop_token stop) {
          run_watch_loop(stop, *store, published, options);
        });
  }
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
  if (impl_) {
    impl_->request_stop();
  }
}

Result<void> ServeSession::wait() {
  if (!impl_) {
    return {};
  }
  return impl_->join();
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
