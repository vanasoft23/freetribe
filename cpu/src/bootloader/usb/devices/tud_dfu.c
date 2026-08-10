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
 * @file    tud_dfu.c
 *
 * @brief   TinyUSB DFU download handler for receiving firmware into reserved DDR2 memory.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <class/dfu/dfu_device.h>
#include <tusb.h>

#include "flash_io.h"
#include "service/boot_image.h"
#include "service/boot_section.h"
#include "service/handoff.h"
#include "ui/ui_controller.h"
#include "usb/boot_usb.h"
#include "gdb_monitor.h"

/*----- Macros -------------------------------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- DFU alts -----------------------------------------------------*/

typedef struct dfu_alt_entry dfu_alt_entry_t;
typedef void (*dfu_finish_cb_t)(dfu_alt_entry_t);
struct dfu_alt_entry {
    dfu_finish_cb_t  finish_cb;
    u8              *dest_ptr;
    u32              max_size;
};

static void _alt_reflash(dfu_alt_entry_t alt);
static void _alt_flash_bootloader(dfu_alt_entry_t alt);
static void _alt_flash_firmware(dfu_alt_entry_t alt);
static void _alt_debug_bootloader(dfu_alt_entry_t alt);
static void _alt_debug_firmware(dfu_alt_entry_t alt);

__attribute__((section(".ddr_data")))
    static dfu_alt_entry_t s_dfu_alt_table[DFU_ALT_COUNT];

static void _init_dfu_alt_table(void);

/*--------------------------------------------------------------------*/

static u32 s_received_size = 0;
static u8  s_status        = DFU_STATUS_OK;

static u32 _dfu_xfer_size(void);
static void _rebase_firmware_payload(const boot_image_info_t *boot_image);

/*----- Extern function implementations ------------------------------*/

void dfu_preinit() {
    _init_dfu_alt_table();
}

/**
 * @brief   Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
 *
 * @param   alt_id   Alt ID passed via command line
 * @param   state    Current state of DFU TUD
 * 
 * @returns Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
 *          During this period, USB host won't try to communicate with us.
 */
u32 tud_dfu_get_timeout_cb(u8 alt_id, u8 state) {
    (void)alt_id;
    (void)state;

    return 0;
}

/**
 * @brief   Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
 * 
 * @param   alt_id   Alt ID passed via command line
 * 
 * @details Once finished flashing, application must call tud_dfu_finish_flashing()
 *          This callback could be returned before flashing op is complete (async).
 */
void tud_dfu_download_cb(u8        alt_id,
                         u16       block_num,
                         u8 const *data,
                         u16       length) {

    if (!gdb_monitor_target_running()) {
        return;
    }
    
    const u32 offset = (u32)block_num * _dfu_xfer_size();

    // Reject an unsupported or out-of-range request
    dfu_alt_entry_t alt = s_dfu_alt_table[alt_id];
    bool invalid_req = false;
    invalid_req |= invalid_req || (alt_id >= DFU_ALT_COUNT) || (!alt.finish_cb);
    invalid_req |= invalid_req || (data == NULL);
    invalid_req |= invalid_req || (offset > alt.max_size);
    invalid_req |= invalid_req || ((u32)length > (alt.max_size - offset));
    if (invalid_req) {
        s_status = DFU_STATUS_ERR_UNKNOWN; // TODO: more specific status reporting
        tud_dfu_finish_flashing(s_status);
        return;
    }

    // Potentially start a new firmware image download
    if (block_num == 0u) {
        s_received_size = 0;
    }
    
    // Store the received download block
    memcpy((void *)(alt.dest_ptr + offset), data, length);

    // Track the highest received image byte
    if ((offset + length) > s_received_size) {
        s_received_size = offset + length;
    }

    tud_dfu_finish_flashing(DFU_STATUS_OK);
}


/**
 * @brief   Invoked when download process is complete, received DFU_DNLOAD (wLength=0) following by DFU_GETSTATUS (state=Manifest)
 *
 * @param   alt_id   Alt ID passed via command line
 * 
 * @details Application can do checksum, or actual flashing if buffered entire image previously.
 *          Once finished flashing, application must call tud_dfu_finish_flashing()
 */
void tud_dfu_manifest_cb(u8 alt_id) {

    // Validate command line --alt param
    dfu_alt_entry_t alt = s_dfu_alt_table[alt_id];
    if (!alt.finish_cb) { // valid entry check
        DEBUG_LOG("DFU invalid alt id passed");
    }

    DEBUG_LOG("boot DFU received %u bytes at 0x%08X",
              (unsigned)s_received_size, (unsigned)alt.dest_ptr);

    if (DFU_STATUS_OK != s_status) {
        tud_dfu_finish_flashing(s_status);
        return;
    }

    tud_dfu_finish_flashing(s_status);

    alt.finish_cb(alt);

}

