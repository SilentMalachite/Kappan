#include <kappan/version.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version_string matches CMake project version") {
  // 期待値は tests/CMakeLists.txt が PROJECT_VERSION から渡す。
  // 生成された version.hpp が古いままだとここで落ちる。
  REQUIRE(kappan::version_string() == KAPPAN_EXPECTED_VERSION);
}
