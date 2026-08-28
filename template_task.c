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
 * @file    template_task.c
 *
 * @brief   Template for task state machine source files.
 *
 */

/*----- Includes -----------------------------------------------------*/



/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef enum { STATE_INIT, STATE_RUN, STATE_ERROR } e_template_task_state;

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static bool _template_init(void);
static void _template_run(void);

/*----- Extern function implementations ------------------------------*/

void svc_template_task(void) {

    static e_template_task_state state = STATE_INIT;

    switch (state) {

    // Initialise template task.
    case STATE_INIT:
        if (_template_init()) {
            state = STATE_RUN;
        }
        // Remain in INIT state until initialisation successful.
        break;

    case STATE_RUN:
        _template_run();
        break;

    case STATE_ERROR:
    default:
        PANIC(PANIC_UNHANDLED_STATE);
        break;
    }
}

/*----- Static function implementations ------------------------------*/

static bool _template_init(void) {

    bool success = false;

    // Initialise...

    success = true;
    return success;
}

static void _template_run(void) {

    // Run...
}

/*----- End of file --------------------------------------------------*/
