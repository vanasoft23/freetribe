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
 * @file    arm_instruction.c
 *
 * @brief   ARM926EJ-S instruction control-flow evaluation
 * 
 * @details I vibecoded this entirely, sorry!
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "arm_instruction.h"

/*----- Defines ------------------------------------------------------*/

#define CPSR_T_MASK (1u << 5)
#define CPSR_J_MASK (1u << 24)
#define CPSR_V_MASK (1u << 28)
#define CPSR_C_MASK (1u << 29)
#define CPSR_Z_MASK (1u << 30)
#define CPSR_N_MASK (1u << 31)

#define ARM_CONDITION_UNCONDITIONAL 0x0fu

#define ARM_REGISTER_PC 15u

/*----- Private functions -------------------------------------------*/

static u32 _read_word(u32 address) {
	return *(const volatile u32 *)(uintptr_t)address;
}

static u32 _register_value(
	const arm_frame_t *frame,
	u32                register_index,
	u32                pc_value
) {
	switch (register_index) {
	case 0u:              return frame->r0;
	case 1u:              return frame->r1;
	case 2u:              return frame->r2;
	case 3u:              return frame->r3;
	case 4u:              return frame->r4;
	case 5u:              return frame->r5;
	case 6u:              return frame->r6;
	case 7u:              return frame->r7;
	case 8u:              return frame->r8;
	case 9u:              return frame->r9;
	case 10u:             return frame->r10;
	case 11u:             return frame->r11;
	case 12u:             return frame->r12;
	case 13u:             return frame->sp;
	case 14u:             return frame->lr;
	case ARM_REGISTER_PC: return pc_value;
	default:              return 0u;
	}
}

static bool _condition_passed(u32 condition, u32 cpsr) {
	const bool negative = (cpsr & CPSR_N_MASK) != 0u;
	const bool zero     = (cpsr & CPSR_Z_MASK) != 0u;
	const bool carry    = (cpsr & CPSR_C_MASK) != 0u;
	const bool overflow = (cpsr & CPSR_V_MASK) != 0u;

	switch (condition) {
	case 0x0u: // EQ
		return zero;
	case 0x1u: // NE
		return !zero;
	case 0x2u: // CS/HS
		return carry;
	case 0x3u: // CC/LO
		return !carry;
	case 0x4u: // MI
		return negative;
	case 0x5u: // PL
		return !negative;
	case 0x6u: // VS
		return overflow;
	case 0x7u: // VC
		return !overflow;
	case 0x8u: // HI
		return carry && !zero;
	case 0x9u: // LS
		return !carry || zero;
	case 0xau: // GE
		return negative == overflow;
	case 0xbu: // LT
		return negative != overflow;
	case 0xcu: // GT
		return !zero && (negative == overflow);
	case 0xdu: // LE
		return zero || (negative != overflow);
	case 0xeu: // AL
		return true;
	default:
		return false;
	}
}

static u32 _rotate_right(u32 value, u32 amount) {
	amount &= 31u;
	if (amount == 0u) {
		return value;
	}
	return (value >> amount) | (value << (32u - amount));
}

static u32 _arithmetic_shift_right(u32 value, u32 amount) {
	if (amount == 0u) {
		return value;
	}
	if (amount >= 32u) {
		return (value & 0x80000000u) != 0u ? 0xffffffffu : 0u;
	}

	u32 result = value >> amount;
	if ((value & 0x80000000u) != 0u) {
		result |= 0xffffffffu << (32u - amount);
	}
	return result;
}

static u32 _immediate_shift(
	u32  value,
	u32  shift_type,
	u32  amount,
	bool carry
) {
	switch (shift_type) {
	case 0u: // LSL
		return amount == 0u ? value : value << amount;
	case 1u: // LSR; zero encodes a shift by 32
		return amount == 0u ? 0u : value >> amount;
	case 2u: // ASR; zero encodes a shift by 32
		return amount == 0u
			? ((value & 0x80000000u) != 0u ? 0xffffffffu : 0u)
			: _arithmetic_shift_right(value, amount);
	case 3u: // ROR; zero encodes RRX
		return amount == 0u
			? ((carry ? 1u : 0u) << 31) | (value >> 1)
			: _rotate_right(value, amount);
	default:
		return value;
	}
}

