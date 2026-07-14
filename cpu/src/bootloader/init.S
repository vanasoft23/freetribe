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
        .global _initial_stack
        .global start_boot
        .global boot_switch_to_ddr_stacks

@************************ Internal Definitions ********************************

        .set  INITIAL_STACK_SIZE, 0x200
        .set  UND_STACK_SIZE, 0x40
        .set  ABT_STACK_SIZE, 0x40
        .set  FIQ_STACK_SIZE, 0x40
        .set  IRQ_STACK_SIZE, 0x600

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

@
@ Set up temporary ARM stacks for exception modes and SVC mode.
@        
         LDR   r0, =_initial_stack             @ read the stack address

         MSR   cpsr_c, #MODE_UND|I_F_BIT       @ switch to undef mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#UND_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_ABT|I_F_BIT       @ switch to abort mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#ABT_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_FIQ|I_F_BIT       @ switch to FIQ mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#FIQ_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_IRQ|I_F_BIT       @ switch to IRQ mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#IRQ_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_SVC|I_F_BIT       @ change to SVC mode
         MOV   sp,r0                           @ write the stack pointer

@
@ Enter the start_boot function. Execution continues in SVC mode.
@
Enter_main:
         LDR   r10,=start_boot                 @ Get the address of start_boot
         MOV   lr,pc                           @ Dummy return 
         BX    r10                             @ Branch to start_boot
         SUB   pc, pc, #0x08                   @ looping   

@******************************************************************************
@ Switch bootloader stacks to DDR after DDR has been initialized.
@******************************************************************************

boot_switch_to_ddr_stacks:
         LDR   r0, =_ddr_stack                 @ read the stack address

         MSR   cpsr_c, #MODE_UND|I_F_BIT       @ switch to undef mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#UND_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_ABT|I_F_BIT       @ switch to abort mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#ABT_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_FIQ|I_F_BIT       @ switch to FIQ mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#FIQ_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_IRQ|I_F_BIT       @ switch to IRQ mode
         MOV   sp,r0                           @ write the stack pointer
         SUB   r0,r0,#IRQ_STACK_SIZE           @ give stack space

         MSR   cpsr_c, #MODE_SVC|I_F_BIT       @ return to SVC mode
         MOV   sp,r0                           @ leave the rest for C stack
         BX    lr                              @ return to start_boot

@
@ End of the file
@
         .end
