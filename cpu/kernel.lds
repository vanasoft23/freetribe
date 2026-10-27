/*
 * Linker script for Freetribe.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)

VECTOR_TABLE_SIZE = 0x80;

MEMORY
{
	ARM_RAM (rwx)   : ORIGIN = 0xFFFF0000, LENGTH = 0x2000
	OC_RAM (rwx)    : ORIGIN = 0x80000000, LENGTH = 0x20000
	EXT_RAM (rwx)   : ORIGIN = 0xC0000000, LENGTH = 0x4000000
}

SECTIONS
{
	
	/***************************************************************************
	 *
	 * ARM926EJ-S internal RAM
	 *
	 **************************************************************************/

	. = ORIGIN(ARM_RAM);
	
	/*
	 * Vector table is moved to 
	 * ARM RAM by startup code.
	 */
	.vector_table (NOLOAD) :
	{
		. = ALIGN(8); 
		_vector_table = .;
		. += VECTOR_TABLE_SIZE; 
	} > ARM_RAM

	/***************************************************************************
	 *
	 * AM1802 On-Chip RAM
	 *
	 **************************************************************************/

	. = ORIGIN(ARM_RAM);

	.page_table (NOLOAD) :
	{
		KEEP(*(.page_table))
	} > OC_RAM
	
	ASSERT((ADDR(.page_table) & (16K - 1)) == 0,
		"Page table start must be 16 KiB aligned")
	
	/**
	 * .stack is the section ARM banked stack pointers:
	 * USR/SYS, SVC, IRQ, FIQ, ABT, UND
	 *
	 * Please note that FreeRTOS task stacks are placed in heap!
	 */

	.stack ALIGN(8) (NOLOAD):
	{
		_stack_start = .;

		/* no need for USR/SYS stack; FreeRTOS handles it in heap */

		/* SVC - 4 KiB */
		_svc_stack_low = .;
		. += 4K;
		_svc_stack_top = .;

		/* IRQ - 4 KiB */
		_irq_stack_low = .;
		. += 4K;
		_irq_stack_top = .;

		/* FIQ - 1 KiB */
		_fiq_stack_low = .;
		. += 1K;
		_fiq_stack_top = .;

		/* UND - 1 KiB */
		_und_stack_low = .;
		. += 1K;
		_und_stack_top = .;

		/* ABT - 1 KiB */
		_abt_stack_low = .;
		. += 1K;
		_abt_stack_top = .;

		_stack_end = .;
	} > OC_RAM

	/***************************************************************************
	 *
	 * External DDR2
	 *
	 **************************************************************************/
	
	. = 0xC0000000;
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
	
	.bss ALIGN(8) (NOLOAD) :
	{
		_bss_start = .;
		*(.bss*)
		*(COMMON)
		. = ALIGN(4); /* Startup clears BSS in whole words. */
		_bss_end = .;
	} > EXT_RAM

	.heap ALIGN(8) (NOLOAD) :
	{
		_heap_start = .;
		KEEP(*(.heap))
		. = ALIGN(4);
		_heap_end = .;
	} > EXT_RAM

}
