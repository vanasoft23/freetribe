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
@
@ Original work by Texas Instruments, modified by bangcorrupt 2023.
@ Adapted from AM335x Starterware.

@
@ Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
@
@
@  Redistribution and use in source and binary forms, with or without
@  modification, are permitted provided that the following conditions
@  are met:
@
@    Redistributions of source code must retain the above copyright
@    notice, this list of conditions and the following disclaimer.
@
@    Redistributions in binary form must reproduce the above copyright
@    notice, this list of conditions and the following disclaimer in the
@    documentation and/or other materials provided with the
@    distribution.
@
@    Neither the name of Texas Instruments Incorporated nor the names of
@    its contributors may be used to endorse or promote products derived
@    from this software without specific prior written permission.
@
@  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
@  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
@  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
@  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
@  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
@  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
@  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
@  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
@  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
@  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
@  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@
@***********************************************************************

@***********************************************************************
@
@ exceptionhandler.S - Definitions of exception handlers
@
@***********************************************************************

@
@ Share header file with assembly source code
@

#include <hw_intc.h>
#include <hw_aintc.h>
#include <soc_AM1808.h>

#include "arm_frame.h"
#include "gdb_monitor_layout.h"

@************************** Global symbols *****************************
        .extern g_monitor
        .extern tusb_int_handler
        .extern tud_task_ext
        .extern tud_cdc_rsp_isr_task

        .global IRQHandler
        .global FIQHandler
        .global AbortHandler
        .global SWIHandler
        .global UndefInstHandler
        .global CPUAbortHandler
        .global fnRAMVectors
        .global p_IRQPriority

        @
        @ Exception handling / kernel panic
        @

        .global boot_exception_kind
        .global boot_exception_lr
        .global boot_exception_cpsr
        .global boot_exception_spsr
        .global boot_exception_sctlr
#ifdef DEBUG
        .global last_irq_handler
#endif
        @ TODO: define these in a header enum as well
        .equ EXCEPTION_NO_ISR,          1
        .equ EXCEPTION_ABORT,           2
        .equ EXCEPTION_UNDEF_INSTR, 3
        

        .equ NUM_INTERRUPTS, 101

        .equ ADDR_HIPVR1, SOC_AINTC_0_REGS + AINTC_HIPVR(0)
        .equ ADDR_HIPVR2, SOC_AINTC_0_REGS + AINTC_HIPVR(1)
        .equ ADDR_HIPIR1, SOC_AINTC_0_REGS + AINTC_HIPIR(0)
        .equ ADDR_HIPIR2, SOC_AINTC_0_REGS + AINTC_HIPIR(1)
        .equ ADDR_HINLR1, SOC_AINTC_0_REGS + AINTC_HINLR(0)
        .equ ADDR_HINLR2, SOC_AINTC_0_REGS + AINTC_HINLR(1)
        .equ ADDR_SECR, SOC_AINTC_0_REGS + AINTC_SECR(0)
        .equ ADDR_SICR, SOC_AINTC_0_REGS + AINTC_SICR
        .equ SWI_NUM_MASK, 0xFF000000
 
        .equ PRI_INDEX_MASK, AINTC_HIPIR_PRI_INDX
        .equ OVERRIDE_MASK, AINTC_HINLR_OVERRIDE

        .equ INTR_BYTE_SHIFT, 0x03
        .equ INTR_BIT_MASK, 0x07

        .equ ADDR_SIR_IRQ, SOC_AINTC_0_REGS + INTC_SIR_IRQ
        .equ ADDR_SIR_FIQ, SOC_AINTC_0_REGS + INTC_SIR_FIQ
        .equ ADDR_CONTROL, SOC_AINTC_0_REGS + INTC_CONTROL
        .equ ADDR_THRESHOLD, SOC_AINTC_0_REGS + INTC_THRESHOLD
        .equ ADDR_IRQ_PRIORITY, SOC_AINTC_0_REGS + INTC_IRQ_PRIORITY

        .equ CPSR_MODE_MASK, 0x1F

        .equ MASK_ACTIVE_IRQ, INTC_SIR_IRQ_ACTIVEIRQ
        .equ MASK_ACTIVE_FIQ, INTC_SIR_FIQ_ACTIVEFIQ
        .equ NEWIRQAGR, INTC_CONTROL_NEWIRQAGR
        .equ NEWFIQAGR, INTC_CONTROL_NEWFIQAGR

        .equ MODE_SYS, 0x1F
        .equ MODE_SVC, 0x13
        .equ MODE_IRQ, 0x12 
        .equ I_BIT, 0x80
        .equ GDB_SIGTRAP, 5

