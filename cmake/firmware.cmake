function(collect_sources out_var recursive)
  set(result "")
  foreach(dir IN LISTS ARGN)
    if(recursive)
      file(GLOB_RECURSE found CONFIGURE_DEPENDS
        "${dir}/*.c"
        "${dir}/*.cpp"
        "${dir}/*.S"
      )
    else()
      file(GLOB found CONFIGURE_DEPENDS
        "${dir}/*.c"
        "${dir}/*.cpp"
        "${dir}/*.S"
      )
    endif()

    list(APPEND result ${found})
  endforeach()

  list(REMOVE_DUPLICATES result)
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(collect_dirs out_var recursive)
  set(result "")
  foreach(dir IN LISTS ARGN)
    if(EXISTS "${dir}" AND IS_DIRECTORY "${dir}")
      list(APPEND result "${dir}")
    endif()

    if(recursive)
      file(GLOB_RECURSE found CONFIGURE_DEPENDS LIST_DIRECTORIES true
        "${dir}/*"
      )
    else()
      file(GLOB found CONFIGURE_DEPENDS LIST_DIRECTORIES true
        "${dir}/*"
      )
    endif()

    foreach(item IN LISTS found)
      if(IS_DIRECTORY "${item}")
        list(APPEND result "${item}")
      endif()
    endforeach()
  endforeach()

  list(REMOVE_DUPLICATES result)
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(to_include_flags out_var)
  set(result "")
  foreach(dir IN LISTS ARGN)
    if(dir)
      list(APPEND result "-I${dir}")
    endif()
  endforeach()
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(make_link_dir_flags out_var)
  set(result "")
  foreach(dir IN LISTS ARGN)
    if(dir)
      list(APPEND result "-L${dir}")
    endif()
  endforeach()
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# CMake's built-in CMAKE_EXPORT_COMPILE_COMMANDS only records sources compiled
# by CMake language targets. Firmware sources in this project are compiled by
# add_custom_command(), so record the equivalent invocations ourselves for
# clangd and other tooling.
function(json_quote out_var value)
  string(REPLACE "\\" "\\\\" result "${value}")
  string(REPLACE "\"" "\\\"" result "${result}")
  string(REPLACE "\n" "\\n" result "${result}")
  string(REPLACE "\r" "\\r" result "${result}")
  string(REPLACE "\t" "\\t" result "${result}")
  set(${out_var} "\"${result}\"" PARENT_SCOPE)
endfunction()

function(record_firmware_compile_command compiler source output)
  set(args "${compiler}" ${ARGN} -c "${source}" -o "${output}")
  set(json_args "")
  foreach(arg IN LISTS args)
    json_quote(quoted_arg "${arg}")
    list(APPEND json_args "${quoted_arg}")
  endforeach()
  string(JOIN ", " json_args_text ${json_args})

  json_quote(json_directory "${CMAKE_CURRENT_SOURCE_DIR}")
  json_quote(json_source "${source}")
  json_quote(json_output "${output}")
  set(entry
    "  {\n"
    "    \"directory\": ${json_directory},\n"
    "    \"file\": ${json_source},\n"
    "    \"output\": ${json_output},\n"
    "    \"arguments\": [${json_args_text}]\n"
    "  }"
  )
  string(JOIN "" entry ${entry})
  set_property(GLOBAL APPEND PROPERTY FREETRIBE_COMPILE_COMMANDS "${entry}")
endfunction()

function(write_firmware_compile_commands output_file)
  get_property(entries GLOBAL PROPERTY FREETRIBE_COMPILE_COMMANDS)
  string(JOIN ",\n" content ${entries})
  file(GENERATE OUTPUT "${output_file}" CONTENT "[\n${content}\n]\n")
endfunction()

