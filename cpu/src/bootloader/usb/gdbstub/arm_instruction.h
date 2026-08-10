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
 * @file    arm_instruction.h
 *
 * @brief   ARM926EJ-S instruction control-flow evaluation
 * 
 * @details I vibecoded this entirely, sorry!
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef ARM_INSTRUCTION_H
#define ARM_INSTRUCTION_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "arm_frame.h"

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    ARM_INSTRUCTION_STATUS_OK = 0,
    ARM_INSTRUCTION_STATUS_INVALID_ARGUMENT,
    ARM_INSTRUCTION_STATUS_INVALID_STATE,
    ARM_INSTRUCTION_STATUS_UNALIGNED,
    ARM_INSTRUCTION_STATUS_UNSUPPORTED,
    ARM_INSTRUCTION_STATUS_UNPREDICTABLE,
} arm_instruction_status_t;

typedef enum {
    ARM_INSTRUCTION_SET_ARM = 0,
    ARM_INSTRUCTION_SET_THUMB,
} arm_instruction_set_t;

typedef struct {
    u32                   address;
    arm_instruction_set_t instruction_set;
} arm_instruction_step_target_t;

/*----- Public API ---------------------------------------------------*/

/**
 * @brief   Calculate the instruction executed after the framed one.
 *
 * @details Evaluates ARMv5TEJ control flow from ARM state. Instruction
 *          words and indirect branch targets are read directly from
 *          target memory. The output is unchanged when an error is
 *          returned.
 *
 * @param[in]  frame     Register state before the current instruction.
 * @param[out] target    Normalized address and instruction set of the
 *                       successor.
 *
 * @returns Status describing whether the successor was calculated.
 */
arm_instruction_status_t arm_instruction_get_step_target(
    const arm_frame_t             *frame,
    arm_instruction_step_target_t *target
);

/*--------------------------------------------------------------------*/

#endif /* ARM_INSTRUCTION_H */

/*----- End of file --------------------------------------------------*/