@**************************** Text Section *****************************
        .text
        .code 32
        .global __exception_handlers_start
__exception_handlers_start:

@***********************************************************************
@*                  Function Definition of SWI Handler
@***********************************************************************

SWIHandler:
        STMFD    sp!, {r0-r1, lr}         @ Save context in SVC stack
        LDR      r0, [lr, #-4]            @ R0 points to SWI instruction
        BIC      r0, r0, #SWI_NUM_MASK    @ Get the SWI number
        CMP      r0, #458752
        MRSEQ    r1, spsr                 @ Copy SPSR
        ORREQ    r1, r1, #0x1F            @ Change the mode to System
        MSREQ    spsr_cf, r1              @ Restore SPSR
        LDMFD    sp!, {r0-r1, pc}^        @ Restore registers from IRQ stack


@***********************************************************************
@*                  Function Definition of IRQ Handler
@***********************************************************************
@
@ The IRQ handler jumps to the ISR of highest priority pending IRQ. The address
@ is taken from the HIPVR2 register, which contains the ISR address of highest
@ pending IRQ. This handler does not support nesting.
@
@ Stack layout is the complete arm_frame_t, relative to current/final SP:
@
@ [sp, #0x00]  R4
@ [sp, #0x04]  R5
@ [sp, #0x08]  R6
@ [sp, #0x0C]  R7
@ [sp, #0x10]  R8
@ [sp, #0x14]  R9
@ [sp, #0x18]  R10
@ [sp, #0x1C]  R11
@ [sp, #0x20]  SP_usr/sys
@ [sp, #0x24]  LR_usr/sys
@ [sp, #0x28]  CPSR of interrupted context
@ [sp, #0x2C]  padding
@
@ [sp, #0x30]  R0
@ [sp, #0x34]  R1
@ [sp, #0x38]  R2
@ [sp, #0x3C]  R3
@ [sp, #0x40]  R12
@ [sp, #0x44]  PC
@
@ Total frame size: 0x48 bytes
@ IRQ-entry SP:     sp + 0x48
@
IRQHandler:
        @ Save first piece of caller context in IRQ stack
        @ R0, R1, R2, R3, R12, PC (LR_irq)
        SUB      lr, lr, #4               @ LR holds the correct return address
        STMFD    sp!, {r0-r3, r12, lr}    @ Save context in IRQ stack

        @
        @ Safe execution of ISR handler
        @

        @ Snapshot the current highest-priority IRQ.
        LDR      r0, =ADDR_HIPIR2
        LDR      r1, [r0]

        @ Reject NONE (bit 31) and out-of-range indices in one comparison.
        CMP      r1, #NUM_INTERRUPTS
        BHS      irq_spurious

        @ Dispatch the same index that was validated.
        LDR      r0, =fnRAMVectors
        LDR      r2, [r0, r1, LSL #2]

        @ Protect against an enabled but unregistered system interrupt.
        CMP      r2, #0
        BEQ      irq_spurious

#ifdef DEBUG
        LDR      r3, =last_irq_handler
        STR      r2, [r3]

        @ Avoid calling fnRAMVectors nullptr
        CMP      r2, #0
        BEQ      irq_no_isr
#endif

        @ Execute ISR handler
        MOV      lr, pc                   @ Save return address in LR
        MOV      pc, r2                   @ Go to ISR (still in IRQ mode)


        @ Check whether GDB server requested stop
        LDR      r0, gdb_should_stop      @ R0 contains stop request bool
        CMP      r0, #0                   @ Was no stop requested?
        BEQ      end                      @ Then jump to epilogue

        @
        @ From here we are only in GDB...
        @ Rest of register layout has to be gathered into stack first,
        @ to make stack layout look like an arm_frame_t struct.
        @

        @ Allocate extra stack space to complete the arm_frame_t
        SUB      sp, sp, #48              @ R4 R5 R6 R7 R8 R9 R10 R11 SP LR CPSR + 4 byte padding

        @ Push R4, R11, SP_sys, LR_sys from usr/sys context to stack
        STMIA    sp, {r4-r11, sp, lr}^    @ caret selects sys for banked registers

        @ SVC mode callee means the sp_sys and lr_sys we pushed are incorrect,
        @ so we have to replace them with sp_svc and lr_svc.

        @ Skip this part if callee was not in SVC mode
        MRS      r3, spsr                 @ R3 contains SPSR_irq
        AND      r1, r3, #CPSR_MODE_MASK
        CMP      r1, #MODE_SVC
        BNE      irq_captured_sp_lr

        @ Preserve cpsr_irq
        MRS      r2, cpsr                 @ R2 contains CPSR_irq

        @ Switch to SVC mode
        BIC      r1, r2, #CPSR_MODE_MASK  @ CPSR mode bit cleared
        ORR      r1, r1, #MODE_SVC        @ Set SYS mode bit
        ORR      r1, r1, #0xC0            @ Disable IRQ and FIQ temporarily
        MSR      cpsr_c, r1

        @ Capture sp_svc and sp_lr
        MOV      r0, sp                   @ R0 contains SP_svc
        MOV      r1, lr                   @ R1 contains LR_svc
 
        @ Switch back to IRQ mode
        MSR      cpsr_c, r2               @ SP is now SP_irq again

        @ Overwrite previously wrong SP and LP in stack's arm_frame_t
        STR      r0, [sp, #ARM_FRAME_SP_OFFSET]
        STR      r1, [sp, #ARM_FRAME_LR_OFFSET]

irq_captured_sp_lr:

        @ Store CPSR of caller, which was SPSR_irq stored in R3
        STR      r3, [sp, #ARM_FRAME_CPSR_OFFSET]

        @ Give GDB monitor the pointer to the IRQ frame
        LDR      r3, =g_monitor
        STR      sp, [r3, #GDB_MONITOR_FRAME_PTR_OFFSET]

        @ Tell GDB monitor the frame is ready
        MOV      r0, #1
        STRB     r0, [r3, #GDB_MONITOR_FRAME_READY_OFFSET]

        @
        @ Loop, only running USB CDC task.
        @ All other device callbacks other than CDC should be disabled.
        @
trap_loop:

        @ Poll USB hardware and enqueue DCD events
        MOV      r0, #0                   @ rhport
        MOV      r1, #1                   @ in_isr=true
        BL       tusb_int_handler         

        @ Process queued TinyUSB events without blocking
        MOV      r0, #0                   @ timeout_ms=0
        MOV      r1, #1                   @ in_isr=true
        BL       tud_task_ext             

        @ Process GDB RSP input/output over CDC
        BL       tud_cdc_rsp_isr_task

        @ Continue or loop
        LDR      r0, gdb_should_stop      @ R0 contains stop request bool
        CMP      r0, #0                   @ Was no stop requested?
        BNE      trap_loop                @ Then loop

        @ Clean up on GDB continue
        ADD      sp, sp, #48              @ Discard the extended frame and alignment padding

end:
        LDMFD    sp!, {r0-r3, r12, pc}^   @ Pop directly into PC, restoring CPSR flags atomically


irq_no_isr:
        LDMFD    sp!, {r0-r3, r12, lr}    @ Restore original context
        MOV      r3, #EXCEPTION_NO_ISR
        B        exception_common

irq_spurious:

        LDMFD    sp!, {r0-r3, r12, pc}^   @ Pop directly into PC, restoring CPSR flags atomically




        .align 2
        .global gdb_should_stop
        .type   gdb_should_stop, %object
gdb_should_stop:
        .word   0
        .size   gdb_should_stop, 4

@***********************************************************************
@*                  Function Definition of FIQ Handler
@*********************************************************************** 
@
@ The FIQ Handler jumps to the ISR of the highest priority pending FIQ. The
@ address is taken from HIPVR1, which contains the ISR address of the highest
@ pending FIQ. This handler does not support nesting
@
FIQHandler:

        @ NOTE: when implementing FIQ, please add properly banked regs to
        @       arm_frame_t in IRQHandler to avoid GDB step/stop/continue bugs.

        @
        @ Save the required context in FIQ stack. 
        @
        STMFD    r13!, {r0-r3, r12, r14}  @ Save context in FIQ stack
        LDR      r0, =ADDR_HIPVR1         @ R0 points to address of HIPVR1
        LDR      r1, [r0]                 @ R1 contains address of ISR
        ADD      r14, pc, #0              @ Save return address in LR 
        LDR      pc, [r1]                 @ Go to ISR (still in FIQ mode)
        LDMFD    r13!, {r0-r3, r12, r14}  @ Restore registers from FIQ stack
        SUBS     pc, r14, #0x4            @ Return to program state before FIQ 
        
@***********************************************************************
@*             Function Definition of Abort Handler
@***********************************************************************

AbortHandler:
        MOV      r3, #EXCEPTION_ABORT
        B        exception_common

@***********************************************************************
@*         Function Definition of Undefined Instruction Handler
@***********************************************************************

UndefInstHandler:
        STMFD    sp!, {r0-r3, r12, lr}    @ Preserve the interrupted context

        @
        @ Check for active breakpoints
        @
        LDR      r12, =g_monitor          @ R12 points to g_monitor for this routine
        SUB      r1, lr, #4               @ R1 contains exception address
                                          @ R2 points to first breakpoint in array
        ADD      r2, r12, #GDB_MONITOR_BREAKPTS_OFFSET
                                          @ R3 contains end address of breakpoint array
        ADD      r3, r2, #(NUM_SW_BREAKPOINTS * GDB_BREAKPOINT_SIZE)

undef_breakpt_loop:

        @ Check whether breakpoint is active and hit by virtue of
        @ exception address matching breakpoint address.
        LDR      r0, [r2, #GDB_BREAKPOINT_ADDRESS_OFFSET]
        CMP      r0, r1
        BNE      undef_breakpt_checked

        @ Check whether this is the step (temporary) breakpoint
        ADD      r0, r12, #GDB_MONITOR_BREAKPTS_OFFSET
        CMP      r2, r0
        BNE      undef_bp_hit             @ Persistent: leave it installed

        @ Restore the instruction before exposing the stopped frame
        LDR      r0, [r2, #GDB_BREAKPOINT_ORIGINAL_OFFSET]
        STR      r0, [r1]
        MCR      p15, 0, r1, c7, c10, 1   @ Clean D-cache line by MVA
        MCR      p15, 0, r1, c7, c10, 4   @ Drain the write buffer
        MCR      p15, 0, r1, c7, c5, 1    @ Invalidate I-cache line by MVA

        @ Deactivate breakpoint
        MVN      r0, #0
        STR      r0, [r2, #GDB_BREAKPOINT_ADDRESS_OFFSET]
        
        B        undef_bp_hit
        
undef_breakpt_checked:
        ADD      r2, r2, #GDB_BREAKPOINT_SIZE
        CMP      r2, r3
        BNE      undef_breakpt_loop

        @ No breakpoint was hit...
        @ branch to the common handler with undefined-instruction code
        LDMFD    sp!, {r0-r3, r12, lr}    @ Restore original context
        MOV      r3, #EXCEPTION_UNDEF_INSTR
        B        exception_common

undef_bp_hit:

        @
        @ Rest of register layout has to be gathered into stack first,
        @ to make stack layout look like an arm_frame_t struct.
        @

        @ The saved exception LR becomes the stopped PC and return address
        STR      r1, [sp, #20]

        @ Allocate extra stack space to complete the arm_frame_t
        SUB      sp, sp, #48              @ R4 R5 R6 R7 R8 R9 R10 R11 SP LR CPSR + 4 byte padding

        @ Push R4, R11, SP_sys, LR_sys from usr/sys context to stack
        STMIA    sp, {r4-r11, sp, lr}^    @ caret selects sys for banked registers

        @ SVC mode callee means the sp_sys and lr_sys we pushed are incorrect,
        @ so we have to replace them with sp_svc and lr_svc.

        @ Skip this part if callee was not in SVC mode
        MRS      r3, spsr                 @ R3 contains SPSR_undef
        AND      r1, r3, #CPSR_MODE_MASK
        CMP      r1, #MODE_SVC
        BNE      undef_captured_sp_lr

        @ Preserve cpsr_irq
        MRS      r2, cpsr                 @ R2 contains CPSR_undef

        @ Switch to SVC mode
        BIC      r1, r2, #CPSR_MODE_MASK  @ CPSR mode bit cleared
        ORR      r1, r1, #MODE_SVC        @ Set SYS mode bit
        ORR      r1, r1, #0xC0            @ Disable IRQ and FIQ temporarily
        MSR      cpsr_c, r1

        @ Capture sp_svc and sp_lr
        MOV      r0, sp                   @ R0 contains SP_svc
        MOV      r1, lr                   @ R1 contains LR_svc
 
        @ Switch back to IRQ mode
        MSR      cpsr_c, r2               @ SP is now SP_undef again

        @ Overwrite previously wrong SP and LP in stack's arm_frame_t
        STR      r0, [sp, #ARM_FRAME_SP_OFFSET]
        STR      r1, [sp, #ARM_FRAME_LR_OFFSET]

undef_captured_sp_lr:

        @ Store CPSR of caller, which was SPSR_irq stored in R3
        STR      r3, [sp, #ARM_FRAME_CPSR_OFFSET]

        @ Publish the step trap after its frame and stop reason are ready
        MOV      r0, #GDB_SIGTRAP
        STR      r0, [r12, #GDB_MONITOR_SIGVAL_OFFSET]
        STR      sp, [r12, #GDB_MONITOR_FRAME_PTR_OFFSET]

        @ Enter trap loop in IRQHandler
        LDR      r1, =gdb_should_stop
        MOV      r0, #1
        STR      r0, [r1]
        STRB     r0, [r12, #GDB_MONITOR_FRAME_READY_OFFSET]
        B        trap_loop



@***********************************************************************
@*           Function Definition of common exception handler
@***********************************************************************

exception_common:
        MOV     r2, r14          @ Copy LR_undef/LR_abt into r0 immediately
        MRC     p15, 0, r1, c1, c0, 0
        MRS     r12, spsr
@ Disable all interrupts
        MRS     r0, cpsr

        LDR     r4, =boot_exception_kind
        STR     r3, [r4]
        LDR     r4, =boot_exception_lr
        STR     r2, [r4]
        LDR     r4, =boot_exception_cpsr
        STR     r0, [r4]
        LDR     r4, =boot_exception_spsr
        STR     r12, [r4]
        LDR     r4, =boot_exception_sctlr
        STR     r1, [r4]

        ORR     r0, r0, #0xC0    @ Mask IRQ + FIQ
        MSR     cpsr_c, r0
1:
        B       1b               @ Infinite loop for GDB attach

@
@ Go to infinite loop if returned from C handler
@
loop0:
        B       loop0                      


        .global __exception_handlers_end
__exception_handlers_end:

@***********************************************************************

        .data
        .align  2

boot_exception_kind:
        .word   0
boot_exception_lr:
        .word   0
boot_exception_cpsr:
        .word   0
boot_exception_spsr:
        .word   0
boot_exception_sctlr:
        .word   0
last_irq_handler:
        .word   0


        .end
    
    
    
