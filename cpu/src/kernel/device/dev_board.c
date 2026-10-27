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
 * @file    dev_board.c
 *
 * @brief   Initialisation and termination of main board.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_aintc.h"
#include "per_ddr.h"
#include "per_gpio.h"
#include "per_pinmux.h"
#include "per_uart.h"
#include "per_spi2.h"
#include "per_timer2.h"

#include "dev_board.h"

#include "dev_delay.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _hardware_init(void);

static void _hardware_terminate(void);

/*----- Extern function implementations ------------------------------*/

extern void vPortTickISR( void *context );

/*
 * @brief   Initialise and configure main board.
 *
 * @param   None
 *
 * @return  None
 */
void dev_board_init(void)
{
	per_pinmux_init();

	per_aintc_init();
	
	// FreeRTOS tick timer
	per_timer2_init(TIMER0_BASE, &(timer_config_t){
		.mode      = TIMER_MODE_32_BIT_UNCHAINED,
		.source    = TIMER_SOURCE_INTERNAL,
		.counter12 = {
			.operation    = TIMER_OPER_CONTINUOUS,
			.period       = 0x5DBFu, // 1ms, Assumes 24 MHz input (ASYNC3)
			.on_period    = vPortTickISR
		}
	});
	
	dev_delay_init();

	per_gpio_init();

	_hardware_init();

	spi_config_t config = {
		.instance = 1
	};
	per_spi2_init(&config);


}

/*----- Static function implementations ------------------------------*/

/*
 * @brief   Shut down periperals.
 *
 *          Returns main board to unitialised state,
 *          allowing factory SBL and APP to boot.
 *
 * @param  none
 *
 * @return none
 *
 */
void dev_board_terminate(void)
{
	// Allows handing off to factory bootloader.
	// Factory SBL will hang if DDR already initialised.
	per_ddr_terminate();

	// Flush TRS MIDI before terminating MCU.
	per_uart_terminate(1);
	per_uart_terminate(0);

	_hardware_terminate();
}

void dev_board_power_off(void)
{ per_gpio_set_indexed(PIN_SHUTDOWN, 0); }

/*
 * @brief   Set GPIO to initialise board hardware?
 *
 * @param   none
 *
 * @return  none
 */
static void _hardware_init(void)
{
	/// TODO: What does each pin represent?

	per_gpio_set_indexed(PIN_BOARD_MCU_RESET, 1);

	per_gpio_set_indexed(PIN_GP6_6, 1);

	// Only red lights if not set.
	// Something with power controller,
	per_gpio_set_indexed(PIN_POWER_CTL, 1);

	dev_delay_spin_us(60);

	// ADC Reset.
	per_gpio_set_indexed(PIN_BOARD_ADC_RESET, 0);

	// ADC MCLK on ??
	per_gpio_set_indexed(PIN_BOARD_ADC_MCLK, 1);

	/// TODO: Timeout error.
	//
	// Wait until B8P15 is high.
	while (!per_gpio_get_indexed(PIN_GP8_15))
		;

	dev_delay_spin_us(10);

	per_gpio_set_indexed(PIN_GP7_11, 1);

	/// TODO: What are these for?
	//
	// This is set during boot, then toggles continuously while app running.
	per_gpio_set_indexed(PIN_GP6_11, 1);
	per_gpio_set_indexed(PIN_GP6_12, 1);
}

static void _hardware_terminate(void)
{
	per_gpio_set_indexed(PIN_BOARD_MCU_RESET, 0);
	per_gpio_set_indexed(PIN_GP6_6, 0);
	per_gpio_set_indexed(PIN_POWER_CTL, 0);

	dev_delay_block_us(60);

	per_gpio_set_indexed(PIN_BOARD_ADC_RESET, 0);
	per_gpio_set_indexed(PIN_BOARD_ADC_MCLK, 0);

	dev_delay_block_us(10);
}

/*----- End of file --------------------------------------------------*/
