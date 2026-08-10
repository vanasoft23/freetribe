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
 * @file    gdb_monitor_layout.h
 *
 * @brief   GDB monitor object struct layout
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef GDB_MONITOR_LAYOUT_H
#define GDB_MONITOR_LAYOUT_H

#define GDB_MONITOR_SIGVAL_OFFSET           0
#define GDB_MONITOR_IGNORE_STOP_OFFSET      4
#define GDB_MONITOR_FRAME_READY_OFFSET      5
#define GDB_MONITOR_FRAME_PTR_OFFSET        8
#define GDB_MONITOR_BREAKPTS_OFFSET         12

#define GDB_BREAKPOINT_ADDRESS_OFFSET       0
#define GDB_BREAKPOINT_ORIGINAL_OFFSET      4
#define GDB_BREAKPOINT_SIZE                 8

#define INACTIVE_BREAKPT_ADDR               0xffffffff /* unreachable even from ARM RAM */
#define ARM_BREAKPOINT_OP                   0xe7f000f0
#define NUM_SW_BREAKPOINTS                  12

#ifndef __ASSEMBLER__

#include "ft.h"
#include "arm_frame.h"

typedef struct {
    u32  address;                    // INACTIVE_BREAKPT_ADDR when inactive
    u32  original_instruction;       // replaced with undefined instruction
} gdb_breakpoint_t;

typedef struct __attribute__((packed, aligned(4))) {
    int               sigval;        // unix sigval; values above 0 indicate target is halted
    bool              ignore_stop;   // disables adherence to stop requests
    bool              frame_ready;   // IRQHandler is now in trap loop
    u8                _reserved0[2];
    arm_frame_t      *frame_ptr;     // ARM register layout to be appointed to stack of IRQHandler
    gdb_breakpoint_t  breakpts[NUM_SW_BREAKPOINTS];
} gdb_monitor_t;

_Static_assert(
    offsetof(gdb_monitor_t, sigval) == GDB_MONITOR_SIGVAL_OFFSET,
    "gdb_monitor_t sigval offset"
);
_Static_assert(
    offsetof(gdb_monitor_t, ignore_stop) == GDB_MONITOR_IGNORE_STOP_OFFSET,
    "gdb_monitor_t ignore_stop offset"
);
_Static_assert(
    offsetof(gdb_monitor_t, frame_ready) == GDB_MONITOR_FRAME_READY_OFFSET,
    "gdb_monitor_t frame_ready offset"
);
_Static_assert(
    offsetof(gdb_monitor_t, frame_ptr) == GDB_MONITOR_FRAME_PTR_OFFSET,
    "gdb_monitor_t frame_ptr offset"
);
_Static_assert(
    GDB_MONITOR_FRAME_PTR_OFFSET % _Alignof(arm_frame_t *) == 0,
    "frame_ptr must be naturally aligned"
);
_Static_assert(
    offsetof(gdb_monitor_t, breakpts) == GDB_MONITOR_BREAKPTS_OFFSET,
    "gdb_monitor_t breakpts offset"
);

_Static_assert(
    offsetof(gdb_breakpoint_t, address) == GDB_BREAKPOINT_ADDRESS_OFFSET,
    "gdb_breakpoint_t address offset"
);
_Static_assert(
    offsetof(gdb_breakpoint_t, original_instruction) == GDB_BREAKPOINT_ORIGINAL_OFFSET,
    "gdb_breakpoint_t original instruction offset"
);
_Static_assert(
    sizeof(gdb_breakpoint_t) == GDB_BREAKPOINT_SIZE,
    "gdb_breakpoint_t size"
);

#endif
#endif
