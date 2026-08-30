#pragma once

#include <kappan/config.hpp>
#include <kappan/document.hpp>
#include <kappan/error.hpp>

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace kappan::content {

[[nodiscard]] std::string display(const std::filesystem::path &source, const Config &config);

[[nodiscard]] int yaml_file_line(const YAML::Mark &mark, int yaml_start_line);

[[nodiscard]] Result<std::string> scalar_key(const YAML::Node &node, std::string_view key,
                                             const std::filesystem::path &source,
                                             const Config &config, int yaml_start_line);

[[nodiscard]] Result<std::vector<std::string>> read_tags(const YAML::Node &node,
                                                         const std::filesystem::path &source,
                                                         const Config &config, int yaml_start_line);

} // namespace kappan::content
