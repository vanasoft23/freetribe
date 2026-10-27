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
@ exceptionhandler.S - Definitions of exception handlers
@ Author: vanasoft23
@
@***********************************************************************

#include "fatal_error.h"
#include "arm_frame.h"
#ifndef FREETRIBE_FREERTOS
    #include "gdb_monitor_layout.h"
#endif

@************************** Global symbols *****************************
#ifndef FREETRIBE_FREERTOS
        .extern g_monitor
        .extern tusb_int_handler
        .extern tud_task_ext
        .extern tud_cdc_rsp_task
        .extern vPortYield
        .extern g_syscall_table
#endif

        .global IRQHandler
        .global FIQHandler
        .global AbortHandler
        .global SWIHandler
        .global UndefInstHandler
        .global CPUAbortHandler
        .global fnRAMVectors
        .global p_IRQPriority

        .global exception_kind
        .equ EXCEPTION_NO_ISR,      0
        .equ EXCEPTION_ABORT,       1
        .equ EXCEPTION_UNDEF_INSTR, 2
#ifdef DEBUG
        .global last_irq_handler
#endif

        .equ NUM_INTERRUPTS, 101
        .equ ADDR_HIPVR1, (0xFFFEE000) + (0x1600 + (0 * 4))
        .equ ADDR_HIPIR2, (0xFFFEE000) + (0x900 + (1 * 4))
        .equ SWI_NUM_MASK, 0xFF000000
        .equ CPSR_MODE_MASK, 0x1F
        .equ MODE_SVC, 0x13

        .equ GDB_SIGTRAP, 5
        
        .equ SYS_YIELD, 0

@**************************** Text Section *****************************
        .text
        .cpu arm926ej-s
        .code 32

#ifdef FREETRIBE_FREERTOS
        #include "port_context.inc"
#endif

        .global __exception_handlers_start
__exception_handlers_start:

    CMP     r7, #0                        @ SYS_YIELD
    BEQ     SWI_Yield

    @ Ordinary syscall
    STMFD   sp!, {r4-r11, r12, lr}

    SUB     sp, sp, #8
    STR     r7, [sp]

    BL      syscall_dispatch

    ADD     sp, sp, #8
    LDMFD   sp!, {r4-r11, r12, lr}
    MOVS    pc, lr

