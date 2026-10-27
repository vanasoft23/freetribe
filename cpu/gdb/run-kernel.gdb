set confirm off
set pagination off
set breakpoint pending on

# Keep the kernel executable selected while running the DDR helper.
file ./build/kernel/kernel.elf
add-symbol-file ./build/ddr_bringup/ddr_bringup.elf 0x80000000
load ./build/ddr_bringup/ddr_bringup.elf

tbreak debug_ddr_ready
set $pc = 0x80000000
continue

printf "DDR ready, loading kernel ELF...\n"

remove-symbol-file -a &debug_ddr_ready
load ./build/kernel/kernel.elf
set $pc = 0xC0000000

#b _kernel_task
#b vPortTickISR

#tbreak main

monitor semihosting enable 0xFFFF0008
monitor semihosting IOClient 2
