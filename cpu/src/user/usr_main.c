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
 * @file    usr_main.c
 *
 * @brief   Freetribe user task.
 *
 */

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>



#include "FreeRTOS.h"
#include "task.h"

#include "knl_main.h"
#include "usr_main.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef enum { STATE_INIT, STATE_RUN, STATE_ERROR } t_user_task_state;

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static bool _usr_init(void);
static void _usr_run(void);

/*----- Extern function implementations ------------------------------*/

void usr_main_task(void *param)
{
	(void)param;

	(void)_usr_init();
		
	for (;;) {
		_usr_run();
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

__attribute__((weak)) bool app_init(void)
{
	//
	return 0;
}

__attribute__((weak)) void app_run(void)
{
	//
}

/*----- Static function implementations ------------------------------*/

static bool _usr_init(void)
{
	return app_init();
}

static void _usr_run(void)
{ app_run(); }

/*----- End of file --------------------------------------------------*/
