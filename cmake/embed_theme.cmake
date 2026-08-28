if(NOT DEFINED INPUT_DIR OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "INPUT_DIR and OUTPUT are required")
endif()

get_filename_component(output_dir "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")

set(header "#pragma once\n\n#include <string_view>\n\nnamespace kappan::render::embedded {\n\n")
foreach(name IN ITEMS base post page)
  file(READ "${INPUT_DIR}/${name}.html" content)
  string(APPEND header "inline constexpr std::string_view ${name}_html = R\"kappan(${content})kappan\";\n\n")
endforeach()
string(APPEND header "} // namespace kappan::render::embedded\n")
file(WRITE "${OUTPUT}" "${header}")
