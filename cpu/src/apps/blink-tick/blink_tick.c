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

					   Copyright bangcorrupt 2024

----------------------------------------------------------------------*/

/**
 * @file    blink-tick.c
 *
 * @brief   Example application for FreeRTOS elapsed-time checks.
 *
 * This example uses the FreeRTOS tick count to toggle an LED periodically.
 */

/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "FreeRTOS.h"
#include "task.h"

/*----- Macros -------------------------------------------------------*/

#define LED_PERIOD_MS 500u

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static TickType_t g_last_led_tick;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/


/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise application.
 *
 * Record the initial LED timestamp.
 * Status is assumed successful.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void)
{

	t_status status = ERROR;

	g_last_led_tick = xTaskGetTickCount();

	status = SUCCESS;

	return status;
}

/**
 * @brief   Run application.
 *
 * Toggle an LED when its interval has elapsed.
 */
void app_run(void)
{

	TickType_t now = xTaskGetTickCount();
	if ((TickType_t)(now - g_last_led_tick) >= pdMS_TO_TICKS(LED_PERIOD_MS)) {
		ft_toggle_led(LED_TAP);
		g_last_led_tick = now;
	}
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
