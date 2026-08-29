#pragma once

#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace kappan::serve {

struct ServeOptions {
  std::filesystem::path source;
  std::string host = "127.0.0.1";
  std::uint16_t port = 8080;
  DraftPolicy drafts = DraftPolicy::Exclude;
};

class ServeSession {
public:
  ServeSession(ServeSession &&) noexcept;
  ServeSession &operator=(ServeSession &&) noexcept;
  ~ServeSession();
  ServeSession(const ServeSession &) = delete;
  ServeSession &operator=(const ServeSession &) = delete;

  [[nodiscard]] static Result<ServeSession> start(const ServeOptions &options);
  [[nodiscard]] std::uint16_t port() const;
  [[nodiscard]] bool running() const;
  void request_stop();
  [[nodiscard]] Result<void> wait();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit ServeSession(std::unique_ptr<Impl> impl);
};

[[nodiscard]] Result<void> run(const ServeOptions &options);

} // namespace kappan::serve
