# 配布バイナリが、対象環境に元から在るライブラリ以外へ依存していないことを検証する。
# 検証ツールが見つからない場合は、黙って検証が消えるのを防ぐため失敗させる。

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED KAPPAN_EXE)
  message(FATAL_ERROR "dist_selfcontained: -DKAPPAN_EXE が指定されていません")
endif()
if(NOT EXISTS "${KAPPAN_EXE}")
  message(FATAL_ERROR "dist_selfcontained: 実行ファイルがありません: ${KAPPAN_EXE}")
endif()

set(deps "")
set(raw "")
set(violations "")

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  find_program(KAPPAN_OTOOL NAMES otool)
  if(NOT KAPPAN_OTOOL)
    message(FATAL_ERROR "dist_selfcontained: otool が見つかりません。検証を飛ばすと自己完結性を保証できません")
  endif()
  execute_process(
    COMMAND "${KAPPAN_OTOOL}" -L "${KAPPAN_EXE}"
    OUTPUT_VARIABLE raw
    ERROR_VARIABLE tool_stderr
    RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "dist_selfcontained: otool -L に失敗しました (終了コード ${code})\n${tool_stderr}")
  endif()
  # 1 行目は対象ファイル自身（先頭タブなし）。依存行は先頭がタブ。
  string(REPLACE "\n" ";" lines "${raw}")
  foreach(line IN LISTS lines)
    if(line MATCHES "^\t([^ ]+) ")
      list(APPEND deps "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  # 実測値（2026-08-30, macOS arm64）は CFNetwork / CoreFoundation /
  # libc++.1.dylib / libSystem.B.dylib の 4 つで、すべて下記の 2 つに収まる。
  foreach(dep IN LISTS deps)
    if(NOT dep MATCHES "^/usr/lib/" AND NOT dep MATCHES "^/System/Library/")
      list(APPEND violations "${dep}")
    endif()
  endforeach()

elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  find_program(KAPPAN_LDD NAMES ldd)
  if(NOT KAPPAN_LDD)
    message(FATAL_ERROR "dist_selfcontained: ldd が見つかりません。検証を飛ばすと自己完結性を保証できません")
  endif()
  execute_process(
    COMMAND "${KAPPAN_LDD}" "${KAPPAN_EXE}"
    OUTPUT_VARIABLE raw
    ERROR_VARIABLE tool_stderr
    RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "dist_selfcontained: ldd に失敗しました (終了コード ${code})\n${tool_stderr}")
  endif()
  set(allowed linux-vdso.so.1 libm.so.6 libc.so.6 ld-linux-x86-64.so.2)
  string(REPLACE "\n" ";" lines "${raw}")
  foreach(line IN LISTS lines)
    if(line MATCHES "^\t([^ ]+)")
      get_filename_component(soname "${CMAKE_MATCH_1}" NAME)
      list(APPEND deps "${soname}")
    endif()
  endforeach()
  foreach(dep IN LISTS deps)
    if(dep MATCHES "^libstdc\\+\\+\\.so" OR dep MATCHES "^libgcc_s\\.so")
      list(APPEND violations "${dep}（静的リンクされているはず）")
    elseif(NOT "${dep}" IN_LIST allowed)
      list(APPEND violations "${dep}")
    endif()
  endforeach()

elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  find_program(KAPPAN_DUMPBIN NAMES dumpbin)
  if(NOT KAPPAN_DUMPBIN)
    message(FATAL_ERROR "dist_selfcontained: dumpbin が見つかりません。MSVC 開発環境が有効か確認してください")
  endif()
  execute_process(
    COMMAND "${KAPPAN_DUMPBIN}" /dependents "${KAPPAN_EXE}"
    OUTPUT_VARIABLE raw
    ERROR_VARIABLE tool_stderr
    RESULT_VARIABLE code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "dist_selfcontained: dumpbin /dependents に失敗しました (終了コード ${code})\n${tool_stderr}")
  endif()
  set(allowed KERNEL32.dll WS2_32.dll ADVAPI32.dll)
  string(REPLACE "\n" ";" lines "${raw}")
  foreach(line IN LISTS lines)
    if(line MATCHES "^[ \t]+([A-Za-z0-9_.+-]+\\.[Dd][Ll][Ll])")
      list(APPEND deps "${CMAKE_MATCH_1}")
    endif()
  endforeach()
  foreach(dep IN LISTS deps)
    string(TOUPPER "${dep}" upper)
    if(upper MATCHES "^VCRUNTIME140" OR upper MATCHES "^MSVCP140")
      list(APPEND violations "${dep}（/MD になっている。/MT でリンクすること）")
    elseif(NOT "${dep}" IN_LIST allowed)
      list(APPEND violations "${dep}")
    endif()
  endforeach()

else()
  message(FATAL_ERROR "dist_selfcontained: 想定外のホスト: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

if(NOT deps)
  message(
    FATAL_ERROR
    "dist_selfcontained: 依存を 1 つも取得できませんでした。出力の解析に失敗しています\n${raw}")
endif()

if(violations)
  message(
    FATAL_ERROR
    "dist_selfcontained: ${KAPPAN_EXE} に許可されていない依存があります: ${violations}\n"
    "採取結果:\n${raw}")
endif()

message(STATUS "dist_selfcontained: 依存は ${deps} のみでした")
