set confirm off
set pagination off
set breakpoint pending on

monitor semihosting enable 0xFFFF0008
monitor semihosting IOClient 2

file ./build/ddr_bringup/ddr_bringup.elf
load ./build/ddr_bringup/ddr_bringup.elf

tbreak debug_ddr_ready
set $pc = 0x80000000
continue

printf "DDR ready, loading hardware-test ELF...\n"

file ./build/hw_test/hw_test.elf
load ./build/hw_test/hw_test.elf
tbreak _boot_abort
set $pc = start
continue
