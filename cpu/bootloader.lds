
/* Note: currently, the stack is initially appointed to ARM RAM, but during
         the startup phase, it's being put into DDR.
*/

OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)


INITIAL_STACK_SIZE     = 0x1F80;
DDR_STACK_SIZE         = 0x200000;
VECTOR_TABLE_SIZE      = 0x80;
MMU_PAGE_TABLE_SIZE    = 0x4000;
MMU_PAGE_TABLE_ADDR    = ORIGIN(OC_RAM) + LENGTH(OC_RAM) - MMU_PAGE_TABLE_SIZE;
FIRMWARE_RESERVED_SIZE = 0x1000000; /* 16 MiB to support DFU reflash operation */
BOOTSECT_SIZE          = 0x20000;

MEMORY
{
    OC_RAM (rwx)    : ORIGIN = 0x80000000, LENGTH = 0x20000
    ARM_RAM (rwx)   : ORIGIN = 0xFFFF0000, LENGTH = 0x2000
    DDR2_RAM (rw)   : ORIGIN = 0xC0000000, LENGTH = 64M
}

SECTIONS
{

    /***************************************************************************
     *
     * OC RAM
     *
     **************************************************************************/
    
    .rsthand :
    {
        *init.S.o    (.text)
    } > OC_RAM

    .text : ALIGN(4)
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > OC_RAM

    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > OC_RAM

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > OC_RAM

    .data : ALIGN(4)
    {
        *(.data .data.*)
    } > OC_RAM

    /*
     * Must fit in boot section as AIS payload
     */
    __firmware_reserved_size = FIRMWARE_RESERVED_SIZE;
    .ais_head : { KEEP(*(.ais_head)) } > OC_RAM
    .ais_tail : { KEEP(*(.ais_tail)) } > OC_RAM
    __ais_head_size = SIZEOF(.ais_head);
    __ais_tail_size = SIZEOF(.ais_tail);
    __sbl_max_size =
        BOOTSECT_SIZE
        - SIZEOF(.ais_head)
        - SIZEOF(.ais_tail)
        - 2; /* 16-bit checksum */
    
    ASSERT(. - ORIGIN(OC_RAM) <= __sbl_max_size,
           "bootloader exceeds maximum image size and cannot fit inside AIS"
    )

    . = MMU_PAGE_TABLE_ADDR;

    .mmu_page_table (NOLOAD) :
    {
        _mmu_page_table_start = .;
        KEEP(*(.mmu_page_table))
        . = ALIGN(0x4000);
        _mmu_page_table_end = .;
    } > OC_RAM

    ASSERT(SIZEOF(.mmu_page_table) == MMU_PAGE_TABLE_SIZE,
           "bootloader MMU page table must occupy exactly 16 KiB")

    /***************************************************************************
     *
     * ARM RAM
     *
     **************************************************************************/

    . = ORIGIN(ARM_RAM);

    /*
     * TODO: write-protect this
     */
    .vector_table (NOLOAD) : ALIGN(8)
    {
        _vector_table = .;
        . += VECTOR_TABLE_SIZE;
    } > ARM_RAM

    .initial_stack (NOLOAD) :
    {
        _initial_stack_low = .;
         . += INITIAL_STACK_SIZE; 
        _initial_stack = .;
    } > ARM_RAM

    /***************************************************************************
     *
     * DDR
     *
     **************************************************************************/

    . = ORIGIN(DDR2_RAM) + FIRMWARE_RESERVED_SIZE;

    .ddr_data ALIGN(8) (NOLOAD) :
    {
        *(.ddr_data)
        *(.ddr_data.*)
    }

    .ddr_stack . (NOLOAD) :
	{
		. = ALIGN(8);
		_ddr_stack_low = .;
		. += DDR_STACK_SIZE;
		_ddr_stack = .;
	} > DDR2_RAM

    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > DDR2_RAM

    /*
     * Give the raw binary a linker-defined end without overlapping the
     * MMU page table's VMA. objcopy uses this section's OC RAM load address.
     */
    .sbl_image_end : AT(ORIGIN(OC_RAM) + __sbl_max_size - 1)
    {
        BYTE(0);
    } > DDR2_RAM

    _ddr2_start = ORIGIN(DDR2_RAM);
    _ddr2_end = ORIGIN(DDR2_RAM) + LENGTH(DDR2_RAM);

}
