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
 * @file    svc_usb.c
 *
 * @brief   FreeRTOS-owned TinyUSB service.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "dev_usb.h"

#include "svc_fatfs.h"
#include "svc_usb.h"

/*----- Macros -------------------------------------------------------*/

// #define USB_INT_CHANNEL 11
// #define USB_TASK_PERIOD_MS 1

/*----- Static variable definitions ----------------------------------*/

struct usb_service {
	SemaphoreHandle_t mutex;
	volatile bool     ready;
	volatile bool     init_failed;
};

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void svc_usb_task(void *param) {

	(void)param;

}

bool svc_usb_ready(void) { return false; }

bool svc_usb_init_failed(void) { return true; }

bool svc_usb_mounted(void) {

	return false;
}

/*----- End of file --------------------------------------------------*/
