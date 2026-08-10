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
 * @file    flash_io.c
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "bootloader/ffconf.h"
#include <ff.h>

#include "dev_flash.h"

#include "flash_io.h"

/*----- Macros -------------------------------------------------------*/

#define SECTOR_SIZE (4u*1024u)
#define BLOCK_SIZE  (128u*1024u)  /* operation block size */

/*----- Static variable definitions ----------------------------------*/

static u8 s_cache[FLASH_TOTAL_SIZE];
static u8 s_dump_cache[BLOCK_SIZE];

/*----- Static function prototypes -----------------------------------*/

static flashio_status_t _cache_old_flash(u32 addr, u32 len, int max_retries);
static bool _try_write(u32 addr, u8 *p_src, u32 len, int max_retries);
static bool _try_read(u32 addr, u8 *p_dest, u32 len, int max_retries);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Dump the entire flash contents to SD card.
 */
flashio_status_t flashio_dump_sdcard(
    const char           *filepath,
    flashio_progress_cb_t callback)
{
    //
    // Invalid request safeguard
    //
    if (NULL == filepath)
    {
        return FLASHIO_INVALID_PARAM;
    }

    //
    // Open flash dump file
    //
    FIL file;
    FRESULT res;
    res = f_open(&file, filepath, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != res) {
        return FLASHIO_SDCARD_FAIL;
    }

    //
    // Read/write loop
    //
    u32 offset = 0;

    while (offset < FLASH_TOTAL_SIZE) {
        
        if (callback)
            callback(offset, FLASH_TOTAL_SIZE);

        u32 remaining = FLASH_TOTAL_SIZE - offset;
        u32 chunk_size = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        // Read chunk
        bool success = _try_read(offset, s_dump_cache, chunk_size, 2);
        if (!success) {
            f_close(&file);
            return FLASHIO_READ_FAILED;
        }

        // Write chunk to SDcard
        UINT bytes_written;
        res = f_write(&file, s_dump_cache, (UINT)chunk_size, &bytes_written);
        if ((FR_OK != res) || (bytes_written != chunk_size)) {
            return FLASHIO_SDCARD_FAIL;
        }

        offset += chunk_size;

    }

    if (callback)
        callback(offset, FLASH_TOTAL_SIZE);

    //
    // Flush file
    //
    res = f_close(&file);
    if (FR_OK != res) {
        return FLASHIO_SDCARD_FAIL;
    }
    return FLASHIO_SUCCESS;
}


/**
 * @brief   Read flash in blocks of 128 KiB. Reads each block twice to verify validity.
 */
flashio_status_t flashio_read_safe(
    u32                   src_addr,
    u8                   *p_dest,
    u32                   len,
    int                   max_retries,
    flashio_progress_cb_t callback)
{
    if ((len == 0)
     || (max_retries <= 0)
     || (src_addr > FLASH_TOTAL_SIZE)
     || (len > FLASH_TOTAL_SIZE - src_addr)
     || (NULL == p_dest))
    {
        return FLASHIO_INVALID_PARAM;
    }

    u32 offset = 0;

    while (offset < len) {
        
        if (callback)
            callback(offset, len);

        u32 remaining = len - offset;
        u32 chunk_len = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        bool success = _try_read(src_addr+offset, p_dest+offset, chunk_len, max_retries);
        if (!success) {
            return FLASHIO_READ_FAILED;
        }

        offset += chunk_len;

    }

    return FLASHIO_SUCCESS;
}


/**
 * @brief   Write flash in blocks of 128 KiB. Retries if needed.
 */
flashio_status_t flashio_write_safe(
    u32                   dest_addr,
    u8                   *p_src,
    u32                   len,
    int                   max_retries,
    flashio_progress_cb_t callback)
{
    //
    // Invalid function call safeguard
    //
    if ((len == 0)
     || (dest_addr > FLASH_TOTAL_SIZE)
     || (len > FLASH_TOTAL_SIZE - dest_addr)
     || (NULL == p_src))
    {
        return FLASHIO_INVALID_PARAM;
    }
    
    //
    // Make backup in case of repair
    //
    u32 request_end = dest_addr + len;

    u32 backup_start = dest_addr - (dest_addr % SECTOR_SIZE);

    u32 backup_end = request_end;
    u32 end_offset = backup_end % SECTOR_SIZE;

    if (end_offset != 0) {
        backup_end += SECTOR_SIZE - end_offset;
    }

    u32 backup_len = backup_end - backup_start;

    flashio_status_t status = _cache_old_flash(backup_start, backup_len, max_retries);
    if (FLASHIO_SUCCESS != status) {
        return status;
    }

    //
    // Actual write starts
    //
    bool success  = false;
    u32  offset   = 0;

    while (offset < len) {
        
        if (callback)
            callback(offset, len);

        u32 remaining = len - offset;
        u32 chunk_len = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;

        success = _try_write(dest_addr+offset, p_src+offset, chunk_len, max_retries);

        offset += chunk_len;

        if (!success) {
            break;
        }
    }

    if (success) {
        return FLASHIO_SUCCESS;
    }

    //
    // On failure, hopefully repair up until where it did work..
    //
    if (!_try_write(backup_start, &s_cache[backup_start], backup_len, max_retries)) {
        return FLASHIO_WRITE_CORRUPT;
    }
    return FLASHIO_WRITE_REPAIRED;

}

/*----- Static function implementations ------------------------------*/

static flashio_status_t _cache_old_flash(u32 addr, u32 len, int max_retries)
{
    for (int retry = 0; retry < max_retries; retry++) {

        dev_flash_read(addr, &s_cache[addr], len);
        if (dev_flash_verify(addr, &s_cache[addr], len)) {
            return FLASHIO_SUCCESS;
        }
    }
    return FLASHIO_READ_FAILED;

}

static bool _try_write(u32 addr, u8 *p_src, u32 len, int max_retries)
{
    for (int retry = 0; retry < max_retries; retry++) {

        dev_flash_write(addr, p_src, len);
        if (dev_flash_verify(addr, p_src, len)) {
            return true;
        }

    }

    return false;
}

static bool _try_read(u32 addr, u8 *p_dest, u32 len, int max_retries) {

    for (int retry = 0; retry < max_retries; retry++) {

        dev_flash_read(addr, p_dest, len);
        dev_flash_read(addr, &s_cache[addr], len);
        if (0 == memcmp(p_dest, &s_cache[addr], len)) {
            return true;
        }

    }

    return false;
}
