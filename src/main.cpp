#include <kappan/version.hpp>

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>

int main(int argc, char **argv) {
  try {
    CLI::App app{"Kappan — static site generator"};
    app.set_version_flag("--version,-v", kappan::version_string());
    CLI11_PARSE(app, argc, argv);
    return EXIT_SUCCESS;
  } catch (const std::exception &ex) {
    std::cerr << std::format("{}\n", ex.what());
    return EXIT_FAILURE;
  }
}
