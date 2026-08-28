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
 * @file    arm_frame.h
 * 
 * @brief   Register layout of ARM frame.
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
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef ARM_FRAME_H
#define ARM_FRAME_H

#define ARM_FRAME_R4_OFFSET      0
#define ARM_FRAME_R5_OFFSET      4
#define ARM_FRAME_R6_OFFSET      8
#define ARM_FRAME_R7_OFFSET      12
#define ARM_FRAME_R8_OFFSET      16
#define ARM_FRAME_R9_OFFSET      20
#define ARM_FRAME_R10_OFFSET     24
#define ARM_FRAME_R11_OFFSET     28
#define ARM_FRAME_SP_OFFSET      32
#define ARM_FRAME_LR_OFFSET      36
#define ARM_FRAME_CPSR_OFFSET    40
#define ARM_FRAME_PADDING_OFFSET 44
#define ARM_FRAME_R0_OFFSET      48
#define ARM_FRAME_R1_OFFSET      52
#define ARM_FRAME_R2_OFFSET      56
#define ARM_FRAME_R3_OFFSET      60
#define ARM_FRAME_R12_OFFSET     64
#define ARM_FRAME_PC_OFFSET      68
#define ARM_FRAME_SIZE           72

#ifndef __ASSEMBLER__

#include "ft.h"
#include <stddef.h>

typedef struct {
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 r11;
	u32 sp;
	u32 lr;
	u32 cpsr;
	u32 _padding; // stack must remaing aligned to 8 bytes in IRQHandler
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 pc;
} arm_frame_t;

_Static_assert(offsetof(arm_frame_t, r4) == ARM_FRAME_R4_OFFSET, "arm_frame_t r4 offset");
_Static_assert(offsetof(arm_frame_t, r5) == ARM_FRAME_R5_OFFSET, "arm_frame_t r5 offset");
_Static_assert(offsetof(arm_frame_t, r6) == ARM_FRAME_R6_OFFSET, "arm_frame_t r6 offset");
_Static_assert(offsetof(arm_frame_t, r7) == ARM_FRAME_R7_OFFSET, "arm_frame_t r7 offset");
_Static_assert(offsetof(arm_frame_t, r8) == ARM_FRAME_R8_OFFSET, "arm_frame_t r8 offset");
_Static_assert(offsetof(arm_frame_t, r9) == ARM_FRAME_R9_OFFSET, "arm_frame_t r9 offset");
_Static_assert(offsetof(arm_frame_t, r10) == ARM_FRAME_R10_OFFSET, "arm_frame_t r10 offset");
_Static_assert(offsetof(arm_frame_t, r11) == ARM_FRAME_R11_OFFSET, "arm_frame_t r11 offset");
_Static_assert(offsetof(arm_frame_t, sp) == ARM_FRAME_SP_OFFSET, "arm_frame_t sp offset");
_Static_assert(offsetof(arm_frame_t, lr) == ARM_FRAME_LR_OFFSET, "arm_frame_t lr offset");
_Static_assert(offsetof(arm_frame_t, cpsr) == ARM_FRAME_CPSR_OFFSET, "arm_frame_t cpsr offset");
_Static_assert(offsetof(arm_frame_t, r0) == ARM_FRAME_R0_OFFSET, "arm_frame_t r0 offset");
_Static_assert(offsetof(arm_frame_t, r1) == ARM_FRAME_R1_OFFSET, "arm_frame_t r1 offset");
_Static_assert(offsetof(arm_frame_t, r2) == ARM_FRAME_R2_OFFSET, "arm_frame_t r2 offset");
_Static_assert(offsetof(arm_frame_t, r3) == ARM_FRAME_R3_OFFSET, "arm_frame_t r3 offset");
_Static_assert(offsetof(arm_frame_t, r12) == ARM_FRAME_R12_OFFSET, "arm_frame_t r12 offset");
_Static_assert(offsetof(arm_frame_t, pc) == ARM_FRAME_PC_OFFSET, "arm_frame_t pc offset");
_Static_assert(sizeof(arm_frame_t) == ARM_FRAME_SIZE, "arm_frame_t size");

#endif /* !__ASSEMBLER__ */
#endif /* ARM_FRAME_H */
/*----- End of file --------------------------------------------------*/