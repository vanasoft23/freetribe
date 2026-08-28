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

printf "DDR ready, loading kernel ELF...\n"

file ./build/kernel/kernel.elf
load ./build/kernel/kernel.elf
set $pc = 0xC0000000

#b _kernel_task
#b vPortTickISR