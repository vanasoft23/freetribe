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

#include "per_aintc.h"
#include "per_ddr.h"
#include "per_gpio.h"
#include "per_pinmux.h"
#include "per_uart.h"

#include "dev_board.h"

#include "dev_lcd.h"

/// TODO: Move blocking delay to device layer.
#include "svc_delay.h"

/*----- Macros -------------------------------------------------------*/

#define BOARD_MCU_RESET_PIN       105u // GP6P8
#define BOARD_UNKNOWN_GP6P6_PIN   103u // GP6P6
#define BOARD_POWER_CONTROL_PIN   126u // GP7P13
#define BOARD_ADC_RESET_PIN        99u // GP6P2
#define BOARD_ADC_MCLK_PIN        123u // GP7P10
#define BOARD_READY_PIN           144u // GP8P15
#define BOARD_UNKNOWN_GP7P11_PIN  124u // GP7P11

#define BOARD_ACTIVITY_BANK 6u
#define BOARD_ACTIVITY_PIN  11u

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _hardware_init(void);

static void _hardware_terminate(void);

/*----- Extern function implementations ------------------------------*/

/*
 * @brief   Initialise and configure main board.
 *
 * @param   None
 *
 * @return  None
 */
void dev_board_init(void) {

    per_pinmux_init();

    per_aintc_init();

    /// TODO: Move blocking delay to device layer.
    delay_init();

    per_gpio_init();

    _hardware_init();

    /// TODO: Move up to svc_system.
    dev_lcd_set_backlight(true, true, true);

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
void dev_board_terminate(void) {
    // Allows handing off to factory bootloader.
    // Factory SBL will hang if DDR already initialised.
    per_ddr_terminate();

    // Flush TRS MIDI before terminating MCU.
    per_uart_terminate(1);
    per_uart_terminate(0);

    _hardware_terminate();
}

void dev_board_power_off(void) { per_gpio_set(7, 14, 0); }

/*
 * @brief   Set GPIO to initialise board hardware?
 *
 * @param   none
 *
 * @return  none
 */
static void _hardware_init(void) {

    /// TODO: What does each pin represent?

    // MCU out of reset.
    per_gpio_set_indexed(BOARD_MCU_RESET_PIN, 1);

    per_gpio_set_indexed(BOARD_UNKNOWN_GP6P6_PIN, 1);

    // Only red lights if not set.
    // Something with power controller,
    per_gpio_set_indexed(BOARD_POWER_CONTROL_PIN, 1);

    delay_block_us(60);

    // ADC Reset.
    per_gpio_set_indexed(BOARD_ADC_RESET_PIN, 0);

    // ADC MCLK on ??
    per_gpio_set_indexed(BOARD_ADC_MCLK_PIN, 1);

    /// TODO: Timeout error.
    //
    // Wait until B8P15 is high.
    while (!per_gpio_get_indexed(BOARD_READY_PIN))
        ;

    delay_block_us(10);

    per_gpio_set_indexed(BOARD_UNKNOWN_GP7P11_PIN, 1);

    /// TODO: What are these for?
    //
    // This is set during boot, then toggles continuously while app running.
    per_gpio_set(BOARD_ACTIVITY_BANK, BOARD_ACTIVITY_PIN, 1);
    //
    // per_gpio_set(6, 12, 1); // Set GP6P12
}

static void _hardware_terminate(void) {

    per_gpio_set_indexed(BOARD_MCU_RESET_PIN, 0);
    per_gpio_set_indexed(BOARD_UNKNOWN_GP6P6_PIN, 0);
    per_gpio_set_indexed(BOARD_POWER_CONTROL_PIN, 0);

    delay_block_us(60);

    per_gpio_set_indexed(BOARD_ADC_RESET_PIN, 0);
    per_gpio_set_indexed(BOARD_ADC_MCLK_PIN, 0);

    delay_block_us(10);
}

/*----- End of file --------------------------------------------------*/
