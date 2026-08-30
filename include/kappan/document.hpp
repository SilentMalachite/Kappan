#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace kappan {

struct LandingAction {
  std::string label;
  std::string href;
};

struct LandingItem {
  std::string title;
  std::string text;
  std::string icon;
};

struct LandingSection {
  std::string type;
  std::string eyebrow;
  std::string title;
  std::string text;
  std::string image;
  std::vector<LandingAction> actions;
  std::vector<LandingItem> items;
};

struct FrontMatter {
  std::string title;
  std::optional<std::chrono::sys_seconds> date;
  std::string layout;
  std::string slug;
  bool draft = false;
  std::vector<std::string> tags;
  std::string description;
  std::string image;
  std::vector<LandingSection> sections;
};

struct Document {
  std::filesystem::path source;
  FrontMatter front_matter;
  std::string body_html;
  std::filesystem::path output_path;
  std::string permalink;
};

} // namespace kappan
