#include <kappan/version.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version_string matches CMake project version") {
  REQUIRE(kappan::version_string() == "0.1.0");
}
