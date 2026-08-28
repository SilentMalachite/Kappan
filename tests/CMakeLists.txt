find_package(Catch2 3 CONFIG REQUIRED)

add_executable(
  kappan_tests
  unit/test_version.cpp
  unit/test_utf8.cpp
  unit/test_markdown.cpp
  unit/test_convert.cpp
  unit/test_slug.cpp
  unit/test_datetime.cpp
  unit/test_config.cpp
  unit/test_front_matter.cpp
  unit/test_build_site.cpp
  unit/test_render.cpp
  unit/test_scaffold.cpp
  unit/test_site.cpp
  unit/test_output.cpp
  golden/test_golden.cpp)
target_link_libraries(kappan_tests PRIVATE kappan_lib Catch2::Catch2WithMain)
target_include_directories(kappan_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
kappan_set_warnings(kappan_tests)

include(Catch)
catch_discover_tests(kappan_tests)