static arm_instruction_status_t _interworking_target(
	u32                            address,
	arm_instruction_step_target_t *target
) {
	if ((address & 1u) != 0u) {
		target->address         = address & ~1u;
		target->instruction_set = ARM_INSTRUCTION_SET_THUMB;
		return ARM_INSTRUCTION_STATUS_OK;
	}
	if ((address & 2u) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}

	target->address         = address;
	target->instruction_set = ARM_INSTRUCTION_SET_ARM;
	return ARM_INSTRUCTION_STATUS_OK;
}

static void _arm_target(u32 address, arm_instruction_step_target_t *target) {
	target->address         = address & ~3u;
	target->instruction_set = ARM_INSTRUCTION_SET_ARM;
}

static void _thumb_target(u32 address, arm_instruction_step_target_t *target) {
	target->address         = address & ~1u;
	target->instruction_set = ARM_INSTRUCTION_SET_THUMB;
}

static u32 _branch_offset(u32 instruction) {
	u32 offset = (instruction & 0x00ffffffu) << 2;
	if ((offset & (1u << 25)) != 0u) {
		offset |= 0xfc000000u;
	}
	return offset;
}

static u32 _register_count(u32 register_list) {
	u32 count = 0u;
	while (register_list != 0u) {
		count += register_list & 1u;
		register_list >>= 1;
	}
	return count;
}

static arm_instruction_status_t _decode_operand2(
	const arm_frame_t *frame,
	u32                instruction,
	u32               *operand
) {
	const bool carry = (frame->cpsr & CPSR_C_MASK) != 0u;

	if ((instruction & (1u << 25)) != 0u) {
		const u32 immediate = instruction & 0xffu;
		const u32 rotation  = ((instruction >> 8) & 0x0fu) * 2u;
		*operand = _rotate_right(immediate, rotation);
		return ARM_INSTRUCTION_STATUS_OK;
	}

	const u32 rm         = instruction & 0x0fu;
	const u32 shift_type = (instruction >> 5) & 0x03u;

	if ((instruction & (1u << 4)) == 0u) {
		const u32 value  = _register_value(frame, rm, frame->pc + 8u);
		const u32 amount = (instruction >> 7) & 0x1fu;
		*operand = _immediate_shift(value, shift_type, amount, carry);
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// This helper is used only when Rd is r15, which makes a
	// register-specified shift architecturally unpredictable on ARMv5.
	return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
}

static bool _is_data_processing(u32 instruction) {
	const bool status_register_write =
		(instruction & 0x0fb0fff0u) == 0x0120f000u ||
		(instruction & 0x0fb0f000u) == 0x0320f000u;
	const bool immediate =
		(instruction & 0x0e000000u) == 0x02000000u;
	const bool immediate_shift =
		(instruction & 0x0e000010u) == 0x00000000u;
	const bool register_shift =
		(instruction & 0x0e000090u) == 0x00000010u;

	return !status_register_write &&
		(immediate || immediate_shift || register_shift);
}

static arm_instruction_status_t _decode_data_processing(
	const arm_frame_t             *frame,
	u32                            instruction,
	arm_instruction_step_target_t *target
) {
	const u32 opcode = (instruction >> 21) & 0x0fu;

	// ARMv5 defines r15 in a register-specified shift as UNPREDICTABLE.
	if ((instruction & (1u << 4)) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}
	if (opcode >= 8u && opcode <= 11u) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}
	if ((instruction & (1u << 20)) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNSUPPORTED;
	}

	u32 operand2;
	arm_instruction_status_t status =
		_decode_operand2(frame, instruction, &operand2);
	if (status != ARM_INSTRUCTION_STATUS_OK) {
		return status;
	}

	const u32 rn = (instruction >> 16) & 0x0fu;
	const u32 operand1 = _register_value(frame, rn, frame->pc + 8u);
	const u32 carry = (frame->cpsr & CPSR_C_MASK) != 0u ? 1u : 0u;
	u32 result;

	switch (opcode) {
	case 0x0u: // AND
		result = operand1 & operand2;
		break;
	case 0x1u: // EOR
		result = operand1 ^ operand2;
		break;
	case 0x2u: // SUB
		result = operand1 - operand2;
		break;
	case 0x3u: // RSB
		result = operand2 - operand1;
		break;
	case 0x4u: // ADD
		result = operand1 + operand2;
		break;
	case 0x5u: // ADC
		result = operand1 + operand2 + carry;
		break;
	case 0x6u: // SBC
		result = operand1 - operand2 - (1u - carry);
		break;
	case 0x7u: // RSC
		result = operand2 - operand1 - (1u - carry);
		break;
	case 0xcu: // ORR
		result = operand1 | operand2;
		break;
	case 0xdu: // MOV
		result = operand2;
		break;
	case 0xeu: // BIC
		result = operand1 & ~operand2;
		break;
	case 0xfu: // MVN
		result = ~operand2;
		break;
	default:
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}

	_arm_target(result, target);
	return ARM_INSTRUCTION_STATUS_OK;
}

