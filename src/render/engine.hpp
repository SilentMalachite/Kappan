#pragma once

#include <kappan/config.hpp>
#include <kappan/document.hpp>
#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace kappan::render {

struct RenderedPage {
  std::filesystem::path output_path;
  std::string html;
};

class Engine {
public:
  Engine(Engine &&) noexcept;
  Engine &operator=(Engine &&) noexcept;
  ~Engine();

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  [[nodiscard]] static Result<Engine> load(const Config &config);

  [[nodiscard]] Result<RenderedPage> render(const Site &site, const Document &document) const;

  [[nodiscard]] Result<RenderedPage> render_listing(const Site &site, int page_number) const;

  [[nodiscard]] Result<RenderedPage> render_tag(const Site &site, std::string_view tag_slug) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit Engine(std::unique_ptr<Impl> impl);
};

} // namespace kappan::render
