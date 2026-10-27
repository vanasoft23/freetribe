OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)


VECTOR_TABLE_SIZE      = 0x80;
STACK_SIZE             = 0x1F80;
MMU_PAGE_TABLE_SIZE    = 0x4000;
OC_CODE_HEADROOM_SIZE  = 0x800;
BOOTSECT_SIZE          = 0x20000;
FLASHIO_CACHE_SIZE     = 16M;
DFU_STAGING_SIZE       = 16M;

MEMORY
{
    OC_RAM (rwx)         : ORIGIN = 0x80000000, LENGTH = 112K
    OC_MMU_RAM (rw)      : ORIGIN = 0x8001C000, LENGTH = 16K
    ARM_RAM (rwx)        : ORIGIN = 0xFFFF0000, LENGTH = 8K
    DDR2_RAM (rw)        : ORIGIN = 0xC0000000, LENGTH = 60M
    DDR2_RESERVED (rw)   : ORIGIN = 0xC3C00000, LENGTH = 4M
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

    ASSERT(. + OC_CODE_HEADROOM_SIZE <= ORIGIN(OC_MMU_RAM),
           "bootloader code leaves less than 2 KiB of OC RAM headroom"
    )

    .mmu_page_table ORIGIN(OC_MMU_RAM) (NOLOAD) :
    {
        _mmu_page_table_start = .;
        KEEP(*(.mmu_page_table))
        . = ALIGN(0x4000);
        _mmu_page_table_end = .;
    } > OC_MMU_RAM

    ASSERT(SIZEOF(.mmu_page_table) == MMU_PAGE_TABLE_SIZE,
           "bootloader MMU page table must occupy exactly 16 KiB")

    /***************************************************************************
     *
     * ARM RAM
     *
     **************************************************************************/

    . = ORIGIN(ARM_RAM);

    .vector_table (NOLOAD) : ALIGN(8)
    {
        _vector_table = .;
        . += VECTOR_TABLE_SIZE;
    } > ARM_RAM

    /* still grows towards vector table YIKES!!!!!! */
    ._stack (NOLOAD) :
    {
        _stack_low = .;
         . += STACK_SIZE;
        _stack = .;
    } > ARM_RAM

    /***************************************************************************
     *
     * DDR
     *
     **************************************************************************/

    _ddr2_start = ORIGIN(DDR2_RAM);
    _program_ddr_end = ORIGIN(DDR2_RESERVED);
    _ddr2_end = ORIGIN(DDR2_RESERVED) + LENGTH(DDR2_RESERVED);

    /* Bootloader-only scratch. ELF loading may overwrite this area. */
    .flashio_cache ORIGIN(DDR2_RAM) (NOLOAD) :
    {
        . = ALIGN(8);
        *(.flashio_cache)
        *(.flashio_cache.*)
        . = ALIGN(8);
    } > DDR2_RAM

    ASSERT(SIZEOF(.flashio_cache) == FLASHIO_CACHE_SIZE,
           "flash I/O rollback cache must occupy exactly 16 MiB")

    .dfu_staging (NOLOAD) :
    {
        . = ALIGN(8);
        *(.dfu_staging)
        *(.dfu_staging.*)
        . = ALIGN(8);
    } > DDR2_RAM

    ASSERT(SIZEOF(.dfu_staging) == DFU_STAGING_SIZE,
           "DFU staging buffer must occupy exactly 16 MiB")

    ASSERT(. <= _program_ddr_end,
           "bootloader scratch buffers exceed the program DDR region")

    /* Runtime state that must survive until the final program handoff. */
    .bss ORIGIN(DDR2_RESERVED) (NOLOAD) :
    {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        _bss_end = .;
    } > DDR2_RESERVED

    .bootsect_cache (NOLOAD) :
    {
        . = ALIGN(8);
        *(.bootsect_cache)
        *(.bootsect_cache.*)
        . = ALIGN(8);
    } > DDR2_RESERVED

    ASSERT(SIZEOF(.bootsect_cache) == BOOTSECT_SIZE,
           "boot section cache must occupy exactly 128 KiB")

    ASSERT(. <= _ddr2_end,
           "bootloader BSS and persistent caches exceed reserved DDR")

    /*
     * Give the raw binary a linker-defined end without overlapping the
     * MMU page table's VMA. objcopy uses this section's OC RAM load address.
     */
    /* .sbl_image_end : AT(ORIGIN(OC_RAM) + __sbl_max_size - 1)
    {
        BYTE(0);
    } > DDR2_RESERVED */

}
