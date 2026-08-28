set confirm off
set pagination off
set breakpoint pending on

monitor semihosting enable 0xFFFF0008
monitor semihosting IOClient 2

#   This is for working on USB GDB stub
# set debug remote 1

load ./build/bootloader/bootloader.elf
file ./build/bootloader/bootloader.elf
set $pc = 0x80000000
