/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

 Freetribe is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
                  (at your option) any later version.

     Freetribe is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    boot_section.c
 *
 * @brief   SBL functionality.
 * 
 * @details During our SBL startup phase, a backup of the SBL image is copied from SRAM to
 *          DDR. Up and until this happens, no static data may be modified, in other words: only stack
 *          is allowed to be used. Why? Anything in the .data section in SRAM may be subject to change
 *          during runtime, making it impossible for us to treat the SRAM contents as a bootloader image.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "flash_io.h"
#include "boot_section.h"

/*----- Macros -------------------------------------------------------*/

#define SRAM_START         0x80000000u
#define CHECKSUM_LENGTH    2u

#ifdef DEBUG_BOOTLOADER_INSTALL
#  define BOOTSECT_START     0x00F00000u /* safe address in flash */
#else
#  define BOOTSECT_START     0x00000000u
#endif
#define BOOTSECT_SIZE      (128u*1024u)

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".ais_head"), used))
    static u8 ais_head[] = {
        // "TIPA": AIS bootscript magic
        0x54, 0x49, 0x50, 0x41,
        // Sequential Read Enable
        0x63, 0x59, 0x53, 0x58,
        // Function Execute 6: PLL and Clock Configuration
        0x0D, 0x59, 0x53, 0x58, 0x06, 0x00, 0x03, 0x00, 
        0x01, 0x00, 0x18, 0x00, 0x05, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        // Section Load: With this command we load our own SBL into SRAM at 0x80000000
        0x01, 0x59, 0x53, 0x58,
        0x00, 0x00, 0x00, 0x80,
        0x00,0x00,0x00,0x00 // <-- section size, gets replaced at runtime
    };

__attribute__((section(".ais_tail"), used))
    static const u8 ais_tail[] = {
        // Jump and Close command
        0x06, 0x59, 0x53, 0x58, 0x00, 0x00, 0x00, 0x80, 
    };

__attribute__((section(".ddr_data"), aligned(8)))
    u8 g_cached_bootsect[BOOTSECT_SIZE]; // contains runtime SBL copy!

__attribute__((section(".ddr_data"), aligned(8)))
    u8 s_verify_cache[BOOTSECT_SIZE - CHECKSUM_LENGTH];

/*----- Static function declarations ---------------------------------*/

static u16            _calc_checksum(u8 *buffer, u32 len);
static void           _construct_new_bootsect(void);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Copy bootloader runtime to DDR for later use.
 * @details Q: Why do we need a DDR mirror?
 *          A: Because data used by the bootloader is subject to change.
 */
void create_sbl_ddr_snapshot(void)
{
    memcpy((void*)CACHED_SBL_PTR, (void*)SRAM_START, SBL_SIZE);
}

/**
 * @brief   Install bootloader into first 128 KiB of flash.
 * @details The boot section exists of 4 parts:
 *          1. AIS script beginning.
 *          2. our SBL payload, which is part of AIS script.
 *          3. AIS script ending, which is the instruction to jump to our SBL.
 *          4. 16-bit checksum
 */
bootsect_res_t install_sbl_to_flash(void)
{
    _construct_new_bootsect();

// #ifndef NO_BOOTSECT_VERIFY
//     bootsect_res_t res = verify_bootsection();
//     if (BOOTSECT_VERIFY_CHECKSUM_OK != res) {
//         return res;
//     }
// #endif

    flashio_status_t flash_res;
    flash_res = flashio_write_safe(BOOTSECT_START,
                                   g_cached_bootsect,
                                   BOOTSECT_SIZE,
                                   3,
                                   NULL);
    switch (flash_res) {
    case FLASHIO_SUCCESS       : return BOOTSECT_INSTALL_SUCCESS;
    case FLASHIO_WRITE_CORRUPT : return BOOTSECT_INSTALL_FAIL_CORRUPTED;
    case FLASHIO_WRITE_REPAIRED: return BOOTSECT_INSTALL_FAIL_REPAIRED;
    default                    : return BOOTSECT_UNKNOWN_ERROR;
    }
}

/**
 * @brief   Verify whether the current bootsection is corrupt according
 *          to it's checksum.
 */
bootsect_res_t verify_bootsection(void)
{
    flashio_status_t st = flashio_read_safe(BOOTSECT_START,
                                            s_verify_cache,
                                            sizeof(s_verify_cache),
                                            3,
                                            NULL);
    if (FLASHIO_SUCCESS != st) {
        return BOOTSECT_VERIFY_ERROR;
    }
    
    u16 sum = _calc_checksum(s_verify_cache, sizeof(s_verify_cache));
    if (0 == sum) {
        return BOOTSECT_VERIFY_CHECKSUM_OK;
    } else {
        return BOOTSECT_VERIFY_CHECKSUM_BAD;
    }
    
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Calculate little endian 16-bit word additive checksum over a buffer.
 */
static u16 _calc_checksum(u8 *buffer, u32 len) {

    u16 sum = 0;

    for (u32 i = 0; i < len; i += CHECKSUM_LENGTH) {
        u16 word = buffer[i] | ((u16)buffer[i+1] << 8);
        sum += word;
    }

    return sum;
}

/**
 * @brief   Paste boot section header and tail, and a checksum around the new bootsection's memory object.
 */
static void _construct_new_bootsect(void) {

    // put SBL size in ais head Section Load cmd param
    *((u32*)&ais_head[sizeof(ais_head) - sizeof(u32)]) = SBL_SIZE;

    // Copy AIS head
    memcpy(g_cached_bootsect, ais_head, sizeof(ais_head));

    // (SBL payload is already copied at startup)

    // Copy AIS tail
    memcpy(g_cached_bootsect + sizeof(ais_head) + SBL_SIZE, ais_tail, sizeof(ais_tail));

    // Write checksum
    u32 ais_total_size = sizeof(ais_head) + SBL_SIZE + sizeof(ais_tail);
    u16 *sum_ptr       = (u16*)(g_cached_bootsect + ais_total_size);
    *sum_ptr = _calc_checksum(g_cached_bootsect, ais_total_size);
}
