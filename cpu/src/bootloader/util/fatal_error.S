@----------------------------------------------------------------------
@
@ Capture the ARM call-boundary frame before entering the C fatal handler.
@
@----------------------------------------------------------------------

#include "arm_frame.h"

        .extern g_fatal_active
        .extern g_fatal_frame
        .extern fatal_error_impl

        .global fatal_error
        .type   fatal_error, %function

        .text
        .align  2
        .code   32

fatal_error:
        @ Build an arm_frame_t on the caller's stack. ARM_FRAME_SIZE is a
        @ multiple of eight, so the public-interface stack alignment remains
        @ valid when the original variadic arguments are restored below.
        SUB      sp, sp, #ARM_FRAME_SIZE
        STMIA    sp, {r4-r11}

        ADD      r4, sp, #ARM_FRAME_SIZE
        STR      r4, [sp, #ARM_FRAME_SP_OFFSET]
        STR      lr, [sp, #ARM_FRAME_LR_OFFSET]

        MRS      r4, cpsr
        STR      r4, [sp, #ARM_FRAME_CPSR_OFFSET]
        ORR      r5, r4, #0xC0
        MSR      cpsr_c, r5

        MOV      r4, #0
        STR      r4, [sp, #ARM_FRAME_PADDING_OFFSET]

        ADD      r4, sp, #ARM_FRAME_R0_OFFSET
        STMIA    r4, {r0-r3}
        STR      r12, [sp, #ARM_FRAME_R12_OFFSET]

        @ The bootloader is ARM-state code, so the call instruction is four
        @ bytes immediately before the return address held in LR.
        SUB      r4, lr, #4
        STR      r4, [sp, #ARM_FRAME_PC_OFFSET]

        @ A recursive fatal must not overwrite the first useful frame.
        LDR      r4, =g_fatal_active
        LDR      r5, [r4]
        CMP      r5, #0
        BNE      .Lfatal_halt
        MOV      r5, #1
        STR      r5, [r4]

        @ Copy the temporary frame into persistent debugger-visible storage.
        LDR      r4, =g_fatal_frame
        MOV      r5, sp
        MOV      r6, #(ARM_FRAME_SIZE / 4)
.Lcopy_frame:
        LDR      r7, [r5], #4
        STR      r7, [r4], #4
        SUBS     r6, r6, #1
        BNE      .Lcopy_frame

        @ Restore the exact fatal_error(format, ...) call ABI, then tail-call
        @ the non-returning C implementation without changing LR.
        ADD      r4, sp, #ARM_FRAME_R0_OFFSET
        LDMIA    r4, {r0-r3}
        LDR      r12, [sp, #ARM_FRAME_R12_OFFSET]
        LDR      lr, [sp, #ARM_FRAME_LR_OFFSET]
        LDMIA    sp, {r4-r11}
        ADD      sp, sp, #ARM_FRAME_SIZE
        B        fatal_error_impl

.Lfatal_halt:
        B        .Lfatal_halt

        .size fatal_error, . - fatal_error