static arm_instruction_status_t _decode_load_pc(
	const arm_frame_t             *frame,
	u32                            instruction,
	arm_instruction_step_target_t *target
) {
	const bool pre_index  = (instruction & (1u << 24)) != 0u;
	const bool add        = (instruction & (1u << 23)) != 0u;
	const bool byte       = (instruction & (1u << 22)) != 0u;
	const bool writeback  = (instruction & (1u << 21)) != 0u;
	const bool reg_offset = (instruction & (1u << 25)) != 0u;
	const u32  rn         = (instruction >> 16) & 0x0fu;

	if (byte) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}
	if ((!pre_index && writeback) ||
		(rn == ARM_REGISTER_PC && (!pre_index || writeback))) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}

	u32 offset;
	if (!reg_offset) {
		offset = instruction & 0x0fffu;
	} else {
		if ((instruction & (1u << 4)) != 0u) {
			return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
		}

		const u32 rm = instruction & 0x0fu;
		if (rm == ARM_REGISTER_PC) {
			return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
		}

		const u32 shift_type = (instruction >> 5) & 0x03u;
		const u32 shift      = (instruction >> 7) & 0x1fu;
		const bool carry = (frame->cpsr & CPSR_C_MASK) != 0u;
		offset = _immediate_shift(
			_register_value(frame, rm, frame->pc + 8u),
			shift_type,
			shift,
			carry
		);
	}

	const u32 base = _register_value(frame, rn, frame->pc + 8u);
	const u32 address = !pre_index
		? base
		: (add ? base + offset : base - offset);

	if ((address & 3u) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNALIGNED;
	}
	return _interworking_target(_read_word(address), target);
}

static arm_instruction_status_t _decode_load_multiple_pc(
	const arm_frame_t             *frame,
	u32                            instruction,
	arm_instruction_step_target_t *target
) {
	const bool pre_index = (instruction & (1u << 24)) != 0u;
	const bool add       = (instruction & (1u << 23)) != 0u;
	const bool psr       = (instruction & (1u << 22)) != 0u;
	const bool writeback = (instruction & (1u << 21)) != 0u;
	const u32 rn         = (instruction >> 16) & 0x0fu;
	const u32 registers  = instruction & 0xffffu;

	if (psr) {
		return ARM_INSTRUCTION_STATUS_UNSUPPORTED;
	}
	if (rn == ARM_REGISTER_PC ||
		(writeback && (registers & (1u << rn)) != 0u)) {
		return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
	}

	const u32 count = _register_count(registers);
	const u32 base  = _register_value(frame, rn, frame->pc + 8u);
	u32 start_address;

	if (add) {
		start_address = base + (pre_index ? 4u : 0u);
	} else {
		start_address = base - (count * 4u) + (pre_index ? 0u : 4u);
	}

	const u32 pc_address = start_address + ((count - 1u) * 4u);
	if ((pc_address & 3u) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNALIGNED;
	}
	return _interworking_target(_read_word(pc_address), target);
}

/*----- Public API ---------------------------------------------------*/

