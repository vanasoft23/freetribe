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
 * @file    firmware.c
 * 
 * @brief   Synchronous firmware loading.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "bootloader/ffconf.h"
#include <ff.h>

#include "dev_flash.h"

#include "service/boot_image.h"
#include "service/firmware.h"
#include "util/pathstr.h"

/*----- Macros -------------------------------------------------------*/

#define FIRMWARE_FLASH_ADDR      0x00020000u
#define FIRMWARE_DEST_ADDR       0xC0000000u
#define FIRMWARE_READ_CHUNK_SIZE (64u * 1024u)

/*----- Static variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static firmware_result_t _detect_boot_file(
    FIL               *file,
    const char        *name,
    u32                file_size,
    boot_image_info_t *boot_image
);
static bool _read_exact(FIL *file, void *dest, UINT size);
static firmware_result_t _read_sd_payload(
    FIL                    *file,
    const char             *path,
    u32                     size,
    fw_progress_callback_t  progress_callback
);
static u32 _firmware_read_chunk_size(u32 offset, u32 total);
static void _report_progress(
    fw_progress_callback_t callback,
    u32                    done,
    u32                    total
);

/*----- Extern function implementations ------------------------------*/

void fw_load_flash(fw_progress_callback_t progress_callback) {

    u8 *dest = (u8*)FIRMWARE_DEST_ADDR;
    u32 offset = 0;

    _report_progress(progress_callback, 0, BOOT_IMAGE_FACTORY_PAYLOAD_SIZE);

    while (offset < BOOT_IMAGE_FACTORY_PAYLOAD_SIZE) {
        u32 chunk_size = _firmware_read_chunk_size(
            offset,
            BOOT_IMAGE_FACTORY_PAYLOAD_SIZE
        );

        dev_flash_read(FIRMWARE_FLASH_ADDR + offset, dest + offset, chunk_size);

        offset += chunk_size;
        _report_progress(
            progress_callback,
            offset,
            BOOT_IMAGE_FACTORY_PAYLOAD_SIZE
        );
    }

}

firmware_result_t fw_load_sdcard(
    const char                      *path,
    u32                              file_size,
    fw_progress_callback_t  progress_callback,
    boot_image_info_t               *boot_image
) {

    firmware_result_t result;
    FRESULT res;
    FIL file;

    res = f_open(&file, path, FA_READ);
    if (FR_OK != res) {
        DEBUG_LOG("firmware open failed: %s res=%d", path, (int)res);
        return FW_OPEN_FAILED;
    }

    result = _detect_boot_file(&file, path, file_size, boot_image);
    if (FW_SUCCESS != result) {
        f_close(&file);
        return result;
    }

    DEBUG_LOG(
        "Boot file type: %s offset=%u payload=%u",
        boot_image_type_name(boot_image->type),
        (unsigned)boot_image->payload_offset,
        (unsigned)boot_image->payload_size
    );

    res = f_lseek(&file, boot_image->payload_offset);
    if (FR_OK != res) {
        f_close(&file);
        return FW_READ_FAILED;
    }

    result = _read_sd_payload(
        &file,
        path,
        boot_image->payload_size,
        progress_callback
    );
    if (FW_SUCCESS != result) {
        f_close(&file);
        return result;
    }

    res = f_close(&file);
    if (FR_OK != res) {
        DEBUG_LOG("firmware close failed: %s res=%d", path, (int)res);
        return FW_CLOSE_FAILED;
    }

    DEBUG_LOG("Boot file bytes read: %u", (unsigned)boot_image->payload_size);
    return FW_SUCCESS;

}


const char *fw_result_str(firmware_result_t result) {

    switch (result) {
    case FW_INVALID_FIRMWARE:    return "Invalid firmware";
    case FW_OPEN_FAILED:         return "Open failed";
    case FW_READ_FAILED:         return "Read failed";
    case FW_WRITE_FAILED:        return "Write failed";
    case FW_CLOSE_FAILED:        return "Close failed";
    case FW_SUCCESS:
    default:                     return "Operation failed";
    }

}


/*----- Static function implementations ------------------------------*/

static firmware_result_t _detect_boot_file(
    FIL               *file,
    const char        *name,
    u32                file_size,
    boot_image_info_t *boot_image
) {

    u8 prefix[BOOT_IMAGE_VSB_HEADER_SIZE];
    u8 trailer[BOOT_IMAGE_FREETRIBE_TRAILER_SIZE];
    u32 prefix_size = file_size;
    u32 trailer_size = 0;
    const u8 *trailer_ptr = NULL;

    if (!path_has_extension(name, ".bin") && !path_has_extension(name, ".VSB")) {
        return FW_INVALID_FIRMWARE;
    }

    if (prefix_size > BOOT_IMAGE_VSB_HEADER_SIZE) {
        prefix_size = BOOT_IMAGE_VSB_HEADER_SIZE;
    }

    if ((FR_OK != f_lseek(file, 0)) || ((prefix_size > 0) && !_read_exact(file, prefix, (UINT)prefix_size))) {
        return FW_READ_FAILED;
    }

    if (file_size >= BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) {
        trailer_size = BOOT_IMAGE_FREETRIBE_TRAILER_SIZE;
        trailer_ptr = trailer;

        if ((f_lseek(file, file_size - BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) != FR_OK)
         || !_read_exact(file, trailer, BOOT_IMAGE_FREETRIBE_TRAILER_SIZE)) {
            return FW_READ_FAILED;
        }
    }

    if (!boot_image_classify_parts(
            prefix,
            prefix_size,
            trailer_ptr,
            trailer_size,
            file_size,
            boot_image
        ))
    {
        DEBUG_LOG("not bootable: %s", name);
        return FW_INVALID_FIRMWARE;
    }

    return FW_SUCCESS;

}

static bool _read_exact(FIL *file, void *dest, UINT size) {

    FRESULT res;
    UINT bytes_read = 0;

    res = f_read(file, dest, size, &bytes_read);
    if (FR_OK != res) {
        DEBUG_LOG("f_read failed: res=%d", (int)res);
    }

    return (FR_OK == res) && (bytes_read == size);

}


static firmware_result_t _read_sd_payload(
    FIL                             *file,
    const char                      *path,
    u32                              size,
    fw_progress_callback_t  progress_callback
) {

    u8 *dest = (u8*)FIRMWARE_DEST_ADDR;
    u32 offset = 0;

    _report_progress(progress_callback, 0, size);

    while (offset < size) {
        FRESULT res;
        UINT    bytes_read = 0;
        u32     chunk_size = _firmware_read_chunk_size(offset, size);

        res = f_read(file, dest + offset, (UINT)chunk_size, &bytes_read);
        if ((FR_OK != res) || (bytes_read != chunk_size)) {
            DEBUG_LOG(
                "firmware read failed: %s res=%d bytes=%u/%u",
                path,
                (int)res,
                (unsigned)bytes_read,
                (unsigned)chunk_size
            );
            return FW_READ_FAILED;
        }

        offset += chunk_size;
        _report_progress(progress_callback, offset, size);
    }

    return FW_SUCCESS;

}

static u32 _firmware_read_chunk_size(u32 offset, u32 total) {

    u32 remaining = total - offset;

    if (remaining > FIRMWARE_READ_CHUNK_SIZE) {
        return FIRMWARE_READ_CHUNK_SIZE;
    }

    return remaining;

}

static void _report_progress(
    fw_progress_callback_t callback,
    u32                    done,
    u32                    total
) {

    if (NULL != callback) {
        callback(done, total);
    }

}
