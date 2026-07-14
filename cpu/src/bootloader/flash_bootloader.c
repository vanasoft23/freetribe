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
 * @file    flash_bootloader.c
 *
 * @brief   Functionality to flash bootloader.
 * 
 * @details During our bootloader startup phase, a mirror of the bootloader image is copied from SRAM to
 *          DDR. Up and until this happens, no static data may be modified, in other words: only stack
 *          is allowed to be used. Why? Anything in the .data section in SRAM may be subject to change
 *          during runtime, making it impossible for us to treat the SRAM contents as a bootloader image.
 */

/*----- Includes -----------------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "macros.h"

#include "dev_flash.h"

#include "flash_bootloader.h"

/*----- Macros -------------------------------------------------------*/

#define WRITE_RETRIES  3
#define REPAIR_RETRIES 5

#define SRAM_START         0x80000000u
#define CHECKSUM_LENGTH    2u

#ifdef DEBUG_BOOTLOADER_INSTALL
#  define BOOTSECT_START     0x00F00000u /* safe address in flash */
#else
#  define BOOTSECT_START     0x00000000u
#endif
#define BOOTSECT_SIZE      (128u*1024u)
#define SBL_SIZE           ((uint32_t)(uintptr_t)&__sbl_max_size)

/*----- Extern variable definitions ----------------------------------*/

extern uint8_t __sbl_max_size;

/*----- Static variable definitions ----------------------------------*/

static uint8_t ais_head[] __attribute__((section(".ais_head"), used)) = {
        // "TIPA": AIS bootscript magic
        0x54, 0x49, 0x50, 0x41,
        // Sequential Read Enable
        0x63, 0x59, 0x53, 0x58,
        // Function Execute 6: PLL and Clock Configuration
        0x0D, 0x59, 0x53, 0x58, 0x06, 0x00, 0x03, 0x00, 
        0x01, 0x00, 0x18, 0x00, 0x05, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        // Section Load: With this command we load our own SBL into SRAM at 0x80000000.
        0x01, 0x59, 0x53, 0x58,
        0x00, 0x00, 0x00, 0x80,
        0x00,0x00,0x00,0x00 // <-- section size, gets replaced at runtime
    };

static const uint8_t ais_tail[] __attribute__((section(".ais_tail"), used)) = {
        // Jump and Close command
        0x06, 0x59, 0x53, 0x58, 0x00, 0x00, 0x00, 0x80, 
    };

__attribute__((section(".ddr_data"), aligned(8)))
    static uint8_t s_orig_bootsect[BOOTSECT_SIZE];

__attribute__((section(".ddr_data"), aligned(8)))
    static uint8_t s_new_bootsect[BOOTSECT_SIZE];

/*----- Static function declarations ---------------------------------*/

static uint16_t       _calc_checksum(uint8_t *buffer, uint32_t len);
static bool           _verify_bootsection(uint8_t *data);
static t_flash_result _backup_orig_bootsect(uint8_t *dest);
static void           _construct_new_bootsect(void);
static bool           _attempt_write(uint32_t dest, uint8_t *p_src, uint32_t len, int max_retries);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Copy bootloader runtime to DDR for later use.
 */
void create_bootloader_ddr_mirror(void) {
    
    uint32_t dest = ((uint32_t)s_new_bootsect) + sizeof(ais_head);
    memcpy((void*)dest, (void*)SRAM_START, SBL_SIZE);
}

/**
 * @brief   Install bootloader into first 128 KiB of flash.
 * @details The boot section exists of 3 parts:
 *          1. AIS script beginning.
 *          2. our SBL payload, which is part of AIS script.
 *          3. AIS script ending, which is the instruction to jump to our SBL.
 */
t_flash_result install_bootloader_to_flash(void) {
    
    t_flash_result res;

    res = _backup_orig_bootsect(s_orig_bootsect);
    if (res != FLASH_SUCCESS) return res;

    _construct_new_bootsect();
    
    bool success = _attempt_write(BOOTSECT_START, s_new_bootsect, BOOTSECT_SIZE, WRITE_RETRIES);
    if (!success) {
        // Try to repair...
        success = _attempt_write(BOOTSECT_START, s_orig_bootsect, BOOTSECT_SIZE, REPAIR_RETRIES);
        return success ? FLASH_WRITE_REPAIRED : FLASH_WRITE_CORRUPTED;
    }

    return FLASH_SUCCESS;

}


/*----- Static function implementations ------------------------------*/

/**
 * @brief   Calculate little endian 16-bit word additive checksum over a buffer.
 */
static uint16_t _calc_checksum(uint8_t *buffer, uint32_t len) {

    uint16_t sum = 0;

    for (uint32_t i = 0; i < len; i += CHECKSUM_LENGTH) {
        uint16_t word = buffer[i] | ((uint16_t)buffer[i+1] << 8);
        sum += word;
    }

    return sum;
}


/**
 * @brief   Verifies boot section against it's checksum.
 * 
 * @details Factory boot section puts an LE 16-bit word checksum at the end.
 */
static bool _verify_bootsection(uint8_t *data) {

    uint16_t sum = _calc_checksum(data, BOOTSECT_SIZE);
    DEBUG_LOG("Sum is %u", (unsigned int)sum);
    return (sum == 0);
}


static t_flash_result _backup_orig_bootsect(uint8_t *dest) {

    const int max_retries = 3;

    for (int i = 0; i < max_retries; i++) {

        dev_flash_read(BOOTSECT_START, dest, BOOTSECT_SIZE);

#ifndef NO_BOOTSECT_VERIFY /* maybe make this a bootloader config ini section? */
        if (_verify_bootsection(dest)) {
            return FLASH_SUCCESS;
        }
#else
        return FLASH_SUCCESS;
#endif
    
    }

    return FLASH_READ_FAILED; // invalid checksum or flash read failure
}

/**
 * @brief   Paste boot section header and tail, and a checksum around the new bootsection's memory object.
 */
static void _construct_new_bootsect(void) {

    // put SBL size in ais head Section Load cmd param
    *((uint32_t*)&ais_head[sizeof(ais_head) - sizeof(uint32_t)]) = SBL_SIZE;

    // Copy AIS head
    memcpy(s_new_bootsect, ais_head, sizeof(ais_head));

    // (SBL payload is already copied at startup)

    // Copy AIS tail
    memcpy(s_new_bootsect + sizeof(ais_head) + SBL_SIZE, ais_tail, sizeof(ais_tail));

    // Write checksum
    uint32_t ais_total_size = sizeof(ais_head) + SBL_SIZE + sizeof(ais_tail);
    uint16_t *sum_ptr       = (uint16_t*)(s_new_bootsect + ais_total_size);
    *sum_ptr = _calc_checksum(s_new_bootsect, ais_total_size);
}


/**
 * @brief   Tries to write given data to flash, verifies successful write, and retries on failure.
 * 
 * @returns true on success, false on failure
 */
static bool _attempt_write(uint32_t dest, uint8_t *p_src, uint32_t len, int max_retries) {

    for (int i = 0; i < max_retries; i++) {

        dev_flash_write(dest, p_src, len);
        if (dev_flash_verify(dest, p_src, len)) {
            return true;
        }

    }

    return false;
}

