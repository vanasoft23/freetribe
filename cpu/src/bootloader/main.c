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
 * @file    main.c
 *
 * @brief   Main function for Freetribe CPU firmware.
 */

/*----- Includes -----------------------------------------------------*/

#include <hw_types.h>
#include <hw_syscfg0_AM1808.h>
#include <soc_AM1808.h>

#include "macros.h"

#include "per_pinmux.h"
#include "per_aintc.h"
#include "per_gpio.h"
#include "dev_lcd.h"
#include "dev_flash.h"
#include "svc_delay.h"

#include "gui.h"
#include "file_browser.h"
#include "usb/boot_usb.h"
#include "flash_bootloader.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _init_hardware();
// static void _setup_pinmux();
static void _start_lcd();

static void _test_flash() {

    // const uint32_t src_addr = 0x00F00000;
    // uint8_t a[4], b[4];

    // dev_flash_read(src_addr, a, 4);

    // uint8_t buf[] = { 0xAC, 0x1D, 0xBA, 0xBE };
    // dev_flash_write(src_addr, buf, sizeof(buf));

    // dev_flash_read(src_addr, b, 4);
    
    // dev_flash_write(src_addr, (uint8_t*)"TIPA", 4);

    // DEBUG_LOG("before: %02x %02x %02x %02x", (unsigned int)a[0], (unsigned int)a[1], (unsigned int)a[2], (unsigned int)a[3]);
    // DEBUG_LOG(" after: %02x %02x %02x %02x", (unsigned int)b[0], (unsigned int)b[1], (unsigned int)b[2], (unsigned int)b[3]);

}

/*----- Extern function implementations ------------------------------*/

/**
 * @brief  Run kernel and app.
 *
 */
int main(void) {

    create_bootloader_ddr_mirror(); // if we want to install to flash we need a clean copy

    _init_hardware();
    file_browser_init();
    gui_init();
    boot_usb_init();
    _test_flash();
    
    do {
        boot_usb_task();
        file_browser_tick();
        gui_tick();
    } while(true);

    return 0;
}

/*----- Static function implementations ------------------------------*/

static void _init_hardware() {

    delay_init();
    per_gpio_init();
    per_pinmux_init(); // _setup_pinmux();
    per_aintc_init();

    per_gpio_set(6, 8, true);
    per_gpio_set(6, 6, true);
    per_gpio_set(7, 13, true);

    delay_block_us(60);

    per_gpio_set(6, 2, false);
    per_gpio_set(7, 10, true);

    _start_lcd();

    while (!per_gpio_get(8, 15)) // what is this for?
        ;

    // // @TODO: disable during handoff
    // delay_block_us(10);
    // per_gpio_set_indexed(124, 1); // Set GP7P11
    // per_gpio_set(6, 11, 1); // Set GP6P11
    // ///////////////
    
    delay_block_us(50000); // TODO: how long is enough for MCU and panel UART to be ready?

    dev_flash_init();

}

// static void _setup_pinmux() {
//     // McASP0 Clock, GPIO0 8-9.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(0)) = 0x88111111;

//     // GPIO0 0-7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(1)) = 0x88888888;

//     // McASP0 AXR0, AXR1, GPIO1 10-15.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(2)) = 0x11444448;

//     // UART0 RX, TX, SPI0 SIMO, ENA, CLK, GPIO8 1-2, 6.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(3)) = 0x44221411;

//     // UART1 RX, TX, SPI0 SCS[0], GPIO1 2-5, 7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(4)) = 0x22888814;

//     // SPI1 SIMO, SOMI, CLK, GPIO2 12, 14, 15, EMIFA, GPIO2 8.
//     // HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(5)) = 0x81118188; // comment out for bitbanged SPI
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(5)) = 0x81111111; // non-bitbanged SPI

//     // GPIO2 0-7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(6)) = 0x88888888;

//     // EMIFA Control, GPIO3 8-9, 12-14.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(7)) = 0x88118881;

//     // EMIFA Data 8-15.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(8)) = 0x11111111;

//     // EMIFA Data 0-7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(9)) = 0x11111111;

//     // SD, GPIO4 0-1.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(10)) = 0x88222222;

//     // EMIFA Address 8-15.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(11)) = 0x11111111;

//     // EMFIA Address 0-7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(12)) = 0x11111111;

//     // Reset Out / GPIO6 8-14.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(13)) = 0x88888881;

//     /// TODO: Are the RMII pins exposed?
//     //
//     // RMII, GPIO6 6-7.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(14)) = 0x88888888;

//     // RMII.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(15)) = 0x00000088;

//     // GPIO6 5, GPIO7 10-15.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(16)) = 0x88888880;

//     // GPIO7 8-9.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(17)) = 0x00000088;

//     // GPIO8 10-15.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(18)) = 0x88888800;

//     // GPIO8 0-4, 8-9, RTCK.
//     HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_PINMUX(19)) = 0x18888888;
// }

static void _start_lcd() {

    dev_lcd_set_backlight(true, true, true);

    delay_block_us(5);
    dev_lcd_reset(true);
    delay_block_us(5);
    dev_lcd_reset(false);
    delay_block_us(5);
    dev_lcd_init();
    delay_block_us(5);

}

/*----- End of file --------------------------------------------------*/
