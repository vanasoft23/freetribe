cmake_minimum_required(VERSION 3.24)

project(freetribe LANGUAGES NONE)

# ---------------------------------------------------------------------------
# User-provided options
# ---------------------------------------------------------------------------

# -DLIB_C="C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\arm-none-eabi"
# -DLIB_GCC="C:\Program Files (x86)\GNU Arm Embedded Toolchain\10 2021.10\lib\gcc\arm-none-eabi\10.3.1"
# -DBFIN_INC="C:\apps\dev\bfin\elf\bfin-elf\include"

if(NOT LIB_C)
  set(LIB_C "" CACHE PATH "CPU C library root")
endif()
if(NOT LIB_GCC)
  set(LIB_GCC "" CACHE PATH "GCC support library root")
endif()


set(APP "" CACHE STRING "CPU app directory under cpu/src/apps to link into the kernel")
set(MODULE "" CACHE STRING "DSP module directory under dsp/src/modules to embed in the kernel")

option(BOOT_DEBUG_DELAY "Enable the early boot debug delay" OFF)
option(KERNEL_DDR_CACHEABLE "Deprecated: default for KERNEL_DDR_TEXT_CACHEABLE" ON)
option(KERNEL_DDR_TEXT_CACHEABLE "Map kernel text/rodata DDR section as cacheable when D-cache is enabled" ${KERNEL_DDR_CACHEABLE})
option(GENERATE_ASM "Generate per-source assembly listings (.S files)" OFF)


