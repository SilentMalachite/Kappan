#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <string>

namespace kappan {

struct Config {
  std::string title;
  std::string url;
  std::string language{"ja"};
  std::string description;
  std::filesystem::path source_root;
  std::filesystem::path content_dir;
};

namespace config {

[[nodiscard]] Result<Config> load(const std::filesystem::path &site_yaml);

} // namespace config
} // namespace kappan
