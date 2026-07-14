if(NOT DEFINED KERNEL_BIN_INPUT)
  message(FATAL_ERROR "kernel_bin_checksum.cmake requires -DKERNEL_BIN_INPUT=<file>")
endif()

if(NOT DEFINED KERNEL_BIN_OUTPUT)
  message(FATAL_ERROR "kernel_bin_checksum.cmake requires -DKERNEL_BIN_OUTPUT=<file>")
endif()

set(KERNEL_CHECKSUM_MAGIC "FTKERNL1")
set(KERNEL_CHECKSUM_VERSION "00000001")
set(KERNEL_CHECKSUM_RESERVED "                                ")

function(uint32_to_hex8 out_var value)
  math(EXPR masked "${value} & 0xFFFFFFFF" OUTPUT_FORMAT HEXADECIMAL)
  string(REGEX REPLACE "^0x" "" hex "${masked}")
  string(TOUPPER "${hex}" hex)
  string(LENGTH "${hex}" hex_len)

  while(hex_len LESS 8)
    string(PREPEND hex "0")
    math(EXPR hex_len "${hex_len} + 1")
  endwhile()

  set(${out_var} "${hex}" PARENT_SCOPE)
endfunction()

file(SIZE "${KERNEL_BIN_INPUT}" payload_size)

if(payload_size GREATER 0xFFFFFFFF)
  message(FATAL_ERROR "Input is too large for 32-bit trailer size: ${KERNEL_BIN_INPUT}")
endif()

file(SHA256 "${KERNEL_BIN_INPUT}" payload_sha256)
string(SUBSTRING "${payload_sha256}" 0 8 payload_checksum_hex)
string(TOUPPER "${payload_checksum_hex}" payload_checksum_hex)

uint32_to_hex8(payload_size_hex "${payload_size}")

set(trailer
  "${KERNEL_CHECKSUM_MAGIC}${KERNEL_CHECKSUM_VERSION}${payload_size_hex}${payload_checksum_hex}${KERNEL_CHECKSUM_RESERVED}"
)
string(LENGTH "${trailer}" trailer_length)

if(NOT trailer_length EQUAL 64)
  message(FATAL_ERROR "Kernel checksum trailer is ${trailer_length} bytes, expected 64")
endif()

file(COPY_FILE "${KERNEL_BIN_INPUT}" "${KERNEL_BIN_OUTPUT}")
file(APPEND "${KERNEL_BIN_OUTPUT}" "${trailer}")

message(STATUS "Kernel bin checksum: size=0x${payload_size_hex} checksum=0x${payload_checksum_hex}")
