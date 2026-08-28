#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kappan {

struct FrontMatter {
  std::string title;
  std::optional<std::chrono::sys_seconds> date;
  std::string layout;
  std::string slug;
  bool draft = false;
  std::vector<std::string> tags;
  std::string description;
};

struct Document {
  std::filesystem::path source;
  FrontMatter front_matter;
  std::string body_html;
  std::filesystem::path output_path;
  std::string permalink;
};

} // namespace kappan
