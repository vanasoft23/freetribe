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

#ifndef BOOT_USB_H
#define BOOT_USB_H

// TinyUSB token-pastes this value in TUD_DFU_DESCRIPTOR, so it must be
// a numeric preprocessor macro rather than an enum constant. D'oh!
#define DFU_ALT_COUNT 5

enum {
    ALT_FLASH_BOOTLOADER   = 0,
    ALT_FLASH_FIRMWARE     = 1,
    ALT_DEBUG_BOOTLOADER   = 2,
    ALT_DEBUG_FIRMWARE     = 3,
    ALT_REFLASH            = DFU_ALT_COUNT - 1,
};

void boot_usb_init(void);
void boot_usb_task(void);
void boot_usb_terminate(void);

#endif

/*----- End of file --------------------------------------------------*/
