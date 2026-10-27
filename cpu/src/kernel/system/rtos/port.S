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
@ port.S - FreeRTOS routines
@ Author: vanasoft23
@
@***********************************************************************

@**************************** Functions ********************************

        .global xPortStartScheduler
        .type xPortStartScheduler, %function
        .global vPortYield
        .type vPortYield, %function
        .global vPortTickISR
        .type vPortTickISR, %function
        .global vPortPauseTick
        .type vPortPauseTick, %function

        .extern xTaskIncrementTick
        .extern vTaskSwitchContext
        .extern per_timer2_start
        .extern per_timer2_stop
        @ .extern aintc_sys_disable

@**************************** Constants ********************************

        .equ TIMER0_BASE, 0x01C20000
        .equ TIMER_HALF_12, 0x1

        .equ MODE_SVC, 0x13
        .equ MODE_SYS, 0x1F
        .equ F_BIT, 0x40
        .equ I_BIT, 0x80

@**************************** Text Section *****************************

        .text
        .cpu arm926ej-s
        .code 32

        #include "port_context.inc"

@***********************************************************************
@*             Function Definition of xPortStartScheduler
@***********************************************************************
xPortStartScheduler:

        @ Start the preemption tick timer.
        LDR      r0, =TIMER0_BASE
        MOV      r1, #TIMER_HALF_12
        BL       per_timer2_start

        @ Switch to SVC mode (with IRQ and FIQ disabled), because when
        @ restoring context, we need to write to SPSR, which is
        @ inaccessible from SYS mode.
        MRS      r0, CPSR
        MOV      r0, #MODE_SVC|I_BIT|F_BIT
        MSR      CPSR_c, r0

        @ Start the first task
        portRESTORE_CONTEXT
        .size xPortStartScheduler, .-xPortStartScheduler



@***********************************************************************
@*             Function Definition of vPortPerformYield
@***********************************************************************
vPortPerformYield:
        @ Within an IRQ ISR the link register has an offset from the
        @ true return address, but an SWI ISR does not. Add the offset
        @ manually so the same ISR return code can be used either way.
        ADD lr, lr, #4

        @ Perform the context switch.
        @ First save the context of the current task.
        portSAVE_CONTEXT

        @ Find the highest priority task that is ready to run.
        BL vTaskSwitchContext

        @ Restore the context of the new task, continuing it's execution.
        portRESTORE_CONTEXT
        .size vPortYield, .-vPortYield



@***********************************************************************
@*             Function Definition of vPortTickISR
@***********************************************************************
vPortTickISR:
        @ Called by per_timer2 after timer and AINTC acknowledgement.
        @ The common IRQ entry/exit must save and restore the task context.
        @ Preserve LR and keep the stack 8-byte aligned for the C calls.
        PUSH     {r4, lr}

        BL       xTaskIncrementTick
        CMP      r0, #0
        BLNE     vTaskSwitchContext

        POP      {r4, pc}
        .size vPortTickISR, .-vPortTickISR


@***********************************************************************
@*             Function Definition of vPortPauseTick
@***********************************************************************
@
@ Allows for (temporary) cooperative scheduling.
@
vPortPauseTick:
        LDR      r0, =TIMER0_BASE
        MOV      r1, TIMER_HALF_12
        BL       per_timer2_stop
        
