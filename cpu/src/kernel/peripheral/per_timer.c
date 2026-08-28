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
 * @file    per_timer.c
 *
 * @brief   Driver for timer peripheral.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <csl_interrupt.h>
#include <csl_timer.h>
#include <soc_AM1808.h>

#include "per_timer.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static u32 _timer_system_interrupt(u32 base_addr, u32 int_flags);

/*----- Extern function implementations ------------------------------*/

/// TODO: This only supports interrupts for Timer12,
///       which is fine for our use case.
//
/// TODO: ISR should be part of this peripheral driver,
///         Register callback from device driver above.
//
void timer_init(t_timer_config config) {

	// Set emulation mode FREE.
	TimerEmulationModeSet(config.base_addr, TMR_EMUMGT_FREE);

	// Set timer mode.
	TimerConfigure(config.base_addr, config.mode);

	// Set period.
	TimerPeriodSet(config.base_addr, TMR_TIMER12, config.period);

	// Enable continuous timer.
	TimerEnable(config.base_addr, TMR_TIMER12, TMR_ENABLE_CONT);

	// Configure interrupt in AINTC.
	if (config.p_isr) {
		u32 system_int =
			_timer_system_interrupt(config.base_addr, config.int_flags);

		// Register interrupt service routine.
		IntRegister(system_int, config.p_isr);

		// Set interrupt channel
		IntChannelSet(system_int, config.int_chan);

		// Enable system interrupts for timer.
		IntSystemEnable(system_int);
	}

	// Enable specified interrupts.
	TimerIntEnable(config.base_addr, config.int_flags);

	// Clear all interrupts.
	TimerIntStatusClear(config.base_addr, TMR_INTSTAT12_TIMER_NON_CAPT |
										  TMR_INTSTAT12_TIMER_CAPT |
										  TMR_INTSTAT34_TIMER_NON_CAPT |
										  TMR_INTSTAT34_TIMER_CAPT);
}

// void timer_unregister_interrupt(u32 base_addr) {

//     IntSystemDisable(SYS_INT_TINT12_0);
//     IntUnRegister(SYS_INT_TINT12_0);
	
//     // note: interrupt channel should now be 8 like factory SBL

//     TimerIntDisable(base_addr, TMR_INTSTAT12_TIMER_NON_CAPT |
//                                TMR_INTSTAT12_TIMER_CAPT |
//                                TMR_INTSTAT34_TIMER_NON_CAPT |
//                                TMR_INTSTAT34_TIMER_CAPT);

//     TimerIntStatusClear(base_addr, TMR_INTSTAT12_TIMER_NON_CAPT |
//                                    TMR_INTSTAT12_TIMER_CAPT |
//                                    TMR_INTSTAT34_TIMER_NON_CAPT |
//                                    TMR_INTSTAT34_TIMER_CAPT);
	
// }

u32 timer_count_get(u32 base_addr) {

	return TimerCounterGet(base_addr, TMR_TIMER12);
}

/*----- Static function implementations ------------------------------*/

static u32 _timer_system_interrupt(u32 base_addr, u32 int_flags) {
	if (base_addr == SOC_TMR_1_REGS) {
		if (int_flags & TMR_INT_TMR34_NON_CAPT_MODE)
			return SYS_INT_TINT34_1;

		return SYS_INT_TINT12_1;
	}

	if (int_flags & TMR_INT_TMR34_NON_CAPT_MODE)
		return SYS_INT_TINT34_0;

	return SYS_INT_TINT12_0;
}

/*----- End of file --------------------------------------------------*/
