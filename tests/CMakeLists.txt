find_package(Catch2 3 CONFIG REQUIRED)

add_executable(kappan_tests unit/test_version.cpp unit/test_utf8.cpp unit/test_markdown.cpp
                            unit/test_convert.cpp)
target_link_libraries(kappan_tests PRIVATE kappan_lib Catch2::Catch2WithMain)
target_include_directories(kappan_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
kappan_set_warnings(kappan_tests)

include(Catch)
catch_discover_tests(kappan_tests)
