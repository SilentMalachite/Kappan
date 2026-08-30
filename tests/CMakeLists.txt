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
  unit/test_serve_watch.cpp
  unit/test_serve_publish.cpp
  unit/test_serve_http.cpp
  unit/test_serve_run.cpp
  golden/test_golden.cpp)
target_link_libraries(kappan_tests PRIVATE kappan_lib Catch2::Catch2WithMain httplib::httplib)
target_include_directories(kappan_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
kappan_set_warnings(kappan_tests)

include(Catch)
catch_discover_tests(kappan_tests)

# 実行ファイル実体で examples/ を生成し、golden と突き合わせる。
# ライブラリ経由の golden テストが通らない CLI 経路を被覆する。
add_test(
  NAME dist_smoke
  COMMAND
    ${CMAKE_COMMAND} -DKAPPAN_EXE=$<TARGET_FILE:kappan> -DREPO_ROOT=${CMAKE_SOURCE_DIR}
    -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/dist_smoke -P
    ${CMAKE_SOURCE_DIR}/cmake/dist_smoke.cmake)

# 配布ビルドのときだけ、依存ライブラリが許可リスト内に収まっているかを検証する。
if(KAPPAN_DIST)
  add_test(
    NAME dist_selfcontained
    COMMAND ${CMAKE_COMMAND} -DKAPPAN_EXE=$<TARGET_FILE:kappan> -P
            ${CMAKE_SOURCE_DIR}/cmake/dist_selfcontained.cmake)
endif()
