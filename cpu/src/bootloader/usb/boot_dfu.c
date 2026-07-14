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
 * @file    boot_dfu.c
 *
 * @brief   TinyUSB DFU download handler for receiving firmware into reserved DDR2 memory.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>
#include <string.h>

#include "macros.h"
#include "tusb.h"

#include "handoff.h"
#include "boot_image.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

extern uint8_t FIRMWARE_RESERVED_SIZE;
extern uint8_t _ddr2_start;

/*----- Static variable definitions ----------------------------------*/

static uint32_t s_received_size = 0;
static uint8_t s_status = DFU_STATUS_OK;

/*----- Static function prototypes -----------------------------------*/

static uintptr_t _firmware_base(void);
static uint32_t _dfu_xfer_size(void);
static uint32_t _firmware_max_size(void);
static void _prepare_dfu_payload(const boot_image_info_t *boot_image);

/*----- Extern function implementations ------------------------------*/

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
    (void)alt;
    (void)state;

    return 0;
}

void tud_dfu_download_cb(uint8_t alt,
                         uint16_t block_num,
                         uint8_t const *data,
                         uint16_t length) {
    
    const uint32_t offset = (uint32_t)block_num * _dfu_xfer_size();
    const uint32_t max_size = _firmware_max_size();

    // Start a new firmware image download
    if (block_num == 0u) {
        s_received_size = 0;
        s_status = DFU_STATUS_OK;
    }

    // Reject an unsupported or out-of-range request
    if ((alt != 0u) || (data == NULL) || (offset > max_size) ||
        ((uint32_t)length > (max_size - offset))) {
        s_status = DFU_STATUS_ERR_ADDRESS;
        tud_dfu_finish_flashing(DFU_STATUS_ERR_ADDRESS);
        return;
    }

    // Store the received download block
    memcpy((void *)(_firmware_base() + offset), data, length);

    // Track the highest received image byte
    if ((offset + length) > s_received_size) {
        s_received_size = offset + length;
    }

    s_status = DFU_STATUS_OK;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

/**
 * @brief   Called when entire download has finished.
 */
void tud_dfu_manifest_cb(uint8_t alt) {
    boot_image_info_t boot_image;
    uint8_t *firmware = (uint8_t *)_firmware_base();

    (void)alt;

    DEBUG_LOG("boot DFU received %u bytes at 0x%08X",
              (unsigned)s_received_size, (unsigned)_firmware_base());

    if (DFU_STATUS_OK != s_status) {
        tud_dfu_finish_flashing(s_status);
        return;
    }

    if (!boot_image_classify_memory(firmware, s_received_size, &boot_image)) {
        DEBUG_LOG("boot DFU invalid image size=%u", (unsigned)s_received_size);
        s_status = DFU_STATUS_ERR_FILE;
        tud_dfu_finish_flashing(s_status);
        return;
    }

    DEBUG_LOG(
        "boot DFU image type: %s offset=%u payload=%u",
        boot_image_type_name(boot_image.type),
        (unsigned)boot_image.payload_offset,
        (unsigned)boot_image.payload_size
    );

    _prepare_dfu_payload(&boot_image);

    boot_image_handoff(&boot_image);
    
    tud_dfu_finish_flashing(s_status);

}

void tud_dfu_abort_cb(uint8_t alt) {
    (void)alt;

    s_status = DFU_STATUS_OK;
}

void tud_dfu_detach_cb(void) {
}

/*----- Static function implementations ------------------------------*/

static uintptr_t _firmware_base(void) {
    return (uintptr_t)&_ddr2_start;
}

static uint32_t _dfu_xfer_size(void) {
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? BOOT_DFU_HS_XFER_BUFSIZE
                                                : BOOT_DFU_FS_XFER_BUFSIZE;
}

static uint32_t _firmware_max_size(void) {
    return (uint32_t)(uintptr_t)&FIRMWARE_RESERVED_SIZE;
}

static void _prepare_dfu_payload(const boot_image_info_t *boot_image) {

    uint8_t *firmware = (uint8_t *)_firmware_base();

    if (0 == boot_image->payload_offset) {
        return;
    }

    memmove(
        firmware,
        firmware + boot_image->payload_offset,
        boot_image->payload_size
    );

}
/*----- End of file --------------------------------------------------*/
