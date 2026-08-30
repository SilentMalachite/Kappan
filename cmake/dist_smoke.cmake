# 配布バイナリのスモークテスト。
# 実行ファイル実体に examples/ を生成させ、tests/golden/*/expected と突き合わせる。
# ライブラリ経由の golden テストと違い、CLI の引数解析・出力先判定・終了コードを通る。

cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS KAPPAN_EXE REPO_ROOT WORK_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "dist_smoke: -D${required} が指定されていません")
  endif()
endforeach()

if(NOT EXISTS "${KAPPAN_EXE}")
  message(FATAL_ERROR "dist_smoke: 実行ファイルがありません: ${KAPPAN_EXE}")
endif()

# root 以下の通常ファイルを相対パスの昇順で返す。
# file(GLOB_RECURSE) は '.kappan-out' のような隠しファイルも拾う（実測済み）。
function(kappan_list_files root out_var)
  file(
    GLOB_RECURSE found
    LIST_DIRECTORIES false
    RELATIVE "${root}"
    "${root}/*")
  list(SORT found)
  set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

function(kappan_compare_site example golden)
  set(source "${REPO_ROOT}/examples/${example}")
  set(expected "${REPO_ROOT}/tests/golden/${golden}/expected")
  set(out "${WORK_DIR}/${example}")

  if(NOT IS_DIRECTORY "${expected}")
    message(FATAL_ERROR "dist_smoke: 期待値ディレクトリがありません: ${expected}")
  endif()

  # 空ディレクトリなら --out の削除ポリシーに抵触しない。
  file(REMOVE_RECURSE "${out}")
  file(MAKE_DIRECTORY "${out}")

  execute_process(
    COMMAND "${KAPPAN_EXE}" build --source "${source}" --out "${out}"
    RESULT_VARIABLE code
    OUTPUT_VARIABLE tool_stdout
    ERROR_VARIABLE tool_stderr)
  if(NOT code EQUAL 0)
    message(
      FATAL_ERROR
      "dist_smoke: examples/${example} の生成に失敗しました (終了コード ${code})\n"
      "stdout:\n${tool_stdout}\nstderr:\n${tool_stderr}")
  endif()

  kappan_list_files("${expected}" expected_files)
  kappan_list_files("${out}" actual_files)
  if(NOT expected_files STREQUAL actual_files)
    message(
      FATAL_ERROR
      "dist_smoke: examples/${example} の出力ファイル一覧が期待と違います\n"
      "期待 (${golden}/expected): ${expected_files}\n"
      "実際 (${out}): ${actual_files}")
  endif()
  if(NOT expected_files)
    message(FATAL_ERROR "dist_smoke: ${golden}/expected が空です。期待値の置き場所が違う可能性があります")
  endif()

  foreach(rel IN LISTS expected_files)
    file(READ "${expected}/${rel}" want HEX)
    file(READ "${out}/${rel}" got HEX)
    if(NOT want STREQUAL got)
      message(
        FATAL_ERROR
        "dist_smoke: ${golden}/expected/${rel} と生成結果の内容が違います\n"
        "期待 ${want}\n実際 ${got}")
    endif()
  endforeach()

  file(REMOVE_RECURSE "${out}")
  message(STATUS "dist_smoke: examples/${example} は ${golden}/expected と一致しました（${expected_files}）")
endfunction()

kappan_compare_site(blog blog-ja)
kappan_compare_site(landing landing-ja)