/**
 * @brief   Invoked when the Host has terminated a download or upload transfer
 * 
 * @param   alt_id   Alt ID passed via command line
 */
void tud_dfu_abort_cb(u8 alt_id) {
    (void)alt_id;

    s_status = DFU_STATUS_OK;
}

/**
 * @brief  Invoked when a DFU_DETACH request is received
 */
void tud_dfu_detach_cb(void) {
}

/*----- Static function implementations ------------------------------*/

static u32 _dfu_xfer_size(void) {
    return (tud_speed_get() == TUSB_SPEED_HIGH) ? BOOT_DFU_HS_XFER_BUFSIZE
                                                : BOOT_DFU_FS_XFER_BUFSIZE;
}





static void _rebase_firmware_payload(const boot_image_info_t *boot_image) {

    u8 *firmware = (u8 *)DDR_FIRMWARE_BASE;

    if (0 == boot_image->payload_offset) {
        return;
    }

    memmove(
        firmware,
        firmware + boot_image->payload_offset,
        boot_image->payload_size
    );

}


/*----- DFU alts -----------------------------------------------------*/

static void _init_dfu_alt_table(void) {
    s_dfu_alt_table[ALT_FLASH_BOOTLOADER] = (dfu_alt_entry_t){ .finish_cb = _alt_flash_bootloader, .dest_ptr = CACHED_SBL_PTR        , .max_size = SBL_SIZE                        };
    s_dfu_alt_table[ALT_DEBUG_BOOTLOADER] = (dfu_alt_entry_t){ .finish_cb = _alt_debug_bootloader, .dest_ptr = CACHED_SBL_PTR        , .max_size = SBL_SIZE                        };
    s_dfu_alt_table[ALT_FLASH_FIRMWARE]   = (dfu_alt_entry_t){ .finish_cb = _alt_flash_firmware  , .dest_ptr = (u8*)DDR_FIRMWARE_BASE, .max_size = BOOT_IMAGE_FACTORY_PAYLOAD_SIZE };
    s_dfu_alt_table[ALT_DEBUG_FIRMWARE]   = (dfu_alt_entry_t){ .finish_cb = _alt_debug_firmware  , .dest_ptr = (u8*)DDR_FIRMWARE_BASE, .max_size = FIRMWARE_RESERVED_SIZE          };
    s_dfu_alt_table[ALT_REFLASH]          = (dfu_alt_entry_t){ .finish_cb = _alt_reflash         , .dest_ptr = (u8*)DDR_FIRMWARE_BASE, .max_size = FLASH_TOTAL_SIZE                };
}


static void _alt_reflash(dfu_alt_entry_t alt) {
    
    DEBUG_LOG("alt_reflash()...");

    DEBUG_LOG("NOT IMPLEMENTED.");
    // const int max_retries = 3;
    // for (int i = 0; i < max_retries; i++) {
    //     DEBUG_LOG("writing flash (try %i of %i)...", i, max_retries);
    //     dev_flash_write(0x00000000, alt.dest_ptr, s_received_size);
    //     if (!dev_flash_verify(0x00000000, alt.dest_ptr, s_received_size)) {
    //         DEBUG_LOG("ERROR: verification failed!");
    //     } else {
    //         break;
    //     }
    // }

    DEBUG_LOG("reflashing done.");

}

static void _alt_flash_bootloader(dfu_alt_entry_t alt) {

    DEBUG_LOG("_alt_flash_bootloader()...");
    ui_controller_run_install_sbl();

}

static void _alt_flash_firmware(dfu_alt_entry_t alt) {
    DEBUG_LOG("ERROR: Alt action not implemented");
}

static void _alt_debug_bootloader(dfu_alt_entry_t alt) {
    DEBUG_LOG("ERROR: Alt action not implemented");
}

static void _alt_debug_firmware(dfu_alt_entry_t alt) {

    DEBUG_LOG("_alt_debug_firmware()...");

    boot_image_info_t boot_image;
    if (!boot_image_classify_memory(alt.dest_ptr, s_received_size, &boot_image)) {
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

    _rebase_firmware_payload(&boot_image);
    boot_image_handoff(&boot_image);
}

/*----- End of file --------------------------------------------------*/
