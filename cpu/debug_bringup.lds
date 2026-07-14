
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)


MEMORY
{
    OC_RAM (rwx)    : ORIGIN = 0x80000000, LENGTH = 0x20000
    ARM_RAM (rwx)   : ORIGIN = 0xFFFF0000, LENGTH = 0x2000
    DDR2_RAM (rw)   : ORIGIN = 0xC0000000, LENGTH = 64M
}

SECTIONS
{

    .text : ALIGN(4)
    {
        KEEP(*(.text.start))
        *(.text*)
        *(.rodata*)
    } > OC_RAM

    .data : ALIGN(4)
    {
        *(.data)
    } > OC_RAM

}
