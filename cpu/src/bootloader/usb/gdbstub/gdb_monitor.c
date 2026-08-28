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
 * @file    gdb_monitor.c
 *
 * @brief   GDB stub trap loop
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "arm_frame.h"
#include "arm_instruction.h"
#include "gdb_monitor_layout.h"
#include "gdb_monitor.h"

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

extern const unsigned char __exception_handlers_start[];
extern const unsigned char __exception_handlers_end[];

// owned by IRQHandler to save a few instructions in non-GDB path
extern volatile u32 gdb_should_stop;

volatile gdb_monitor_t g_monitor;

/*----- Static variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _sync_instruction_word(u32 address);
static bool _breakpt_addr_protected(u32 address);

static bool s_monitor_initialized;

/*----- API basics ---------------------------------------------------*/

void gdb_monitor_reset(void)
{
	if (s_monitor_initialized) {
		for (int i = 0; i < NUM_SW_BREAKPOINTS; i++) {
			volatile gdb_breakpoint_t *bp = &g_monitor.breakpts[i];

			if (bp->address != INACTIVE_BREAKPT_ADDR) {
				*(volatile u32 *)(uintptr_t)bp->address = bp->original_instruction;
				_sync_instruction_word(bp->address);
			}
		}
	} else {
		s_monitor_initialized = true;
	}

	for (int i = 0; i < NUM_SW_BREAKPOINTS; i++) {
		g_monitor.breakpts[i].address = INACTIVE_BREAKPT_ADDR;
	}
}

/*----- Start/stop state API -----------------------------------------*/

/**
 * @brief   Invoked from an ISR to tell GDB stub to halt execution.
 * 
 * @details
 * When GDB stub instructs to halt the CPU, this is what really happens:
 * 1. Ctrl+C character is detected in CDC USB ISR handler.
 * 2. Still in the ISR handler, @ref gdb_should_stop flag is raised.
 * 3. As ISR handler returns, we eventually land back into @ref IRQHandler.
 * 4. The next instructions immediately check for @ref gdb_should_stop.
 * 5. If set, we enter a trap loop in which only USB CDC task runs to
 *    parse incoming GDB RSP packets.
 * 
 * @param[in]   sigval    Reason to halt, expressed as Unix signal num.
 */
void gdb_monitor_request_stop(int sigval) {
	g_monitor.sigval = sigval;
	gdb_should_stop  = true; // stop in IRQHandler
}

/**
 * @brief   Exit out of the IRQ trap, continuing execution.
 */
void gdb_monitor_continue(void) {
	g_monitor.frame_ready = false;
	g_monitor.frame_ptr   = NULL;
	gdb_should_stop       = false; // wake up IRQHandler
}

/**
 * @returns Whether the GDB target is currently running.
 */
bool gdb_monitor_target_running(void) {
	return !gdb_should_stop;
}

/**
 * @returns Unix signal, or 0 if target is running.
 */
int gdb_monitor_get_stop_reason(void) {
	return g_monitor.sigval;
}

/**
 * @brief   Control adherence to stop requests.
 */
void gdb_monitor_set_ignore_stop(bool ignore) {
	g_monitor.ignore_stop = ignore;
}

/**
 * @brief
 */
bool gdb_monitor_prepare_step(void) {
	
	if (!g_monitor.frame_ready ||
	     g_monitor.frame_ptr == NULL ||
	     g_monitor.breakpts[0].address != INACTIVE_BREAKPT_ADDR)
	{
		return false;
	}

	arm_frame_t *frame = g_monitor.frame_ptr;

	arm_instruction_step_target_t target;
	arm_instruction_status_t status = arm_instruction_get_step_target(frame, &target);

	if (status != ARM_INSTRUCTION_STATUS_OK) {
		return false;
	}

	// Thumb stepping needs a separate 16-bit breakpoint implementation.
	if (target.instruction_set != ARM_INSTRUCTION_SET_ARM) {
		return false;
	}

	// The decoder should guarantee this, but keep the memory access safe.
	if ((target.address & 3u) != 0u) {
		return false;
	}

	// Patching the current instruction would stop without executing it.
	if (target.address == frame->pc) {
		return false;
	}

	volatile u32 *instruction =
		(volatile u32 *)(uintptr_t)target.address;

	const u32 original = *instruction;

	// Avoid restoring our UDF over another UDF indefinitely.
	if (original == ARM_BREAKPOINT_OP) {
		return false;
	}

	*instruction = ARM_BREAKPOINT_OP;
	_sync_instruction_word(target.address);

	if (*instruction != ARM_BREAKPOINT_OP) {
		*instruction = original;
		_sync_instruction_word(target.address);
		return false;
	}

	g_monitor.breakpts[0].address              = target.address;
	g_monitor.breakpts[0].original_instruction = original;

	return true;
}

