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

/*----- Includes -----------------------------------------------------*/

#include <hw_types.h>
#include <hw_aintc.h>
#include <hw_usbOtg_AM1808.h>
#include <soc_AM1808.h>
#include <csl_interrupt.h>

#include "macros.h"
#include "tusb.h"
#include "boot_usb.h"

/*----- Macros -------------------------------------------------------*/

#define BOOT_USB_INT_CHANNEL 2

#define AM1802_USB_RHPORT 0 // root hub port number

/*----- Static function prototypes -----------------------------------*/

static void _register_usb0_intr(void);
static void _usb0_bridge_clear(void);
static void _usb0_isr(void);

/*----- Extern function implementations ------------------------------*/

void boot_usb_init(void) {

    _register_usb0_intr();

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    // The AM1802 TinyUSB DCD configures CFGCHIP2 and waits for PHYCLKGD.
    tusb_init(AM1802_USB_RHPORT, &dev_init);

}

void boot_usb_task(void) {

    tud_task();

}

void boot_usb_terminate(void) {

    IntSystemDisable(SYS_INT_USB0);
    IntSystemStatusClear(SYS_INT_USB0);
    _usb0_bridge_clear();

}


/*----- Static function implementations ------------------------------*/

static void _register_usb0_intr() {

    IntSystemDisable(SYS_INT_USB0);
    _usb0_bridge_clear();

    IntChannelSet(SYS_INT_USB0, BOOT_USB_INT_CHANNEL);
    IntRegister(SYS_INT_USB0, _usb0_isr);
    IntSystemStatusClear(SYS_INT_USB0);

}

static void _usb0_isr(void) {
    tusb_int_handler(AM1802_USB_RHPORT, true);    
}

static void _usb0_bridge_clear(void) {

    HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_MASK_CLEAR) = 0xffffffffu;
    HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_CLEAR) = 0xffffffffu;
    HWREG(SOC_USB_0_OTG_BASE + USB_0_END_OF_INTR) = 0u;

}

/*----- End of file --------------------------------------------------*/
