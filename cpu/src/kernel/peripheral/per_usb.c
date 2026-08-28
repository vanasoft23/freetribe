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
 * @file per_usb.c
 *
 * @brief AM1802 USB0 interrupt bridge.
 */

/*----- Includes -----------------------------------------------------*/

#include "per_usb.h"

#include "csl_interrupt.h"
#include "hw_types.h"
#include "hw_usbOtg_AM1808.h"
#include "soc_AM1808.h"
#include "tusb.h"

/*----- Macros -------------------------------------------------------*/

#define USB_INSTANCE 0

/*----- Static function prototypes -----------------------------------*/

static void _usb0_isr(void);
static void _usb0_bridge_clear(void);

/*----- Extern function implementations ------------------------------*/

bool per_usb_init_device(u8 int_channel) {

	IntSystemDisable(SYS_INT_USB0);
	_usb0_bridge_clear();

	IntChannelSet(SYS_INT_USB0, int_channel);
	IntRegister(SYS_INT_USB0, _usb0_isr);
	IntSystemStatusClear(SYS_INT_USB0);

	return true;
}

void per_usb_terminate(void) {

	IntSystemDisable(SYS_INT_USB0);
	IntSystemStatusClear(SYS_INT_USB0);
	_usb0_bridge_clear();
}

/*----- Static function implementations ------------------------------*/

static void _usb0_isr(void) { tusb_int_handler(USB_INSTANCE, true); }

static void _usb0_bridge_clear(void) {

	HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_MASK_CLEAR) = 0xffffffffu;
	HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_CLEAR) = 0xffffffffu;
	HWREG(SOC_USB_0_OTG_BASE + USB_0_END_OF_INTR) = 0u;
}

/*----- End of file --------------------------------------------------*/
