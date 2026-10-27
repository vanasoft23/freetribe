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
 * @file    dev_delay.c
 *
 * @brief   Configuration and handling for delay timer.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_timer2.h"

#include "dev_delay.h"

/*----- Macros -------------------------------------------------------*/

#define DELAY_PERIOD 0x8F0D17Fu // 1s at the assumed 150 MHz TIMER3 input clock.
#define CYCLES_PER_US 150u

// ARM clock is assumed to be 300 MHz; the loop is estimated at 4 cycles.
#define CPU_CYCLES_PER_US 300u
#define DELAY_LOOP_CYCLES 4u

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/


/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/// TODO: Init functions should return status code.
//
void dev_delay_init(void)
{
	timer_config_t delay_cfg = {
		.mode = TIMER_MODE_32_BIT_UNCHAINED,
		.source = TIMER_SOURCE_INTERNAL,
		.counter12 = {
			.operation = TIMER_OPER_CONTINUOUS,
			.period = DELAY_PERIOD,
		},
	};

	ASSERT(per_timer2_init(TIMER3_BASE, &delay_cfg));
	ASSERT(per_timer2_start(TIMER3_BASE, TIMER_HALF_12));
}

void dev_delay_block_us(u32 time)
{
	u32 start_count = 0;
	u32 current_count = 0;
	u32 delta = 0;
	u32 elapsed_cycles = 0;
	u32 elapsed_us = 0;

	start_count = dev_delay_get_current_count();

	while (elapsed_us < time) {

		current_count = dev_delay_get_current_count();

		if (current_count >= start_count) {
			delta = current_count - start_count;

		} else {
			delta = (DELAY_PERIOD - start_count) + current_count + 1;
		}

		elapsed_cycles += delta;

		while (elapsed_cycles >= CYCLES_PER_US) {

			elapsed_us++;
			elapsed_cycles -= CYCLES_PER_US;
		}

		start_count = current_count;
	}
}

/// TODO: I think this only works if called at least once per second.
///       This should be ok for most reasonable use cases.
///
///       Could maybe increase DELAY_PERIOD to allow
///       more time before overflow if needed.
///
///       Check if timer peripheral has overflow flag.
///
///       May cause issues when debugging,
///       check timer emulation mode.
//
bool dev_delay_us(t_delay_state *state)
{
	u32 current_count = 0;
	u32 delta = 0;

	if (!state->expired) {

		if (state->elapsed_us < state->delay_time) {

			current_count = dev_delay_get_current_count();

			if (current_count >= state->start_time) {
				delta = current_count - state->start_time;

			} else {
				delta = (DELAY_PERIOD - state->start_time) + current_count + 1;
			}

			state->elapsed_cycles += delta;

			/// TODO: Optimise.

			while (state->elapsed_cycles >= CYCLES_PER_US) {
				state->elapsed_us++;
				state->elapsed_cycles -= CYCLES_PER_US;
			}

			state->start_time = current_count;

		} else {
			state->elapsed_us = 0;
			state->elapsed_cycles = 0;
			state->expired = true;
		}
	}

	return state->expired;
}

void dev_delay_start(t_delay_state *state, u32 time)
{
	state->start_time = dev_delay_get_current_count();
	state->delay_time = time;
	state->elapsed_cycles = 0;
	state->elapsed_us = 0;
	state->expired = false;
}

void dev_delay_block_ms(u32 time)
{ dev_delay_block_us(time * 1000); }

u32 dev_delay_get_current_count(void)
{ return per_timer2_get_count12(TIMER3_BASE); }

/// TODO: Get elapsed time.


void dev_delay_cycles(u32 count)
{
	u32 loops = count / DELAY_LOOP_CYCLES;
	if (count % DELAY_LOOP_CYCLES)
		loops++;

	if (loops) {
		__asm__ volatile (
			"1: subs %0, %0, #1\n"
			"bne 1b\n"
			: "+r" (loops)
			:
			: "cc", "memory"
		);
	}
}

void dev_delay_spin_us(u32 time)
{
	// Chunk the multiplication so even large delays do not overflow.
	const u32 max_chunk = 0xFFFFFFFFu / CPU_CYCLES_PER_US;
	while (time > max_chunk) {
		dev_delay_cycles(max_chunk * CPU_CYCLES_PER_US);
		time -= max_chunk;
	}
	dev_delay_cycles(time * CPU_CYCLES_PER_US);
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