/**
 * @brief   Set a software breakpoint at the desired address.
 * 
 * @param[in] target_addr
 * 
 * @returns true on success, false on failure
 */
bool gdb_monitor_insert_breakpoint(u32 target_addr) {
	
	if ((target_addr & 3u) != 0u || _breakpt_addr_protected(target_addr)) {
		return false;
	}

	for (int i = 1; i < NUM_SW_BREAKPOINTS; i++) {
		volatile gdb_breakpoint_t *bp = &g_monitor.breakpts[i];
		if (bp->address == INACTIVE_BREAKPT_ADDR) {

			bp->original_instruction = *(volatile u32*)target_addr;
			*(volatile u32*)target_addr = ARM_BREAKPOINT_OP;
			_sync_instruction_word(target_addr);
			bp->address = target_addr;

			return true;
		}
	}

	return false;
}

/**
 * @brief   Remove the software breakpoint at said address.
 * 
 * @param[in] target_addr
 * 
 * @returns true on success, false on failure
 */
bool gdb_monitor_remove_breakpoint(u32 target_addr) {

	for (int i = 1; i < NUM_SW_BREAKPOINTS; i++) {
		volatile gdb_breakpoint_t *bp = &g_monitor.breakpts[i];
		if (bp->address != INACTIVE_BREAKPT_ADDR && bp->address == target_addr) {

			*(volatile u32*)target_addr = bp->original_instruction;
			_sync_instruction_word(target_addr);
			bp->address = INACTIVE_BREAKPT_ADDR;

			return true;
		}
	}

	return false;
}

/*----- Register API -------------------------------------------------*/

arm_frame_t *gdb_monitor_read_registers(void) {
	ASSERT(g_monitor.frame_ready);
	return g_monitor.frame_ptr;
}

bool gdb_monitor_frame_ready(void) {
	return g_monitor.frame_ready;
}



/*----- Memory API ---------------------------------------------------*/

bool gdb_monitor_read_memory(
	void *context,
	u32   address,
	u8   *destination,
	u32   length
) {
	(void)context;

	const volatile u8 *source = (const volatile u8 *)(uintptr_t)address;
	for (u32 i = 0u; i < length; ++i) {
		destination[i] = source[i];
	}
	return true;
}

bool gdb_monitor_write_memory(
	void     *context,
	u32       address,
	const u8 *source,
	u32       length
) {
	(void)context;

	volatile u8 *destination = (volatile u8 *)(uintptr_t)address;
	for (u32 i = 0u; i < length; ++i) {
		destination[i] = source[i];
	}
	return true;
}


/*----- Static function implementations ------------------------------*/

static void _sync_instruction_word(u32 address) {
	asm volatile(
		"mcr p15, 0, %0, c7, c10, 1\n\t" // Clean D-cache line to memory
		"mcr p15, 0, %0, c7, c10, 4\n\t" // Wait for the write to complete
		"mcr p15, 0, %0, c7, c5, 1"      // Discard stale I-cache line
		:
		: "r"(address)
		: "memory"
	);
}

/**
 * @brief   Exception handlers are protected from having breakpoints set
 *          inside of them.
 */
static bool _breakpt_addr_protected(u32 address) {

	const uintptr_t start =
		(uintptr_t)__exception_handlers_start;
	const uintptr_t end =
		(uintptr_t)__exception_handlers_end;
	const uintptr_t target = (uintptr_t)address;

	return (target >= start) && (target < end);
}

/*----- End of file --------------------------------------------------*/
