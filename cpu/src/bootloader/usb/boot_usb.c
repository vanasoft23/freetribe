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
 * @file    boot_usb.c
 *
 * @brief   Wrapper around TinyUSB.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <hw_types.h>
#include <hw_aintc.h>
#include <hw_usbOtg_AM1808.h>
#include <soc_AM1808.h>
#include <csl_interrupt.h>
#include <tusb.h>
#include <musb_am1802.h>

#include "tud_cdc.h"
#include "devices/tud_msc_disk.h"
#include "boot_usb.h"

/*----- Macros -------------------------------------------------------*/

#define BOOT_USB_INT_CHANNEL 2

#define AM1802_USB_RHPORT 0 /* root hub port number */

#define MUSB_AM1802_INTR_MASK 0x01ff1e1fu /* TODO: usb0 bridge clear should */
                                          /* be callable as tusb api        */

/*----- Static function prototypes -----------------------------------*/

static void _register_usb0_intr(void);
static void _usb0_bridge_clear(void);
static void _usb0_isr(void);

/*----- Extern function declarations ---------------------------------*/

extern void dfu_preinit(void);

/*----- Extern function implementations ------------------------------*/

void boot_usb_init(void) {

    dfu_preinit();

    _register_usb0_intr();

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    // AM1802 TinyUSB DCD configures CFGCHIP2 and waits for PHYCLKGD.
    tusb_init(AM1802_USB_RHPORT, &dev_init);

}

void boot_usb_task(void) {

    tud_task();

}

void boot_usb_terminate(void) {

    tusb_deinit(AM1802_USB_RHPORT);
    IntSystemStatusClear(SYS_INT_USB0);
    _usb0_bridge_clear();

}


/**
 * @brief   Invoked after the host configures the TinyUSB device.
 */
void tud_mount_cb(void) {

    boot_msc_mount_cb();

}

/**
 * @brief   Invoked after the active TinyUSB configuration is removed.
 */
void tud_umount_cb(void) {

    cdc_umount_cb();
    boot_msc_umount_cb();

}



/*----- Static function implementations ------------------------------*/

static void _register_usb0_intr() {

    _usb0_bridge_clear();
    IntChannelSet(SYS_INT_USB0, BOOT_USB_INT_CHANNEL);
    IntRegister(SYS_INT_USB0, _usb0_isr);

}

static void _usb0_isr(void) {
    tusb_int_handler(AM1802_USB_RHPORT, true);    
}

static void _usb0_bridge_clear(void) {

    musb_dcd_int_disable(AM1802_USB_RHPORT);
    musb_dcd_int_clear(AM1802_USB_RHPORT);
    // HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_MASK_CLEAR) = MUSB_AM1802_INTR_MASK;
    // HWREG(SOC_USB_0_OTG_BASE + USB_0_INTR_SRC_CLEAR) = MUSB_AM1802_INTR_MASK;
    // HWREG(SOC_USB_0_OTG_BASE + USB_0_END_OF_INTR) = 0u;

}

/*----- End of file --------------------------------------------------*/