find_program(ARM_GCC arm-none-eabi-gcc REQUIRED)
find_program(ARM_OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(BFIN_GCC bfin-elf-gcc REQUIRED)
find_program(BFIN_LDR bfin-elf-ldr REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/firmware.cmake")

# ---------------------------------------------------------------------------
# Source discovery
# ---------------------------------------------------------------------------

set(COMMON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/common")
set(CPU_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cpu/src")
set(CPU_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cpu/lib")
set(DSP_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dsp/src")
set(DSP_ALEPH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/dsp/lib/aleph-dsp")

set(cpu_app_sources "")
set(cpu_app_dirs "")
set(cpu_app_common_sources "")
set(cpu_app_common_dirs "")

if(APP)
  set(cpu_app_dir "${CPU_SRC_ROOT}/apps/${APP}")
  if(NOT IS_DIRECTORY "${cpu_app_dir}")
    message(FATAL_ERROR "APP=${APP} does not exist at ${cpu_app_dir}")
  endif()

  message(STATUS "Kernel app: ${APP}")
  collect_sources(cpu_app_sources TRUE "${cpu_app_dir}")
  collect_dirs(cpu_app_dirs TRUE "${cpu_app_dir}")

  set(cpu_app_common_dir "${COMMON_ROOT}/apps/${APP}")
  if(IS_DIRECTORY "${cpu_app_common_dir}")
    collect_sources(cpu_app_common_sources TRUE "${cpu_app_common_dir}")
    collect_dirs(cpu_app_common_dirs TRUE "${cpu_app_common_dir}")
  endif()
endif()

set(dsp_module_sources "")
set(dsp_module_dirs "")
set(dsp_module_common_sources "")
set(dsp_module_common_dirs "")

if(MODULE)
  set(dsp_module_dir "${DSP_SRC_ROOT}/modules/${MODULE}")
  if(NOT IS_DIRECTORY "${dsp_module_dir}")
    message(FATAL_ERROR "MODULE=${MODULE} does not exist at ${dsp_module_dir}")
  endif()

  message(STATUS "DSP module: ${MODULE}")
  collect_sources(dsp_module_sources TRUE "${dsp_module_dir}")
  collect_dirs(dsp_module_dirs TRUE "${dsp_module_dir}")

  set(dsp_module_common_dir "${COMMON_ROOT}/apps/${MODULE}")
  if(IS_DIRECTORY "${dsp_module_common_dir}")
    collect_sources(dsp_module_common_sources TRUE "${dsp_module_common_dir}")
    collect_dirs(dsp_module_common_dirs TRUE "${dsp_module_common_dir}")
  endif()
endif()

# CPU sources
collect_sources(cpu_common_sources FALSE "${COMMON_ROOT}")
collect_sources(cpu_kernel_src_sources TRUE
  "${CPU_SRC_ROOT}/api"
  "${CPU_SRC_ROOT}/kernel"
  "${CPU_SRC_ROOT}/resources"
  "${CPU_SRC_ROOT}/user"
)

# The replacement MMC/SD peripheral is developed and exercised only by the
# dedicated hardware-test target until it is ready for production integration.
set(mmcsd_ft_source "${CPU_SRC_ROOT}/kernel/peripheral/per_mmcsd_ft.c")
list(REMOVE_ITEM cpu_kernel_src_sources "${mmcsd_ft_source}")

collect_sources(cpu_bootloader_sources TRUE "${CPU_SRC_ROOT}/bootloader")
collect_sources(cpu_lib_root_sources FALSE "${CPU_LIB_ROOT}")
collect_sources(cpu_lib_leaf_sources TRUE "${CPU_LIB_ROOT}/LEAF/leaf")
collect_sources(cpu_lib_fatfs_sources FALSE "${CPU_LIB_ROOT}/FatFS")
collect_sources(cpu_lib_csl_sources TRUE "${CPU_LIB_ROOT}/am1802-csl")
collect_sources(cpu_lib_am18x_sources TRUE "${CPU_LIB_ROOT}/am18x-lib")
collect_sources(cpu_lib_ugui_sources TRUE "${CPU_LIB_ROOT}/UGUI")
collect_sources(cpu_lib_tinyprintf_sources FALSE "${CPU_LIB_ROOT}/tinyprintf")
set(cpu_lib_tinyusb_device_sources
  "${CPU_LIB_ROOT}/tinyusb/src/tusb.c"
  "${CPU_LIB_ROOT}/tinyusb/src/common/tusb_fifo.c"
  "${CPU_LIB_ROOT}/tinyusb/src/device/usbd.c"
  "${CPU_LIB_ROOT}/tinyusb/src/class/cdc/cdc_device.c"
  "${CPU_LIB_ROOT}/tinyusb/src/class/dfu/dfu_device.c"
  "${CPU_LIB_ROOT}/tinyusb/src/class/msc/msc_device.c"
  "${CPU_LIB_ROOT}/tinyusb/src/class/vendor/vendor_device.c"
  "${CPU_LIB_ROOT}/tinyusb/src/portable/mentor/musb/dcd_musb.c"
)
collect_sources(cpu_lib_freertos_root_sources FALSE "${CPU_LIB_ROOT}/FreeRTOS-Kernel")
collect_sources(cpu_lib_freertos_port_sources FALSE "${CPU_LIB_ROOT}/FreeRTOS-Kernel/portable/GCC/AM1802")

set(kernel_sources
  ${cpu_common_sources}
  ${cpu_app_common_sources}
  ${cpu_kernel_src_sources}
  ${cpu_app_sources}
  ${cpu_lib_root_sources}
  ${cpu_lib_leaf_sources}
  ${cpu_lib_fatfs_sources}
  ${cpu_lib_ugui_sources}
  ${cpu_lib_csl_sources}
  ${cpu_lib_am18x_sources}
  ${cpu_lib_tinyprintf_sources}
  ${cpu_lib_tinyusb_device_sources}
  ${cpu_lib_freertos_root_sources}
  ${cpu_lib_freertos_port_sources}
)
list(REMOVE_DUPLICATES kernel_sources)

set(bootloader_sources
  ${cpu_common_sources}
  ${cpu_lib_root_sources}
  ${cpu_lib_csl_sources}
  ${cpu_lib_am18x_sources}
  ${cpu_lib_fatfs_sources}
  ${cpu_lib_ugui_sources}
  ${cpu_lib_tinyprintf_sources}
  ${cpu_lib_tinyusb_device_sources}
  ${cpu_bootloader_sources}
  "${CPU_SRC_ROOT}/kernel/peripheral/per_aintc.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_ddr.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_gpio.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_mmcsd.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_mmcsd_prot.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_pinmux.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_spi.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_timer.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_uart.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_usb.c"
  "${CPU_SRC_ROOT}/kernel/device/dev_board.c"
  "${CPU_SRC_ROOT}/kernel/device/dev_flash.c"
  "${CPU_SRC_ROOT}/kernel/device/dev_lcd.c"
  "${CPU_SRC_ROOT}/kernel/device/dev_mcu.c"
  "${CPU_SRC_ROOT}/kernel/device/dev_sdcard.c"
  "${CPU_SRC_ROOT}/kernel/service/svc_fatfs.c"
  "${CPU_SRC_ROOT}/kernel/service/svc_panel.c"
  "${CPU_SRC_ROOT}/kernel/service/svc_delay.c"
  "${CPU_SRC_ROOT}/kernel/service/svc_logger.c"
)
list(REMOVE_DUPLICATES bootloader_sources)

set(debug_bringup_sources
  "${CPU_SRC_ROOT}/debug_bringup.c"
  "${CPU_SRC_ROOT}/kernel/system/startup.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_ddr.c"
  ${cpu_lib_csl_sources}
)
list(REMOVE_DUPLICATES debug_bringup_sources)

set(mmcsd_hw_test_root "${CMAKE_CURRENT_SOURCE_DIR}/cpu/tests/hardware/mmcsd")
set(mmcsd_hw_test_sources
  "${CPU_SRC_ROOT}/kernel/system/init.S"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_gpio.c"
  "${CPU_SRC_ROOT}/kernel/peripheral/per_pinmux.c"
  "${CPU_LIB_ROOT}/am1802-csl/src/csl_gpio.c"
  "${CPU_LIB_ROOT}/tinyprintf/tinyprintf.c"
  "${mmcsd_ft_source}"
  "${mmcsd_hw_test_root}/hw_test.c"
  "${mmcsd_hw_test_root}/main.c"
)

# DSP sources
collect_sources(dsp_common_sources FALSE "${COMMON_ROOT}")
collect_sources(dsp_src_sources TRUE "${DSP_SRC_ROOT}")
collect_sources(dsp_aleph_src_sources TRUE "${DSP_ALEPH_ROOT}/src")
collect_sources(dsp_libfixmath_sources FALSE "${DSP_ALEPH_ROOT}/lib/libfixmath/libfixmath")

set(dsp_sources
  ${dsp_common_sources}
  ${dsp_module_common_sources}
  ${dsp_src_sources}
  ${dsp_aleph_src_sources}
  ${dsp_libfixmath_sources}
)
list(FILTER dsp_sources EXCLUDE REGEX ".*/dsp/src/modules(/.*)?$")
list(APPEND dsp_sources ${dsp_module_sources})
list(REMOVE_DUPLICATES dsp_sources)

# ---------------------------------------------------------------------------
# Extra source dependencies
# ---------------------------------------------------------------------------

set(cpu_generated_resource_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/resources")
set(cpu_generated_resource_dirs "${cpu_generated_resource_dir}")
set(bin2c_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2c.cmake")

set(gdb_target_xml "${CPU_SRC_ROOT}/bootloader/usb/gdbstub/target.xml")
set(gdb_target_xml_header "${cpu_generated_resource_dir}/gdb_target_xml.h")
set(mascot_bmp "${CPU_SRC_ROOT}/resources/mascot.bmp")
set(mascot_bmp_header "${cpu_generated_resource_dir}/mascot_bmp.h")

add_custom_command(
  OUTPUT "${mascot_bmp_header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${cpu_generated_resource_dir}"
  COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${mascot_bmp}"
          "-DOUTPUT=${mascot_bmp_header}"
          "-DSYMBOL=mascot_bmp"
          -DCONST=ON
          -P "${bin2c_script}"
  DEPENDS "${mascot_bmp}" "${bin2c_script}"
  VERBATIM
)

add_custom_command(
  OUTPUT "${gdb_target_xml_header}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${cpu_generated_resource_dir}"
  COMMAND "${CMAKE_COMMAND}"
          "-DINPUT=${gdb_target_xml}"
          "-DOUTPUT=${gdb_target_xml_header}"
          "-DSYMBOL=gdb_target_xml"
          -DCONST=ON
          -P "${bin2c_script}"
  DEPENDS "${gdb_target_xml}" "${bin2c_script}"
  VERBATIM
)


set(bootloader_source_deps
  "${CPU_SRC_ROOT}/bootloader/usb/gdbstub/gdb_server.c|${gdb_target_xml_header}"
  "${CPU_SRC_ROOT}/bootloader/ui/render.c|${mascot_bmp_header}"
)

set(kernel_source_deps "")
if(MODULE)
  list(APPEND kernel_source_deps
    "${CPU_SRC_ROOT}/kernel/service/svc_dsp.c|${dsp_ldr_header}"
  )
endif()

if(MODULE)
  set(dsp_ldr_header "${cpu_generated_resource_dir}/bfin_ldr.h")

  add_custom_command(
    OUTPUT "${dsp_ldr_header}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${cpu_generated_resource_dir}"
    COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${CMAKE_CURRENT_BINARY_DIR}/dsp/boot.ldr"
             "-DOUTPUT=${dsp_ldr_header}"
             "-DSYMBOL=bfin_ldr"
             -P "${bin2c_script}"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/dsp/boot.ldr" "${bin2c_script}"
    VERBATIM
  )
endif()

# ---------------------------------------------------------------------------
# Include directories
# ---------------------------------------------------------------------------

# CPU include directories
collect_dirs(cpu_common_dirs FALSE "${COMMON_ROOT}")
collect_dirs(cpu_src_dirs TRUE "${CPU_SRC_ROOT}")
collect_dirs(cpu_lib_dir FALSE "${CPU_LIB_ROOT}")
collect_dirs(cpu_leaf_dirs TRUE "${CPU_LIB_ROOT}/LEAF/leaf")
collect_dirs(cpu_fatfs_dir FALSE "${CPU_LIB_ROOT}/FatFS")
collect_dirs(cpu_freertos_include_dirs TRUE "${CPU_LIB_ROOT}/FreeRTOS-Kernel/include")
collect_dirs(cpu_freertos_port_dirs TRUE "${CPU_LIB_ROOT}/FreeRTOS-Kernel/portable/GCC/AM1802")
collect_dirs(cpu_csl_inc_dirs TRUE "${CPU_LIB_ROOT}/am1802-csl")
collect_dirs(cpu_am18x_dirs TRUE "${CPU_LIB_ROOT}/am18x-lib")
collect_dirs(cpu_am18x_inc_dirs TRUE "${CPU_LIB_ROOT}/am18x-lib/inc")
collect_dirs(cpu_tinyprintf_inc_dirs TRUE "${CPU_LIB_ROOT}/tinyprintf")
collect_dirs(cpu_tinyusb_inc_dirs TRUE "${CPU_LIB_ROOT}/tinyusb/src")

set(cpu_include_dirs
  ${cpu_common_dirs}
  ${cpu_src_dirs}
  ${cpu_lib_dir}
  ${cpu_leaf_dirs}
  ${cpu_fatfs_dir}
  ${cpu_freertos_include_dirs}
  ${cpu_freertos_port_dirs}
  ${cpu_csl_inc_dirs}
  ${cpu_am18x_dirs}
  ${cpu_am18x_inc_dirs}
  ${cpu_tinyprintf_inc_dirs}
  ${cpu_tinyusb_inc_dirs}
)

if(LIB_C AND IS_DIRECTORY "${LIB_C}")
  list(APPEND cpu_include_dirs "${LIB_C}/include")
endif()
if(LIB_GCC AND IS_DIRECTORY "${LIB_GCC}")
  list(APPEND cpu_include_dirs "${LIB_GCC}/include")
endif()

list(FILTER cpu_include_dirs EXCLUDE REGEX ".*/cpu/src/apps(/.*)?$")
if(cpu_generated_resource_dirs)
  list(PREPEND cpu_include_dirs ${cpu_generated_resource_dirs})
endif()
list(APPEND cpu_include_dirs ${cpu_app_common_dirs} ${cpu_app_dirs})
list(REMOVE_DUPLICATES cpu_include_dirs)

# DSP include directories
collect_dirs(dsp_common_dirs FALSE "${COMMON_ROOT}")
collect_dirs(dsp_src_dirs TRUE "${DSP_SRC_ROOT}")
collect_dirs(dsp_aleph_src_dirs TRUE "${DSP_ALEPH_ROOT}/src")
collect_dirs(dsp_libfixmath_dirs FALSE "${DSP_ALEPH_ROOT}/lib/libfixmath/libfixmath")

set(dsp_include_dirs
  ${dsp_common_dirs}
  ${dsp_src_dirs}
  ${dsp_aleph_src_dirs}
  ${dsp_libfixmath_dirs}
)

list(FILTER dsp_include_dirs EXCLUDE REGEX ".*/dsp/src/modules(/.*)?$")
list(APPEND dsp_include_dirs ${dsp_module_common_dirs} ${dsp_module_dirs})
list(REMOVE_DUPLICATES dsp_include_dirs)

# ---------------------------------------------------------------------------
# Compiler Flags
# ---------------------------------------------------------------------------

set(kernel_c_flags
  -mcpu=arm926ej-s
  -O0
  -ggdb
  -fdata-sections
  -ffunction-sections
  -fanalyzer
  -fstack-usage
  -Wstack-usage=512
  -Wall
  -Wno-unused
  -Wno-strict-aliasing
  -DNESTED_INTERRUPTS=0
  -DDEBUG
  -DFREETRIBE_FREERTOS
  "-DCFG_TUSB_CONFIG_FILE=\"kernel/service/usb/tusb_config.h\""
  -DKERNEL_DDR_TEXT_CACHEABLE=$<IF:$<BOOL:${KERNEL_DDR_TEXT_CACHEABLE}>,1,0>
  # -DDEBUG_SDDRIVER
  -MMD
  -MP
)

set(bootloader_c_flags
  -mcpu=arm926ej-s
  -ggdb
  -Os
  -fdata-sections
  -ffunction-sections
  #-fomit-frame-pointer
  -fstack-usage
  -Wstack-usage=1024
  -Wall
  -Wno-unused
  -Wno-strict-aliasing
  -DDEBUG
  # -DDEBUG_BOOTLOADER_INSTALL
  -DFREETRIBE_FATFS_READONLY=0
  $<$<BOOL:${NO_BOOTSECT_VERIFY}>:-DNO_BOOTSECT_VERIFY>
  "-DCFG_TUSB_CONFIG_FILE=\"bootloader/usb/tusb_config.h\""
  # -DDEBUG_SDDRIVER
  -DUGUI_USE_FONT_5X8
  -MMD
  -MP
)

set(bootloader_asm_flags "")
if(BOOT_DEBUG_DELAY)
  list(APPEND bootloader_asm_flags -DBOOT_DEBUG_DELAY)
endif()

set(debug_bringup_c_flags
  ${bootloader_c_flags}
)

set(mmcsd_hw_test_c_flags
  -mcpu=arm926ej-s
  -Og
  -ggdb
  -fdata-sections
  -ffunction-sections
  -Wall
  -Wextra
)

set(mmcsd_hw_test_asm_flags
  -mcpu=arm926ej-s
  -ggdb
)

set(dsp_c_flags
  -mcpu=bf527
  -Os
  -mfast-fp
  -fdata-sections
  -ffunction-sections
  -Wall
  -DARCH_BFIN=1
  -D__ADSPBF523__
  -DDEBUG
  -DBLACKFIN
  -MMD
  -MP
)

# ---------------------------------------------------------------------------
# Linker Options
# ---------------------------------------------------------------------------

set(cpu_link_options
  -Wl,-e,start
  -Wl,-u,start
  -Wl,-u,__aeabi_uidiv
  -Wl,-u,__aeabi_idiv
  -Wl,--gc-sections
  -Wl,--print-memory-usage
  # -Wl,--no-warn-rwx-segments # Incompatible on Windows GNU ARM Embedded Toolchain 10 2021.10
)

set(bootloader_link_options
  -Wl,-e,start
  -Wl,-u,start
  -Wl,-u,__aeabi_uidiv
  -Wl,-u,__aeabi_idiv
  -Wl,--gc-sections
  -Wl,--print-memory-usage
)

set(mmcsd_hw_test_link_options
  -Wl,-e,start
  -Wl,-u,start
  -Wl,--gc-sections
  -Wl,--print-memory-usage
)

set(cpu_link_libraries
  -lm
  -lc
  -lgcc
)

set(dsp_link_options
  -Wl,--gc-sections
)

set(dsp_link_libraries
  -lm
  -lbfdsp
  -lbffastfp
)

set(dsp_ldr_flags
  -T BF527
  -c
  --bits 8
  --bmode spi_slave
  # --verbose
)

# ---------------------------------------------------------------------------
# ASM Flags
# ---------------------------------------------------------------------------

set(dsp_asm_flags
  -mcpu=bf527
  -mfast-fp
  -DARCH_BFIN=1
  -D__ADSPBF523__
  -DDEBUG
  -DBLACKFIN
  -MMD
  -MP
)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

add_firmware_target(
  NAME kernel
  COMPILER "${ARM_GCC}"
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cpu/kernel.lds"
  OBJCOPY_TOOL "${ARM_OBJCOPY}"
  BIN_CHECKSUM_TRAILER
  SOURCES ${kernel_sources}
  INCLUDE_DIRS ${cpu_include_dirs}
  C_FLAGS ${kernel_c_flags}
  LINK_OPTIONS ${cpu_link_options}
  LINK_DIRS ${LIB_GCC} "${LIB_C}/lib"
  LINK_LIBS ${cpu_link_libraries}
  SOURCE_DEPS ${kernel_source_deps}
)

add_firmware_target(
  NAME bootloader
  COMPILER "${ARM_GCC}"
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cpu/bootloader.lds"
  OBJCOPY_TOOL "${ARM_OBJCOPY}"
  OBJCOPY_OPTIONS --gap-fill 0x00
  SOURCES ${bootloader_sources}
  INCLUDE_DIRS ${cpu_include_dirs}
  C_FLAGS ${bootloader_c_flags}
  ASM_FLAGS ${bootloader_asm_flags}
  LINK_OPTIONS ${bootloader_link_options}
  LINK_DIRS ${LIB_GCC} "${LIB_C}/lib"
  SOURCE_DEPS ${bootloader_source_deps}
)

add_firmware_target(
  NAME debug_bringup
  COMPILER "${ARM_GCC}"
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cpu/debug_bringup.lds"
  SOURCES ${debug_bringup_sources}
  INCLUDE_DIRS ${cpu_include_dirs}
  C_FLAGS ${debug_bringup_c_flags}
  LINK_OPTIONS ${bootloader_link_options}
  LINK_DIRS ${LIB_GCC} "${LIB_C}/lib"
)

add_firmware_target(
  NAME mmcsd_hw_test
  COMPILER "${ARM_GCC}"
  LINKER_SCRIPT "${mmcsd_hw_test_root}/mmcsd_hw_test.lds"
  SOURCES ${mmcsd_hw_test_sources}
  INCLUDE_DIRS ${cpu_include_dirs} "${mmcsd_hw_test_root}"
  C_FLAGS ${mmcsd_hw_test_c_flags}
  ASM_FLAGS ${mmcsd_hw_test_asm_flags}
  LINK_OPTIONS ${mmcsd_hw_test_link_options}
  LINK_DIRS ${LIB_GCC} "${LIB_C}/lib"
  LINK_LIBS ${cpu_link_libraries}
)

add_firmware_target(
  NAME dsp
  COMPILER "${BFIN_GCC}"
  LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/dsp/dsp.lds"
  SOURCES ${dsp_sources}
  INCLUDE_DIRS ${dsp_include_dirs}
  C_FLAGS ${dsp_c_flags}
  ASM_FLAGS ${dsp_asm_flags}
  LINK_OPTIONS ${dsp_link_options}
  LINK_LIBS ${dsp_link_libraries}
  LDR_TOOL "${BFIN_LDR}"
  LDR_FLAGS ${dsp_ldr_flags}
)

# Make a bare `cmake --build build` behave like Ctrl+Shift+B.
add_custom_target(build_all ALL
  DEPENDS bootloader
  DEPENDS kernel
  DEPENDS debug_bringup
  DEPENDS dsp
)

# The firmware targets use custom compiler commands, so firmware.cmake emits
# their compilation database explicitly instead of relying on CMake's native
# CMAKE_EXPORT_COMPILE_COMMANDS support.
write_firmware_compile_commands("${CMAKE_BINARY_DIR}/compile_commands.json")
