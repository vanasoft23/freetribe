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
 * @file    svc_system.c
 *
 * @brief   System task.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "ft_error.h"

#include "dev_board.h"
#include "dev_flash.h"
#include "dev_dsp_ipc.h"

#include "svc_delay.h"
#include "svc_system.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static void (*p_print_callback)(char *text) = NULL;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void svc_system_init(void) {

    // Initialise hardware.
    dev_board_init();

    dev_dsp_ipc_init();

    /// TODO: How long is necessary?
    ///          Are there signals we can test?
    //
    // Give the hardware time to wake up.
    delay_block_us(500000); // 2000000

    /// TODO: Only necessary if flash is locked.
    //
    // Workaround flash write protect.
    // dev_flash_unlock();
    
}

/// TODO: Implement standard library IO via SysEx.
//
void svc_system_print(char *text) {

    if (p_print_callback != NULL) {
        (p_print_callback)(text);
    }
}

void svc_system_register_print_callback(void (*callback)(char *)) {

    if (callback != NULL) {
        p_print_callback = callback;
    }
}

void svc_system_shutdown(void) {

    dev_board_terminate();
    dev_board_power_off();
}

/*----- Static function implementations ------------------------------*/


/*----- End of file --------------------------------------------------*/