arm_instruction_status_t arm_instruction_get_step_target(
	const arm_frame_t             *frame,
	arm_instruction_step_target_t *target
) {
	if (frame == NULL || target == NULL) {
		return ARM_INSTRUCTION_STATUS_INVALID_ARGUMENT;
	}
	if ((frame->cpsr & (CPSR_T_MASK | CPSR_J_MASK)) != 0u) {
		return ARM_INSTRUCTION_STATUS_INVALID_STATE;
	}
	if ((frame->pc & 3u) != 0u) {
		return ARM_INSTRUCTION_STATUS_UNALIGNED;
	}

	const u32 instruction = _read_word(frame->pc);
	const u32 condition   = instruction >> 28;
	arm_instruction_step_target_t result;

	if (condition != ARM_CONDITION_UNCONDITIONAL &&
		!_condition_passed(condition, frame->cpsr)) {
		_arm_target(frame->pc + 4u, &result);
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// BLX immediate is the only ARMv5 PC-changing unconditional encoding.
	if ((instruction & 0xfe000000u) == 0xfa000000u) {
		const u32 h      = (instruction >> 24) & 1u;
		const u32 offset = _branch_offset(instruction) | (h << 1);
		_thumb_target(frame->pc + 8u + offset, &result);
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}
	if (condition == ARM_CONDITION_UNCONDITIONAL) {
		_arm_target(frame->pc + 4u, &result);
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// BXJ requires Jazelle state knowledge not present in arm_frame_t.
	if ((instruction & 0x0ffffff0u) == 0x012fff20u) {
		return ARM_INSTRUCTION_STATUS_UNSUPPORTED;
	}

	// BKPT and SWI enter exceptions rather than an ordinary successor.
	if ((instruction & 0xfff000f0u) == 0xe1200070u ||
		(instruction & 0x0f000000u) == 0x0f000000u) {
		return ARM_INSTRUCTION_STATUS_UNSUPPORTED;
	}

	// BX and BLX register use address-based interworking.
	const bool branch_exchange =
		(instruction & 0x0ffffff0u) == 0x012fff10u;
	const bool branch_link_exchange =
		(instruction & 0x0ffffff0u) == 0x012fff30u;
	if (branch_exchange || branch_link_exchange) {
		const u32 rm = instruction & 0x0fu;
		if (branch_link_exchange && rm == ARM_REGISTER_PC) {
			return ARM_INSTRUCTION_STATUS_UNPREDICTABLE;
		}

		arm_instruction_status_t status = _interworking_target(
			_register_value(frame, rm, frame->pc + 8u),
			&result
		);
		if (status != ARM_INSTRUCTION_STATUS_OK) {
			return status;
		}
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// B and BL remain in ARM state.
	if ((instruction & 0x0e000000u) == 0x0a000000u) {
		_arm_target(frame->pc + 8u + _branch_offset(instruction), &result);
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// LDM with r15 in its register list performs a load-style PC write.
	if ((instruction & 0x0e100000u) == 0x08100000u &&
		(instruction & (1u << ARM_REGISTER_PC)) != 0u) {
		arm_instruction_status_t status =
			_decode_load_multiple_pc(frame, instruction, &result);
		if (status != ARM_INSTRUCTION_STATUS_OK) {
			return status;
		}
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// Word LDR with r15 as its destination performs address interworking.
	if ((instruction & 0x0c100000u) == 0x04100000u &&
		((instruction >> 12) & 0x0fu) == ARM_REGISTER_PC) {
		arm_instruction_status_t status =
			_decode_load_pc(frame, instruction, &result);
		if (status != ARM_INSTRUCTION_STATUS_OK) {
			return status;
		}
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	// Arithmetic and logical writes to r15 use ARM BranchWritePC semantics.
	if (_is_data_processing(instruction) &&
		((instruction >> 12) & 0x0fu) == ARM_REGISTER_PC) {
		arm_instruction_status_t status =
			_decode_data_processing(frame, instruction, &result);
		if (status != ARM_INSTRUCTION_STATUS_OK) {
			return status;
		}
		*target = result;
		return ARM_INSTRUCTION_STATUS_OK;
	}

	_arm_target(frame->pc + 4u, &result);
	*target = result;
	return ARM_INSTRUCTION_STATUS_OK;
}

/*----- End of file --------------------------------------------------*/
