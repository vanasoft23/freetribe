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

@****************************** Global Symbols*********************************
        .global start
        @ .global _initial_stack
        .global start_boot
        @ .global boot_switch_to_ddr_stacks
        @ .global boot_sanitize_and_jump

        @ .extern boot_sanitize_ddr_and_caches

@************************ Internal Definitions ********************************

@
@ Stack must fit in ARM RAM
@ Note: If we need more stack space we can
@       switch Undef GDB trap loop to SVC stack.
@
        .set  SYS_STACK_SIZE, 0x0
        .set  SVC_STACK_SIZE, 0x1300
        .set  IRQ_STACK_SIZE, 0x600
        .set  FIQ_STACK_SIZE, 0x40
        .set  UND_STACK_SIZE, 0x600
        .set  ABT_STACK_SIZE, 0x40

@
@ to set the mode bits in CPSR for different modes
@        
        .set  MODE_USR, 0x10
        .set  MODE_FIQ, 0x11
        .set  MODE_IRQ, 0x12
        .set  MODE_SVC, 0x13
        .set  MODE_ABT, 0x17
        .set  MODE_UND, 0x1B
        .set  MODE_SYS, 0x1F

        .equ  I_F_BIT, 0xC0

@**************************** Code Section ************************************
        .text

@
@ This code is assembled for ARM instructions
@
        .code 32

@******************************************************************************
@ Bootloader entrypoint
@******************************************************************************

start:

#ifdef BOOT_DEBUG_DELAY
@
@ Startup delay useful for JTAG debugging without RESET functionality
@
         LDR   r0, =2323420
.Lstartup_delay:
         SUBS  r0, r0, #1
         BNE   .Lstartup_delay
#endif

@
@ Set up stack
@
         @ Set up the Stack for Undefined mode
         LDR   r0, =_stack
         MSR   cpsr_c, #MODE_UND|I_F_BIT
         MOV   sp, r0
         SUB   r0, r0, #UND_STACK_SIZE

         @ Set up the Stack for Abort mode
         MSR   cpsr_c, #MODE_ABT|I_F_BIT
         MOV   sp, r0
         SUB   r0,r0, #ABT_STACK_SIZE

         @ Set up the Stack for FIQ mode
         MSR   cpsr_c, #MODE_FIQ|I_F_BIT
         MOV   sp,r0
         SUB   r0,r0, #FIQ_STACK_SIZE

         @ Set up the Stack for IRQ mode
         MSR   cpsr_c, #MODE_IRQ|I_F_BIT
         MOV   sp,r0
         SUB   r0,r0, #IRQ_STACK_SIZE

         @ Set up the Stack for SVC mode
         MSR   cpsr_c, #MODE_SVC|I_F_BIT
         MOV   sp, r0
         SUB   r0, r0, #SVC_STACK_SIZE

         @ Set up the Stack for User/System mode
         MSR   cpsr_c, #MODE_SYS|I_F_BIT
         MOV   sp, r0

         @ Switch back to SVC mode
         MSR   cpsr_c, #MODE_SVC|I_F_BIT

@
@ Paint the Stack section here
@
Paint_Stack_Section:

         LDR   r0, =_stack                     @ Start address of stack
         LDR   r1, =(_stack_low)               @ Lowest address of stack
         LDR   r2, paint
Stack_Loop: 
         STR   r2, [r0], #-4                   @ Paint one word in stack
         CMP   r0, r1
         BGT   Stack_Loop                      @ Paint until lowest address of stack

@
@ Enter the start_boot function. Execution continues in SVC mode.
@
Enter_main:
         LDR   r10,=start_boot                 @ Get the address of start_boot
         MOV   lr,pc                           @ Dummy return 
         BX    r10                             @ Branch to start_boot
         SUB   pc, pc, #0x08                   @ looping

paint:
    .word 0xdeadc0de

@******************************************************************************
@ Switch bootloader stacks to DDR after DDR has been initialized.
@******************************************************************************

@ boot_switch_to_ddr_stacks:
@          LDR   r0, =_ddr_stack                 @ read the stack address

@          MSR   cpsr_c, #MODE_UND|I_F_BIT       @ switch to undef mode
@          MOV   sp, r0                          @ write the stack pointer
@          SUB   r0, r0, #UND_STACK_SIZE         @ allocate stack space for next mode

@          MSR   cpsr_c, #MODE_ABT|I_F_BIT       @ switch to abort mode
@          MOV   sp, r0                          @ write the stack pointer
@          SUB   r0, r0, #ABT_STACK_SIZE         @ allocate stack space for next mode

@          MSR   cpsr_c, #MODE_FIQ|I_F_BIT       @ switch to FIQ mode
@          MOV   sp, r0                          @ write the stack pointer
@          SUB   r0, r0, #FIQ_STACK_SIZE         @ allocate stack space for next mode

@          MSR   cpsr_c, #MODE_IRQ|I_F_BIT       @ switch to IRQ mode
@          MOV   sp, r0                          @ write the stack pointer
@          SUB   r0, r0, #IRQ_STACK_SIZE         @ allocate stack space for next mode
         
@          MSR   cpsr_c, #MODE_SYS|I_F_BIT       @ change to SYS/User mode
@          MOV   sp, r0                          @ write the stack pointer
@          SUB   r0, r0, #SYS_STACK_SIZE         @ allocate stack space for next mode

@          MSR   cpsr_c, #MODE_SVC|I_F_BIT       @ change to SVC mode
@          MOV   sp, r0                          @ write the stack pointer

@          BX    lr                              @ return to start_boot



@**************************** Firmware Handoff ********************************


@ @******************************************************************************
@ @ Abandon the DDR stack, sanitize DDR, then enter the firmware image.
@ @
@ @ This is terminal handoff code: DDR sanitization erases the bootloader's
@ @ active stack, so this routine must not return to its caller.
@ @******************************************************************************

@ boot_sanitize_and_jump:
@          LDR   r0, =_initial_stack
@          MSR   cpsr_c, #MODE_SVC|I_F_BIT
@          MOV   sp, r0

@          BL    boot_sanitize_ddr_and_caches
@          LDR   pc, =0xC0000000


@ End of the file
         .end
