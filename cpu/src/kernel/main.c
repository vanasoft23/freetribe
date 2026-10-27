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

#include "ft.h"

#include <FreeRTOS.h>
#include <task.h>

#include "dev_board.h"
#include "dev_lcd.h"
#include "dev_dsp_ipc.h"

#include "svc_system.h"

#include "system/fatal_error.h"

#include "usr_main.h"

/*----- Macros -------------------------------------------------------*/



/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _startup_task(void *params);
static void _create_startup_task(void);
static void _create_user_task(void);

/*----- Extern function implementations ------------------------------*/


/**
* @brief  Run kernel and app.
*
*/
int main(void)
{
	dev_board_init();
	dev_lcd_set_backlight(true, true, true);
	dev_dsp_ipc_init();
	
	_create_startup_task();
	
	DLOG("starting scheduler");
	vTaskStartScheduler();
	
	return 0;
}

/*----- Static function implementations ------------------------------*/

static void _startup_task(void *params)
{
	DLOG(__FUNCTION__);
	
	// @TODO: wait for MCU to be ready
	
	// @TODO: start DSP task
	// @TODO: start USB task
	// @TODO: start MIDI task
	
	// svc_dsp_wait_ready(); ///////// IMPORTANT: user task must be sealed from kernel init!
	
	_create_user_task();
	
	// svc_system_print("Starting user task...");
	
	// svc_system_print("Freetribe initialised!");
	
	// taskYIELD();
}


static void _create_startup_task(void)
{
	#define STARTUP_TASK_PRIORITY       9
	#define STARTUP_TASK_STACK_WORDS 1024
	
	BaseType_t result = xTaskCreate(
		_startup_task, "startup",
		STARTUP_TASK_STACK_WORDS, NULL,
		STARTUP_TASK_PRIORITY, NULL
	);
	configASSERT(result == pdPASS);
}

static void _create_user_task(void)
{
	#define USER_TASK_STACK_WORDS    1024
	#define USER_TASK_PRIORITY          2
	
	BaseType_t result = xTaskCreate(
		usr_main_task, "user",
		USER_TASK_STACK_WORDS, NULL,
		USER_TASK_PRIORITY, NULL
	);
	configASSERT(result == pdPASS);
}

/*----- End of file --------------------------------------------------*/
