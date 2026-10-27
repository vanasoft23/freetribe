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
 * @file dev_usb.h
 *
 * @brief USB device-layer facade.
 */

#ifndef DEV_USB_H
#define DEV_USB_H

#include "ft.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	DEV_USB_MSC_EXPORT_OK = 0,
	DEV_USB_MSC_EXPORT_DISABLED,
	DEV_USB_MSC_EXPORT_NO_CARD,
	DEV_USB_MSC_EXPORT_INIT_FAILED,
	DEV_USB_MSC_EXPORT_NO_BLOCKS,
} t_dev_usb_msc_export_status;

bool dev_usb_init_device(u8 int_channel);
void dev_usb_task(void);
bool dev_usb_ready(void);
bool dev_usb_mounted(void);
bool dev_usb_cdc_connected(void);
u32 dev_usb_cdc_available(void);
u32 dev_usb_cdc_read(void *buffer, u32 length);
u32 dev_usb_cdc_write(const void *buffer, u32 length);
u32 dev_usb_vendor_write(const void *buffer, u32 length);
bool dev_usb_msc_export_sdcard_readonly(void);
void dev_usb_msc_disable(void);
bool dev_usb_msc_exported(void);
t_dev_usb_msc_export_status dev_usb_msc_export_status(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
