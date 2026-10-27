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

#include "per_pinmux.h"
#include "per_aintc.h"
#include "per_gpio.h"
#include "per_spi2.h"
#include "dev_lcd.h"
#include "dev_flash.h"
#include "dev_delay.h"

#include "ui/ui_controller.h"
#include "usb/boot_usb.h"
#include "gdbstub/gdb_stub.h"
#include "cmd/install.h"
#include "cmd/handoff.h"

#include "fatal_error.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _init_hardware(void);
static void _start_lcd(void);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief  Run kernel and app.
 *
 */
int main(void)
{
// #define CAUSE_EXCEPTION
#ifdef CAUSE_EXCEPTION
	static u32 fatal_timer = 300u;
#endif

	create_sbl_ddr_snapshot();
	_init_hardware();
	ui_controller_init();
	boot_usb_init();
	
	do {
		boot_usb_task();

		u32 entry_addr;
		if (gdb_stub_take_handoff_request(&entry_addr)) {
			handoff_program(entry_addr);
		}

#ifdef CAUSE_EXCEPTION
		fatal_timer--;
		if (fatal_timer == 0)
			illegal_instruction();
#endif

		ui_controller_tick();

	} while(true);

	return 0;
}

/*----- Static function implementations ------------------------------*/

static void _init_hardware(void)
{
	dev_delay_init();
	per_gpio_init();
	per_pinmux_init();
	per_aintc_init();

	spi_config_t config = {
		.instance = 1,
	};
	per_spi2_init(&config);

	per_gpio_set_indexed(PIN_BOARD_MCU_RESET, true);
	per_gpio_set_indexed(PIN_GP6_6, true);
	per_gpio_set_indexed(PIN_POWER_CTL, true); // power control pin; only red lights if not set

	dev_delay_block_us(60);

	per_gpio_set_indexed(PIN_BOARD_ADC_RESET, false);
	per_gpio_set_indexed(PIN_BOARD_ADC_MCLK, true);

	_start_lcd();

	while (!per_gpio_get_indexed(PIN_GP8_15)) // what is this for?
		;

	// /////////////// This was not in factory SBL
	// dev_delay_block_us(10);
	// per_gpio_set_indexed(PIN_GP7_11, 1);
	
	// per_gpio_set_indexed(PIN_GP6_11, 1);
	// per_gpio_set_indexed(PIN_GP6_12, 1);
	// ///////////////
	
	dev_delay_block_us(50000); ///////TODO: how long is enough for MCU and panel UART to be ready?

	dev_flash_init();

}



// static void _setup_pinmux(void)
//{
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

static void _start_lcd(void)
{

	dev_lcd_set_backlight(true, true, true);

	dev_delay_block_us(5);
	dev_lcd_reset(true);
	dev_delay_block_us(5);
	dev_lcd_reset(false);
	dev_delay_block_us(5);
	dev_lcd_init();
	dev_delay_block_us(5);

}

/*----- End of file --------------------------------------------------*/
