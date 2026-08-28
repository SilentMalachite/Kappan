cmake_minimum_required(VERSION 3.28)

project(kappan VERSION 0.1.0 LANGUAGES CXX)

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
  src/util/datetime.cpp
  src/config/load.cpp
  src/markdown/cmark.cpp
  src/content/convert.cpp
  src/content/scan.cpp
  src/content/parse.cpp
  src/content/build.cpp
  src/content/scaffold.cpp
  src/site/build.cpp
  src/site/paginate.cpp
  src/render/escape.cpp
  src/render/context.cpp
  src/render/engine.cpp
  src/output/xml.cpp
  src/output/write.cpp)
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

target_compile_definitions(kappan_lib PUBLIC CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE)

target_link_libraries(
  kappan_lib
  PUBLIC tl::expected spdlog::spdlog libcmark-gfm-extensions_static libcmark-gfm_static
  PRIVATE yaml-cpp::yaml-cpp pantor::inja nlohmann_json::nlohmann_json)

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
