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

#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUSB_DEBUG_PRINTF boot_tusb_printf

#define CFG_TUSB_MCU OPT_MCU_AM1802
#define CFG_TUSB_OS OPT_OS_NONE

#define TUP_USBIP_MUSB
#define TUP_USBIP_MUSB_AM1802
#define TUP_DCD_ENDPOINT_MAX 5

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_HIGH_SPEED)

#define CFG_TUD_ENABLED 1
#define CFG_TUH_ENABLED 0

#define CFG_TUD_ENDPOINT0_SIZE 64

#define CFG_TUD_CDC 0
#define CFG_TUD_DFU 1
#define CFG_TUD_MSC 0
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_TASK_QUEUE_SZ 16
#define CFG_TUD_DFU_XFER_BUFSIZE 16384
#define BOOT_DFU_FS_XFER_BUFSIZE 4096
#define BOOT_DFU_HS_XFER_BUFSIZE 4096
#define CFG_TUD_MSC_EP_BUFSIZE 512

#define CFG_TUSB_DEBUG 0

#endif

/*----- End of file --------------------------------------------------*/
