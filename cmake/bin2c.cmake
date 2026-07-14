if(NOT DEFINED INPUT)
  message(FATAL_ERROR "bin2c.cmake requires -DINPUT=<file>")
endif()

if(NOT DEFINED OUTPUT)
  message(FATAL_ERROR "bin2c.cmake requires -DOUTPUT=<file>")
endif()

if(NOT DEFINED SYMBOL)
  set(SYMBOL "binary_data")
endif()

file(READ "${INPUT}" hex_data HEX)
string(LENGTH "${hex_data}" hex_length)
math(EXPR byte_length "${hex_length} / 2")

set(header "unsigned char ${SYMBOL}[] = {\n")
set(line "    ")
set(index 0)

while(index LESS byte_length)
  math(EXPR offset "${index} * 2")
  string(SUBSTRING "${hex_data}" "${offset}" 2 byte)
  string(APPEND line "0x${byte}, ")

  math(EXPR next_index "${index} + 1")
  math(EXPR line_byte_count "${next_index} % 12")
  if(line_byte_count EQUAL 0)
    string(APPEND header "${line}\n")
    set(line "    ")
  endif()

  set(index "${next_index}")
endwhile()

if(NOT line STREQUAL "    ")
  string(APPEND header "${line}\n")
endif()

string(APPEND header "};\n")
string(APPEND header "unsigned int ${SYMBOL}_len = ${byte_length};\n")

file(WRITE "${OUTPUT}" "${header}")
