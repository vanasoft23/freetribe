@-----------------------------------------------------------------------
@
@                     This file is part of Freetribe
@
@                https://github.com/bangcorrupt/freetribe
@
@                                License
@
@                   GNU AFFERO GENERAL PUBLIC LICENSE
@                      Version 3, 19 November 2007
@
@                           AGPL-3.0-or-later
@
@  Freetribe is free software: you can redistribute it and/or modify it
@ under the terms of the GNU Affero General Public License as published by
@    the Free Software Foundation, either version 3 of the License, or
@                   (at your option) any later version.
@
@     Freetribe is distributed in the hope that it will be useful,
@      but WITHOUT ANY WARRANTY; without even the implied warranty
@        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
@          See the GNU General Public License for more details.
@
@   You should have received a copy of the GNU General Public License
@ along with this program. If not, see <https://www.gnu.org/licenses/>.
@
@                       Copyright bangcorrupt 2023
@
@-----------------------------------------------------------------------

@***********************************************************************
@
@ port_context.inc - FreeRTOS context saving/restoring macros
@ Author: vanasoft23
@
@***********************************************************************

@**************************** Externals ********************************

        .extern pxCurrentTCB
        .extern ulCriticalNesting

@ Task frame offsets from pxCurrentTCB->pxTopOfStack (72 bytes total):
@ +0      Critical nesting count
@ +4      Saved task CPSR, restored through SPSR
@ +8..+64 R0-R14 (R13/task SP at +60, R14/task LR at +64)
@ +68     Adjusted return address (resume address + 4)
@
@ Both macros run in an exception mode with interrupt handling controlled
@ by the caller. Save requires the original task registers and adjusted LR.

.macro portRESTORE_CONTEXT
        @ LR = pxCurrentTCB->pxTopOfStack
        LDR      lr, =pxCurrentTCB
        LDR      lr, [lr]
        LDR      lr, [lr]

        @ Pop the nesting count and saved status together.
        LDMIA    lr!, {r0, r1}
        LDR      r2, =ulCriticalNesting
        MSR      SPSR, r1
        STR      r0, [r2]

        @ Restore User/System R0-R14, preserving exception-mode LR.
        LDMIA    lr, {r0-r14}^
        NOP

        @ LR points at saved R0, 60 bytes before the return address.
        LDR      lr, [lr, #60]

        @ Resume the task and copy SPSR to CPSR.
        SUBS     pc, lr, #4
.endm

.macro portSAVE_CONTEXT
        @ Preserve R0 on the exception stack while obtaining task SP.
        STMDB    sp!, {r0}
        STMDB    sp, {sp}^
        NOP
        LDR      r0, [sp, #-4]

        @ Push the adjusted return address onto the task stack.
        STMDB    r0!, {lr}

        @ Use exception-mode LR as the frame pointer and recover R0.
        MOV      lr, r0
        LDMIA    sp!, {r0}

        @ Save User/System registers, including the task's SP and LR.
        STMDB    lr, {r0-r14}^
        @ R0 is now saved; this unbanked load supplies the required gap.
        LDR      r0, =ulCriticalNesting
        SUB      lr, lr, #60

        @ Push nesting count first in memory, then saved task status.
        MRS      r1, SPSR
        LDR      r0, [r0]
        STMDB    lr!, {r0, r1}

        @ Record the completed frame in the task's TCB.
        LDR      r0, =pxCurrentTCB
        LDR      r0, [r0]
        STR      lr, [r0]
.endm
