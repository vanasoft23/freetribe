set confirm off
set pagination off
set breakpoint pending on

monitor semihosting enable 0xFFFF0008
monitor semihosting IOClient 2

file ./build/debug_bringup/debug_bringup.elf
load ./build/debug_bringup/debug_bringup.elf

tbreak debug_ddr_ready
set $pc = 0x80000000
continue

printf "DDR ready, loading MMC/SD hardware-test ELF...\n"

file ./build/mmcsd_hw_test/mmcsd_hw_test.elf
load ./build/mmcsd_hw_test/mmcsd_hw_test.elf
tbreak mmcsd_hw_test_complete
set $pc = start
continue
