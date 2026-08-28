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
 * @file    gdb_monitor.h
 *
 * @brief   Coordinates debug side of ARM hardware.
 * 
 * @details Owns:
 *           - running/stopped state
 *           - stop reason
 *           - frame layout
 *           - frame readiness
 *          Doesn't own:
 *           - RSP knowledge
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef GDB_MONITOR_H
#define GDB_MONITOR_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "arm_frame.h"
#include "gdb_monitor_layout.h"

/*----- Typedefs -----------------------------------------------------*/

enum e_unix_signal {
	SIGINT   = 2,  // Ctrl+C
	SIGILL   = 4,  // Undefined instruction
	SIGTRAP  = 5,  // Just connected, Breakpoint, single-step
	SIGSEGV  = 11, // Data/prefetch abort
};

/*----- Globals ------------------------------------------------------*/

extern volatile gdb_monitor_t g_monitor;

/*----- API basics ---------------------------------------------------*/

void gdb_monitor_reset(void);

/*----- Start/stop state API -----------------------------------------*/

/**
 * @brief   Invoked from an ISR to tell GDB stub to halt execution.
 * 
 * @param[in]   sigval    Reason to halt, expressed as Unix signal num.
 */
void gdb_monitor_request_stop(int sigval);

/**
 * @brief   Exit out of the IRQ trap, continuing execution.
 */
void gdb_monitor_continue(void);

/**
 * @returns Whether the GDB target is currently running.
 */
bool gdb_monitor_target_running(void);

/**
 * @returns Unix signal, or 0 if target is running.
 */
int gdb_monitor_get_stop_reason(void);

/**
 * @brief   Control adherence to stop requests.
 */
void gdb_monitor_set_ignore_stop(bool ignore);

/**
 * @brief
 */
bool gdb_monitor_prepare_step(void);

/**
 * @brief   Set a software breakpoint at the desired address.
 * 
 * @param[in] target_addr
 * 
 * @returns true on success, false on failure
 */
bool gdb_monitor_insert_breakpoint(u32 target_addr);

/**
 * @brief   Remove the software breakpoint at said address.
 * 
 * @param[in] target_addr
 * 
 * @returns true on success, false on failure
 */
bool gdb_monitor_remove_breakpoint(u32 target_addr);

/*----- Register API -------------------------------------------------*/

/**
 * @brief   Returns register layout
 */
arm_frame_t *gdb_monitor_read_registers(void);

/**
 * @brief   Whether IRQHandler is now in trap loop.
 */
bool gdb_monitor_frame_ready(void);


/*----- Memory API ---------------------------------------------------*/

bool gdb_monitor_read_memory(
	void *context,
	u32   address,
	u8   *destination,
	u32   length
);

bool gdb_monitor_write_memory(
	void     *context,
	u32       address,
	const u8 *source,
	u32       length
);

/*--------------------------------------------------------------------*/

#endif /* GDB_MONITOR_H */

/*----- End of file --------------------------------------------------*/
