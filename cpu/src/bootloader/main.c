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
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <hw_types.h>
#include <hw_syscfg0_AM1808.h>
#include <soc_AM1808.h>

#include "per_pinmux.h"
#include "per_aintc.h"
#include "per_gpio.h"
#include "dev_lcd.h"
#include "dev_flash.h"
#include "svc_delay.h"

#include "ui/ui_controller.h"
#include "usb/boot_usb.h"
#include "gdbstub/gdb_stub.h"
#include "service/boot_section.h"
#include "service/handoff.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _init_hardware(void);
// static void _setup_pinmux(void);
static void _start_lcd(void);

// #define SCAN_GPIO
#ifdef SCAN_GPIO

#define BAT_PIN PIN_GP7_9


// GPIO scanning for battery power interface
static u8 s_pin_checklist[] = {
    
    // 97,98,99,100,101,102,103,104,105,106,107,108,109,110,

    PIN_GP7_8,
    PIN_GP7_9, //risfal
    PIN_GP7_11,
    PIN_GP7_12,
    PIN_SHUTDOWN,

    // PIN_GP8_8, PIN_GP8_9, PIN_GP8_10, PIN_GP8_11, PIN_GP8_12, PIN_GP8_13, PIN_GP8_14, PIN_GP8_15
};

static volatile u32 s_gpio_irqs = 0;
static bool s_pinstates[256];

static void _scan_gpio(void) {
    for (int i = 0; i < sizeof(s_pin_checklist)/sizeof(u8); i++) {

        u8 pin_index = s_pin_checklist[i];

        bool    new_state = per_gpio_get_indexed(pin_index);
        bool *p_old_state = &s_pinstates[pin_index];

        if (new_state != *p_old_state) {
            DEBUG_LOG("Pin %u became %u", (unsigned int)pin_index, (unsigned int)new_state);
        }

        *p_old_state = new_state;

    }

    DEBUG_LOG("GPIO ISR %i", s_gpio_irqs);
}


static void _gpio_isr(void) {
    per_aintc_clear_status_gpio(BAT_PIN);
    s_gpio_irqs++;
}
#endif

/*----- Extern function implementations ------------------------------*/

/**
 * @brief  Run kernel and app.
 *
 */
int main(void) {

#ifdef SCAN_GPIO
    per_aintc_register_gpio_interrupt(9, BAT_PIN, 3, _gpio_isr);
#endif

    create_sbl_ddr_snapshot();

    _init_hardware();
    ui_controller_init();
    boot_usb_init();
    
    do {
        boot_usb_task();

        if (gdb_stub_take_handoff_request()) {
            handoff_factory_firmware();
        }

        ui_controller_tick();

#ifdef SCAN_GPIO
        _scan_gpio();
#endif

    } while(true);

    return 0;
}

/*----- Static function implementations ------------------------------*/

static void _init_hardware(void) {

    delay_init();
    per_gpio_init();
    per_pinmux_init(); // _setup_pinmux();
    per_aintc_init();

    per_gpio_set_indexed(PIN_BOARD_MCU_RESET, true);
    per_gpio_set_indexed(PIN_GP6_6, true);
    per_gpio_set_indexed(PIN_POWER_CTL, true); // power control pin; only red lights if not set

    delay_block_us(60);

    per_gpio_set_indexed(PIN_BOARD_ADC_RESET, false);
    per_gpio_set_indexed(PIN_BOARD_ADC_MCLK, true);

    _start_lcd();

    while (!per_gpio_get_indexed(PIN_GP8_15)) // what is this for?
        ;

    // /////////////// This was not in factory SBL
    // delay_block_us(10);
    // per_gpio_set_indexed(PIN_GP7_11, 1);
    
    // per_gpio_set_indexed(PIN_GP6_11, 1);
    // per_gpio_set_indexed(PIN_GP6_12, 1);
    // ///////////////
    
    delay_block_us(50000); // TODO: how long is enough for MCU and panel UART to be ready?

    dev_flash_init();

}

// static void _setup_pinmux(void) {
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

static void _start_lcd(void) {

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
