cmake_minimum_required(VERSION 3.28)

project(kappan VERSION 0.1.0 LANGUAGES CXX)

if(WIN32 AND NOT MSVC)
  message(
    FATAL_ERROR
    "Windows では MSVC を使う。検出されたコンパイラ: ${CMAKE_CXX_COMPILER_ID}。"
    "MinGW / Clang は vcpkg の x64-windows triplet（MSVC ABI）と一致せず、"
    "Windows SDK 依存のヘッダも通らない")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KAPPAN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(CLI11 CONFIG REQUIRED)
find_package(tl-expected CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(cmark-gfm CONFIG REQUIRED)
find_package(cmark-gfm-extensions CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(inja CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

set(KAPPAN_THEME_DIR ${CMAKE_SOURCE_DIR}/themes/default)
set(KAPPAN_EMBEDDED_THEME ${CMAKE_BINARY_DIR}/include/kappan/embedded_theme.hpp)
set_property(
  DIRECTORY
  APPEND
  PROPERTY CMAKE_CONFIGURE_DEPENDS
           ${KAPPAN_THEME_DIR}/base.html
           ${KAPPAN_THEME_DIR}/post.html
           ${KAPPAN_THEME_DIR}/page.html
           ${KAPPAN_THEME_DIR}/index.html
           ${KAPPAN_THEME_DIR}/tag.html
           ${KAPPAN_THEME_DIR}/landing.html
           ${CMAKE_SOURCE_DIR}/cmake/embed_theme.cmake)
execute_process(
  COMMAND ${CMAKE_COMMAND} -DINPUT_DIR=${KAPPAN_THEME_DIR} -DOUTPUT=${KAPPAN_EMBEDDED_THEME} -P
          ${CMAKE_SOURCE_DIR}/cmake/embed_theme.cmake
  COMMAND_ERROR_IS_FATAL ANY)

add_library(
  kappan_lib STATIC
  src/util/version.cpp
  src/util/path.cpp
  src/util/utf8.cpp
  src/util/slug.cpp
  src/util/escape.cpp
  src/util/url.cpp
  src/util/datetime.cpp
  src/config/load.cpp
  src/markdown/cmark.cpp
  src/content/convert.cpp
  src/content/scan.cpp
  src/content/front_matter_yaml.cpp
  src/content/parse.cpp
  src/content/build.cpp
  src/content/scaffold.cpp
  src/site/build.cpp
  src/site/paginate.cpp
  src/render/escape.cpp
  src/render/context.cpp
  src/render/engine.cpp
  src/output/xml.cpp
  src/output/write.cpp
  src/output/assets.cpp
  src/serve/watch.cpp
  src/serve/publish.cpp
  src/serve/http.cpp
  src/serve/run.cpp)
add_library(kappan::lib ALIAS kappan_lib)

configure_file(
  ${CMAKE_SOURCE_DIR}/include/kappan/version.hpp.in
  ${CMAKE_BINARY_DIR}/include/kappan/version.hpp
  @ONLY)

target_include_directories(
  kappan_lib
  PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include
  PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_features(kappan_lib PUBLIC cxx_std_20)

# cmark-gfm は triplet のリンク形態に応じて静的か動的のどちらか一方しか
# ビルドされず、出てくるインポートターゲット名も変わる。
# 静的（*-osx / x64-linux / x64-windows-static）は libcmark-gfm_static、
# 動的（x64-windows）は libcmark-gfm。存在する側を選ぶ。
if(TARGET libcmark-gfm_static)
  set(KAPPAN_CMARK_LIBS libcmark-gfm-extensions_static libcmark-gfm_static)
  # 静的ライブラリのヘッダに __declspec(dllimport) を付けさせない。
  set(KAPPAN_CMARK_DEFINES CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)
elseif(TARGET libcmark-gfm)
  set(KAPPAN_CMARK_LIBS libcmark-gfm-extensions libcmark-gfm)
  set(KAPPAN_CMARK_DEFINES "")
else()
  message(
    FATAL_ERROR
    "cmark-gfm のインポートターゲットが見つかりません。"
    "libcmark-gfm_static と libcmark-gfm のどちらも定義されていない")
endif()

target_compile_definitions(kappan_lib PUBLIC ${KAPPAN_CMARK_DEFINES})

target_link_libraries(
  kappan_lib
  PUBLIC tl::expected spdlog::spdlog ${KAPPAN_CMARK_LIBS}
  PRIVATE yaml-cpp::yaml-cpp pantor::inja nlohmann_json::nlohmann_json httplib::httplib)

function(kappan_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
    if(KAPPAN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    if(KAPPAN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

kappan_set_warnings(kappan_lib)

add_executable(kappan src/main.cpp)
target_link_libraries(kappan PRIVATE kappan_lib CLI11::CLI11)
target_include_directories(kappan PRIVATE ${CMAKE_SOURCE_DIR}/src)
kappan_set_warnings(kappan)

enable_testing()
add_subdirectory(tests)

find_program(
  CLANG_FORMAT_EXE
  NAMES clang-format clang-format-18 clang-format-17
  HINTS /opt/homebrew/opt/llvm/bin /usr/local/opt/llvm/bin)

if(CLANG_FORMAT_EXE)
  file(
    GLOB_RECURSE
    KAPPAN_FORMAT_FILES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/*.hpp.in
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
    ${CMAKE_SOURCE_DIR}/tests/*.hpp)
  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_EXE} -i ${KAPPAN_FORMAT_FILES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format")
else()
  add_custom_target(
    format
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found; install it and reconfigure"
    COMMENT "clang-format not found")
endif()
