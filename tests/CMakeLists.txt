find_package(Catch2 3 CONFIG REQUIRED)

add_executable(kappan_tests unit/test_version.cpp)
target_link_libraries(kappan_tests PRIVATE kappan_lib Catch2::Catch2WithMain)
kappan_set_warnings(kappan_tests)

include(Catch)
catch_discover_tests(kappan_tests)
