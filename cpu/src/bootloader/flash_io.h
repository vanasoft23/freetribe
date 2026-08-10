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
 * @file   flash_io.h
 * 
 * @author vanasoft23
 */

#ifndef FLASH_IO_H
#define FLASH_IO_H

#include "ft.h"

#define FLASH_TOTAL_SIZE  (16u * 1024u * 1024u)

typedef enum {
    FLASHIO_SUCCESS,
    FLASHIO_INVALID_PARAM,
    FLASHIO_READ_FAILED,
    FLASHIO_WRITE_REPAIRED,
    FLASHIO_WRITE_CORRUPT,
    FLASHIO_SDCARD_FAIL
} flashio_status_t;

typedef void (*flashio_progress_cb_t)(u32 bcount, u32 total);

/**
 * @brief   Dump the entire flash contents to SD card.
 */
flashio_status_t flashio_dump_sdcard(
    const char           *filepath,
    flashio_progress_cb_t callback
);

/**
 * @brief   Read flash in blocks of 128 KiB. Reads each block twice to verify validity.
 */
flashio_status_t flashio_read_safe(
    u32                   src_addr,
    u8                   *p_dest,
    u32                   length,
    int                   max_retries,
    flashio_progress_cb_t callback
);

/**
 * @brief   Write flash in blocks of 128 KiB. Retries if needed.
 */
flashio_status_t flashio_write_safe(
    u32                   dest_addr,
    u8                   *p_src,
    u32                   length,
    int                   max_retries,
    flashio_progress_cb_t callback
);

#endif /* FLASH_IO_H */
