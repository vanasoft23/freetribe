#monitor semihosting enable 0xFFFF0008
#monitor semihosting IOClient 2

#set debug remote 1

file ./build/bootloader/bootloader.elf
target remote \\.\COM10

symbol-file
restore C:/dev/System23/files/flash-dumps/E2_FACTORY_FIRMWARE.bin binary 0xC0000000

#monitor boot
