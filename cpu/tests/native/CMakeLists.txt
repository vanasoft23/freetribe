cmake_minimum_required(VERSION 3.24)

project(freetribe_native_tests LANGUAGES C)

include(CTest)
include(FetchContent)

get_filename_component(
    FREETRIBE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../.."
    ABSOLUTE
)

FetchContent_Declare(
    unity
    URL
        https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.1.tar.gz
    URL_HASH
        SHA256=b41a66d45a6b99758fb3202ace6178177014d52fc524bf1f72687d93e9867292
)
FetchContent_MakeAvailable(unity)

set(RSP_SOURCE_ROOT
    "${FREETRIBE_ROOT}/cpu/src/bootloader/usb/gdbstub")

add_executable(rsp_tests
    "rsp/test_rsp.c"
    "${RSP_SOURCE_ROOT}/rsp_session.c"
)

target_include_directories(rsp_tests PRIVATE
    "${RSP_SOURCE_ROOT}"
    "${FREETRIBE_ROOT}/common"
)

target_compile_features(rsp_tests PRIVATE c_std_11)
target_link_libraries(rsp_tests PRIVATE unity)

if(MSVC)
    target_compile_options(rsp_tests PRIVATE /W4)
else()
    target_compile_options(rsp_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME rsp COMMAND rsp_tests)

add_executable(gdb_server_tests
    "gdb_server/test_gdb_server.c"
    "${RSP_SOURCE_ROOT}/gdb_server.c"
    "${FREETRIBE_ROOT}/cpu/src/bootloader/util/str_misc.c"
)

target_include_directories(gdb_server_tests PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/gdb_server"
    "${RSP_SOURCE_ROOT}"
    "${FREETRIBE_ROOT}/cpu/src/bootloader/util"
    "${FREETRIBE_ROOT}/common"
)

target_compile_features(gdb_server_tests PRIVATE c_std_11)
target_link_libraries(gdb_server_tests PRIVATE unity)

if(MSVC)
    target_compile_options(gdb_server_tests PRIVATE /W4)
else()
    target_compile_options(gdb_server_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME gdb_server COMMAND gdb_server_tests)
