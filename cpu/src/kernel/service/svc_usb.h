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
 * @file    svc_usb.h
 *
 * @brief   USB service task.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef SVC_USB_H
#define SVC_USB_H

#include "ft.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SVC_USB_MSC_EXPORT_OK = 0,
    SVC_USB_MSC_EXPORT_DISABLED,
    SVC_USB_MSC_EXPORT_USB_NOT_READY,
    SVC_USB_MSC_EXPORT_SDCARD_BUSY,
    SVC_USB_MSC_EXPORT_NO_CARD,
    SVC_USB_MSC_EXPORT_SDCARD_INIT_FAILED,
    SVC_USB_MSC_EXPORT_NO_BLOCKS,
    SVC_USB_MSC_EXPORT_DEVICE_ERROR,
} t_svc_usb_msc_export_status;

void svc_usb_task(void *param);
bool svc_usb_ready(void);
bool svc_usb_init_failed(void);
bool svc_usb_mounted(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