function(add_firmware_target)
  cmake_parse_arguments(FW
    "BIN_CHECKSUM_TRAILER"
    "NAME;COMPILER;LINKER_SCRIPT;OBJCOPY_TOOL;LDR_TOOL"
    "SOURCES;INCLUDE_DIRS;C_FLAGS;ASM_FLAGS;LINK_OPTIONS;LINK_DIRS;LINK_LIBS;LDR_FLAGS;OBJCOPY_OPTIONS;SOURCE_DEPS"
    ${ARGN}
  )

  if(NOT FW_NAME OR NOT FW_COMPILER OR NOT FW_LINKER_SCRIPT)
    message(FATAL_ERROR "add_firmware_target(NAME/COMPILER/LINKER_SCRIPT required)")
  endif()

  set(firmware_root "${CMAKE_CURRENT_BINARY_DIR}/${FW_NAME}")
  set(obj_root "${firmware_root}/obj")
  set(asm_root "${firmware_root}/asm")
  set(elf_file "${firmware_root}/${FW_NAME}.elf")
  set(map_file "${firmware_root}/${FW_NAME}.map")

  file(MAKE_DIRECTORY "${firmware_root}")

  to_include_flags(include_flags ${FW_INCLUDE_DIRS})
  make_link_dir_flags(extra_link_dir_flags ${FW_LINK_DIRS})

  set(object_files "")
  set(debug_asm_files "")

  foreach(src IN LISTS FW_SOURCES)
    file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}" "${src}")

    set(obj_file "${obj_root}/${rel}.o")
    get_filename_component(obj_dir "${obj_file}" DIRECTORY)
    set(obj_dep "${obj_file}.d")

    get_filename_component(src_ext "${src}" LAST_EXT)
    if(src_ext STREQUAL ".S")
      set(source_flags ${FW_ASM_FLAGS})
    else()
      set(source_flags ${FW_C_FLAGS})
    endif()

    record_firmware_compile_command(
      "${FW_COMPILER}"
      "${src}"
      "${obj_file}"
      ${include_flags}
      ${source_flags}
    )

    #
    # Include any extra dependencies for this source
    #
    set(src_deps "")
    foreach(source_dep IN LISTS FW_SOURCE_DEPS)
      string(REPLACE "|" ";" source_dep_parts "${source_dep}")
      list(GET source_dep_parts 0 dep_source)
      list(GET source_dep_parts 1 dep_file)

      if(src STREQUAL dep_source)
        list(APPEND src_deps "${dep_file}")
      endif()
    endforeach()


    add_custom_command(
      OUTPUT "${obj_file}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${obj_dir}"
      COMMAND "${FW_COMPILER}"
              ${include_flags}
              ${source_flags}
              -MMD -MP -MF "${obj_dep}" -MT "${obj_file}"
              -c "${src}"
              -o "${obj_file}"
      DEPENDS "${src}"
              ${src_deps}
      DEPFILE "${obj_dep}"
      WORKING_DIRECTORY "${obj_dir}"
      VERBATIM
      COMMAND_EXPAND_LISTS
    )

    list(APPEND object_files "${obj_file}")

    if(GENERATE_ASM AND src IN_LIST FW_SOURCES)
      set(asm_file "${asm_root}/${rel}.S")
      get_filename_component(asm_dir "${asm_file}" DIRECTORY)
      set(asm_dep "${asm_file}.d")

      add_custom_command(
        OUTPUT "${asm_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${asm_dir}"
        COMMAND "${FW_COMPILER}"
                ${include_flags}
                ${source_flags}
                -MMD -MP -MF "${asm_dep}" -MT "${asm_file}"
                -S "${src}"
                -o "${asm_file}"
        DEPENDS "${src}"
        DEPFILE "${asm_dep}"
        WORKING_DIRECTORY "${asm_dir}"
        VERBATIM
        COMMAND_EXPAND_LISTS
      )

      list(APPEND debug_asm_files "${asm_file}")
    endif()
  endforeach()

  add_custom_command(
    OUTPUT "${elf_file}" "${map_file}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${firmware_root}"
    COMMAND "${FW_COMPILER}"
            ${extra_link_dir_flags}
            ${FW_LINK_OPTIONS}
            -Wl,-Map=${map_file}
            -T "${FW_LINKER_SCRIPT}"
            ${object_files}
            ${FW_LINK_LIBS}
            -o "${elf_file}"
    DEPENDS ${object_files} "${FW_LINKER_SCRIPT}"
    WORKING_DIRECTORY "${firmware_root}"
    VERBATIM
    COMMAND_EXPAND_LISTS
  )

  set(all_deps "${elf_file}" "${map_file}")
  list(APPEND all_deps ${object_files} ${debug_asm_files})

  if(FW_OBJCOPY_TOOL)
    set(bin_file "${firmware_root}/${FW_NAME}.bin")
    if(FW_BIN_CHECKSUM_TRAILER)
      set(raw_bin_file "${firmware_root}/${FW_NAME}.raw.bin")
      set(kernel_bin_checksum_script "${CMAKE_CURRENT_SOURCE_DIR}/CMake/kernel_bin_checksum.cmake")

      add_custom_command(
        OUTPUT "${raw_bin_file}"
        COMMAND "${FW_OBJCOPY_TOOL}"
                -O binary
                ${FW_OBJCOPY_OPTIONS}
                "${elf_file}"
                "${raw_bin_file}"
        DEPENDS "${elf_file}"
        WORKING_DIRECTORY "${firmware_root}"
        VERBATIM
        COMMAND_EXPAND_LISTS
      )

      add_custom_command(
        OUTPUT "${bin_file}"
        COMMAND "${CMAKE_COMMAND}"
                "-DKERNEL_BIN_INPUT=${raw_bin_file}"
                "-DKERNEL_BIN_OUTPUT=${bin_file}"
                -P "${kernel_bin_checksum_script}"
        DEPENDS "${raw_bin_file}" "${kernel_bin_checksum_script}"
        WORKING_DIRECTORY "${firmware_root}"
        VERBATIM
        COMMAND_EXPAND_LISTS
      )

      list(APPEND all_deps "${raw_bin_file}")
    else()
      add_custom_command(
        OUTPUT "${bin_file}"
        COMMAND "${FW_OBJCOPY_TOOL}"
                -O binary
                ${FW_OBJCOPY_OPTIONS}
                "${elf_file}"
                "${bin_file}"
        DEPENDS "${elf_file}"
        WORKING_DIRECTORY "${firmware_root}"
        VERBATIM
        COMMAND_EXPAND_LISTS
      )
    endif()
    list(APPEND all_deps "${bin_file}")
  endif()

  set(ldr_file "")
  if(FW_LDR_TOOL)
    set(ldr_file "${firmware_root}/boot.ldr")
    add_custom_command(
      OUTPUT "${ldr_file}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${firmware_root}"
      COMMAND "${FW_LDR_TOOL}"
              ${FW_LDR_FLAGS}
              "${ldr_file}"
              "${elf_file}"
      DEPENDS "${elf_file}"
      WORKING_DIRECTORY "${firmware_root}"
      VERBATIM
      COMMAND_EXPAND_LISTS
    )
    list(APPEND all_deps "${ldr_file}")
  endif()

  add_custom_target("${FW_NAME}" DEPENDS ${all_deps})

  add_custom_target("${FW_NAME}_clean"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${firmware_root}"
  )
endfunction()
