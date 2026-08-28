cmake_minimum_required(VERSION 3.28)

project(kappan VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KAPPAN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

find_package(CLI11 CONFIG REQUIRED)

add_library(kappan_lib STATIC src/util/version.cpp)
add_library(kappan::lib ALIAS kappan_lib)

configure_file(
  ${CMAKE_SOURCE_DIR}/include/kappan/version.hpp.in
  ${CMAKE_BINARY_DIR}/include/kappan/version.hpp
  @ONLY)

target_include_directories(kappan_lib PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)

target_compile_features(kappan_lib PUBLIC cxx_std_20)

function(kappan_set_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
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
