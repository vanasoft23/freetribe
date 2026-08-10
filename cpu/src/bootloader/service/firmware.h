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

#ifndef FIRMWARE_H
#define FIRMWARE_H

#include "ft.h"

#include "boot_image.h"

typedef enum {
    FW_SUCCESS,
    FW_INVALID_FIRMWARE,
    FW_OPEN_FAILED,
    FW_READ_FAILED,
    FW_WRITE_FAILED,
    FW_CLOSE_FAILED
} firmware_result_t;

typedef void (*fw_progress_callback_t)(u32 done, u32 total);

void fw_load_flash(fw_progress_callback_t progress_callback);

firmware_result_t fw_load_sdcard(
    const char              *path,
    u32                      file_size,
    fw_progress_callback_t   progress_callback,
    boot_image_info_t       *boot_image
);

const char *fw_result_str(firmware_result_t result);

#endif
