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
 * @file    per_timer.h
 *
 * @brief   Header file for per_tiomer.c.
 */

#ifndef PER_TIMER_H
#define PER_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "csl_timer.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    u32 base_addr;
    u32 mode;
    u32 period;
    u32 int_flags; // See hal_timer.h.
    u32 int_chan;
    void (*p_isr)(void);
} t_timer_config;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void timer_init(t_timer_config config);
void timer_unregister_interrupt(u32 base_addr);

u32 timer_count_get(u32 base_addr);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
