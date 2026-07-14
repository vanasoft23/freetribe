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
 * @file    flash_bootloader.h
 *
 * @brief   API for flashing bootloader functionality.
 */

#ifndef FLASH_BOOTLOADER_H
#define FLASH_BOOTLOADER_H

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    FLASH_SUCCESS,
    FLASH_READ_FAILED,
    FLASH_WRITE_REPAIRED,
    FLASH_WRITE_CORRUPTED,
} t_flash_result;

t_flash_result install_bootloader_to_flash(void);
void create_bootloader_ddr_mirror(void);

#endif