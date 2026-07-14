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

   You should have received a copy of the GNU Affero General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    main.c
 *
 * @brief   Main function for Freetribe CPU firmware.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>

#include "FreeRTOS.h"
#include "task.h"

#include "knl_main.h"
#include "usr_main.h"

/*----- Macros -------------------------------------------------------*/

#define USER_TASK_PRIORITY    2
#define USER_TASK_STACK_WORDS 1024

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/**
 * @brief  Run kernel and app.
 *
 */
int main(void) {
    BaseType_t result;

    knl_init();

    result = xTaskCreate(
        usr_main_task, "user", USER_TASK_STACK_WORDS, NULL,
        USER_TASK_PRIORITY, NULL
    );
    configASSERT(result == pdPASS);

    vTaskStartScheduler();

    configASSERT(false); // we should never get here

    return 0;
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