@***********************************************************************
@*                  Function Definition of SWI Handler
@***********************************************************************
@
@ R12 is the syscall number
@
SWIHandler:
        @ FreeRTOS yield gets fast path
        CMP      r12, #SYS_YIELD
        BEQ      vPortYield               @ doesn't return here
        
        STMFD    sp!, {r4-r11, r12, lr}
        LDR      r14, =g_syscall_table
        LDR      r14, [r14, r12, LSL #2]
        BLX      r12
        LDMFD    sp!, {r4-r11, r12, lr}
        
        @ Return after SWI and restore CPSR
        MOVS     pc, lr                   
        
@         @ TODO: USB GDBstub print
@         @ CMP      r0, 0x123456
@         @ BL       gdb_stub_print
        
        
@         LDR      r0, [lr, #-4]            @ R0 contains SWI instruction
@         BICS     r0, r0, #SWI_NUM_MASK    @ Get the SWI number, setting CPSR flags
        
@ #ifdef FREETRIBE_FREERTOS
@         @ Yield must receive the task's original R0 and a balanced SVC stack.
@         BEQ      vPortYield               @ CPSR Zero flag means SWI 0
@ #endif



@         MOVS     pc, lr                   @ Return after SWI and restore CPSR


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
        STMFD    sp!, {r0-r3, r12, lr}    @ Save part of context in IRQ stack

        @---------------------------------------------------------------
        @ Safe execution of ISR
        @---------------------------------------------------------------

        @ Snapshot the current highest-priority IRQ.
        LDR      r0, =ADDR_HIPIR2
        LDR      r1, [r0]

        @ Reject NONE (bit 31) and out-of-range indices in one comparison.
        CMP      r1, #NUM_INTERRUPTS
        BHS      irq_spurious

        @ Dispatch the same index that was validated.
        LDR      r0, =fnRAMVectors
        LDR      r2, [r0, r1, LSL #2]

#ifdef DEBUG
        LDR      r3, =last_irq_handler
        STR      r2, [r3]
#endif

        @ Execute ISR, but protect against enabled but unregistered system
        @ interrupt. Avoids calling fnRAMVectors with a nullptr.
        CMP      r2, #0
        BEQ      irq_no_isr
        BLX      r2                       @ Call ISR; saving return address in LR

#ifndef FREETRIBE_FREERTOS

        @ In the USB GDBstub, Ctrl+C might have been detected down the
        @ line from TinyUSB ISR. If so, we don't immediately return
        @ from this IRQHandler.
        LDR      r0, gdb_should_stop
        CMP      r0, #0
        LDMEQFD  sp!, { r0-r3, r12, pc}^  @ No stop: restore context and return

        @---------------------------------------------------------------
        @ Complete arm_frame_t for the GDB monitor
        @ Original r0-r3/r12/PC are already saved.
        @ Add r4-r11, interrupted SP/LR, CPSR, and padding.
        @---------------------------------------------------------------
        BL       preserve_remaining_context

        @ Give GDB monitor the pointer to the IRQ frame
        LDR      r3, =g_monitor
        STR      sp, [r3, #GDB_MONITOR_FRAME_PTR_OFFSET]

        @ Tell GDB monitor the frame is ready
        MOV      r0, #1
        STRB     r0, [r3, #GDB_MONITOR_FRAME_READY_OFFSET]

        @---------------------------------------------------------------
        @ Loop, only running USB CDC task.
        @ All other device callbacks other than CDC should be disabled.
        @---------------------------------------------------------------
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
        BL       tud_cdc_rsp_task

        @ Continue or loop
        LDR      r0, gdb_should_stop      @ R0 contains stop request bool
        CMP      r0, #0                   @ Was no stop requested?
        BNE      trap_loop                @ Then loop

        @ Clean up on GDB continue
        ADD      sp, sp, #48              @ Discard the extended frame and alignment padding

        LDMFD    sp!, {r0-r3, r12, pc}^   @ Pop directly into PC, restoring CPSR flags atomically

#else
        LDMFD    sp!, {r0-r3, r12, pc}^   @ Restore context and return
#endif
        @---------------------------------------------------------------

irq_no_isr:
        MOV      r0, #EXCEPTION_NO_ISR
        B        exception_common

        @---------------------------------------------------------------
        @ Spurious interrupts are completely ignored: return
        @---------------------------------------------------------------
irq_spurious:
        LDMFD    sp!, {r0-r3, r12, pc}^   @ Pop directly into PC, restoring CPSR flags atomically



        .align 2
#ifndef FREETRIBE_FREERTOS
        .global gdb_should_stop
        .type   gdb_should_stop, %object
gdb_should_stop:
        .word   0
        .size   gdb_should_stop, 4
#endif

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
        STMFD    sp!, {r0-r3, r12, lr}    @ Preserve context
        MOV      r0, #EXCEPTION_ABORT
        B        exception_common

@***********************************************************************
@*         Function Definition of Undefined Instruction Handler
@***********************************************************************
UndefInstHandler:
        STMFD    sp!, {r0-r3, r12, lr}    @ Preserve part of context

#ifdef FREETRIBE_FREERTOS
        MOV      r0, #EXCEPTION_UNDEF_INSTR
        B        exception_common
#else
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
        MOV      r0, #EXCEPTION_UNDEF_INSTR
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
#endif

@***********************************************************************
@*           Context saving routine
@***********************************************************************
@
@ R4-R11, CPSR, SPSR of USR/SYS/SVC callee will be gathered in this
@ routine and put on the stack.
#
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
preserve_remaining_context:
        @ Allocate extra stack space to complete the arm_frame_t
        SUB      sp, sp, #48              @ R4 R5 R6 R7 R8 R9 R10 R11 SP LR CPSR + 4 byte padding

        @ Push R4, R11, SP_sys, LR_sys from usr/sys context to stack
        STMIA    sp, {r4-r11, sp, lr}^    @ caret selects usr/sys for banked registers

        @ SVC mode callee means the sp_sys and lr_sys we pushed were incorrect,
        @ so we have to replace them with sp_svc and lr_svc.
        @ This part gets skipped if callee was not in SVC mode
        MRS      r3, spsr                 @ R3 contains SPSR_irq
        AND      r1, r3, #CPSR_MODE_MASK
        CMP      r1, #MODE_SVC
        BNE      sp_lr_good

        @ Preserve cpsr_irq
        MRS      r2, cpsr                 @ R2 contains CPSR_irq

        @ Switch to SVC mode
        BIC      r1, r2, #CPSR_MODE_MASK  @ CPSR mode bit cleared
        ORR      r1, r1, #MODE_SVC        @ Set SVC mode bit
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

sp_lr_good:

        @ Store SPSR_irq, which is the CPSR of caller
        STR      r3, [sp, #ARM_FRAME_CPSR_OFFSET]

        BX       lr


@***********************************************************************
@*           Function Definition of common exception handler
@***********************************************************************
@
@ Entry: original R0-R3, R12 and exception LR are already on the stack.
@ R4-R11 and SPSR still describe the interrupted USR/SYS/SVC context.
@ Complete arm_frame_t, publish it in g_fatal_frame, then report the error.
@
@ Arguments:
@ - R0: exception kind
@
exception_common:

        @ Mask IRQ/FIQ before capturing and publishing the fatal context.
        MRS     r1, cpsr
        ORR     r1, r1, #0xC0
        MSR     cpsr_c, r1

        @ A recursive exception must not overwrite the first fatal frame.
        LDR     r1, =g_fatal_active
        LDR     r2, [r1]
        CMP     r2, #0
        BNE     .Lexception_halt
        MOV     r2, #1
        STR     r2, [r1]

        @ Save exception kind
        LDR     r1, =exception_kind
        STR     r0, [r1]

        @ Capture rest of USR/SYS/SVC context on stack
        @ Original r0-r3, r12 and LR should already be on the stack.
        BL      preserve_remaining_context

        @ Initialise the padding before copying the complete frame.
        MOV     r3, #0
        STR     r3, [sp, #ARM_FRAME_PADDING_OFFSET]

        @ Put USR/SYS/SVC context in g_fatal_frame
        LDR     r0, =g_fatal_frame
        MOV     r1, sp
        MOV     r2, #(ARM_FRAME_SIZE / 4)
.Lcopy_exception_frame:
        LDR     r3, [r1], #4
        STR     r3, [r0], #4
        SUBS    r2, r2, #1
        BNE     .Lcopy_exception_frame

        @ Discard the stack copy; the saved interrupted SP remains intact.
        ADD     sp, sp, #ARM_FRAME_SIZE

        @ Report using the captured frame without capturing handler context.
        LDR     r0, =exception_messages
        LDR     r1, =exception_kind
        LDR     r1, [r1]
        LDR     r1, [r0, r1, LSL #2]
        LDR     r0, =exception_format    @ Format string
        B       fatal_error_impl

.Lexception_halt:
        B       .Lexception_halt

        .global __exception_handlers_end
__exception_handlers_end:

@***********************************************************************
@***********************************************************************
@***********************************************************************

        .data
        .align  2

exception_kind:
        .word   0
last_irq_handler:
        .word   0x23232323

@***********************************************************************

        .section .rodata
        .align 2

exception_format:
        .asciz  "      EXCEPTION\n%s"
exception_messages:
        .word exception_no_isr
        .word exception_abort
        .word exception_undef

exception_no_isr:
        .asciz  "No registered ISR" 
exception_abort:
        .asciz  "Abort"
exception_undef:
        .asciz  "Undefined instruction"

        .end
