/*
 * Linker script for the AM1802 MMC/SD hardware-test image.
 *
 * debug_bringup initializes DDR before GDB loads this image at 0xC0000000.
 * The test startup keeps interrupts disabled and runs entirely from DDR.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(start)

HEAP_SIZE = 0x1000;
STACK_SIZE = 0x4000;

MEMORY
{
    DDR2_RAM (rwx) : ORIGIN = 0xC0000000, LENGTH = 64M
}

SECTIONS
{
    . = ORIGIN(DDR2_RAM);

    .rsthand : ALIGN(4)
    {
        KEEP(*init.S.o(.text*))
    } > DDR2_RAM

    .text : ALIGN(4)
    {
        *(.text*)
        *(.rodata*)
        *(.glue_7*)
        *(.glue_7t*)
    } > DDR2_RAM

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    } > DDR2_RAM

    .data : ALIGN(4)
    {
        *(.data*)
    } > DDR2_RAM

    . = ALIGN(8);
    _bss_start = .;

    .bss (NOLOAD) :
    {
        *(.bss*)
        *(COMMON)
    } > DDR2_RAM

    . = ALIGN(8);
    _bss_end = .;

    .heap (NOLOAD) :
    {
        _heap_start = .;
        . += HEAP_SIZE;
        _heap_end = .;
    } > DDR2_RAM

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        _stack_low = .;
        . += STACK_SIZE;
        _stack = . - 4;
    } > DDR2_RAM
}
