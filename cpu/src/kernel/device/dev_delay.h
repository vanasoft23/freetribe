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
 * @file    dev_delay.h
 *
 * @brief   Public interface for dev_delay.c.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	u32  start_time;
	u32  delay_time;
	u32  elapsed_cycles;
	u32  elapsed_us;
	bool expired;
} t_delay_state;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void dev_delay_init(void);
bool dev_delay_us(t_delay_state *state);
void dev_delay_start(t_delay_state *state, u32 time);
void dev_delay_block_us(u32 time);
void dev_delay_block_ms(u32 time);
u32 dev_delay_get_current_count(void);
u32 dev_delay_get_elapsed_cycles(u32 start_count);
void dev_delay_cycles(u32 count);
// Approximate CPU busy-wait; assumes a 300 MHz ARM clock.
void dev_delay_spin_us(u32 time);
bool dev_delay_ready(void);

#ifdef __cplusplus
}
#endif

/*----- End of file --------------------------------------------------*/
