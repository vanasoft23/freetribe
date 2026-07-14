/*
 * Linker script for Freetribe.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

HEAP_SIZE = 0x200000;
STACK_SIZE = 0x200000;
VECTOR_TABLE_SIZE = 0x80;

MEMORY
{
    OC_RAM (rwx)    : ORIGIN = 0x80000000, LENGTH = 0x20000
    EXT_RAM (rwx)   : ORIGIN = 0xC0000000, LENGTH = 0x4000000
    ARM_RAM (rwx)   : ORIGIN = 0xFFFF0000, LENGTH = 0x2000
}

SECTIONS
{
    . = 0xC0000000;
	. = ALIGN(4);
    _kernel_cacheable_start = .;

    .rsthand :
    {
        *init.S.o(.text*)
    } > EXT_RAM
 
	. = ALIGN(4);
	.text :
	{
	    *(.text*)
        *(.rodata*)
        *(.init*)
        *(.fini*)
        *(.iplt*)
        *(.rel.*)
        *(.glue_7*)
        *(.glue_7t*)
        *(.eh_frame*)
	} > EXT_RAM

	. = ALIGN(4);
    .ARM.exidx :
    {
        *(.ARM.exidx*)
        *(.gnu.linkonce.armexidx.*)
    } > EXT_RAM

    /*
     * The MMU uses 1 MiB section entries. Keep mutable data out of the
     * cacheable text/rodata section so D-cache can stay enabled safely.
     */
	.data ALIGN(0x100000) :
	{
        _kernel_cacheable_end = .;
	    *(.data*)
        *(.init_array*)
        *(.fini_array*)
	} > EXT_RAM
	
	. = ALIGN(4);

	_bss_start = .;
	.bss :
	{
	    *(.bss*)
        *(COMMON)
	} > EXT_RAM

    . = ALIGN(8);

	.freertos_heap (NOLOAD) :
	{
        . = ALIGN(8);
        _freertos_heap_start = .;
        KEEP(*(.freertos_heap))
        . = ALIGN(8);
        _freertos_heap_end = .;
	} > EXT_RAM

    . = ALIGN(8);

	_bss_end = .;
    end = .;
	
	.heap ALIGN(8) :
	{
        _heap_start = .;

	     . += HEAP_SIZE;

        _heap_end = .;
	} > EXT_RAM

	.stack :
	{
        . = ALIGN(8); 
        _stack_low = .;


	     . += STACK_SIZE; 

        _stack = .;
	} > EXT_RAM

    /*
     * Vector table is moved to 
     * ARM RAM by startup code.
     */
	.vector_table :
	{
        . = ALIGN(8); 
        _vector_table = .;


	    . += VECTOR_TABLE_SIZE; 

	} > ARM_RAM


}
