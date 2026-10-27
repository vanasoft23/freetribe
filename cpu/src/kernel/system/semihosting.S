@ Formatted debug output through ARM semihosting.

        .equ MAX_STR_LEN, 512
        .equ SH_SYS_WRITE0, 0x04

        .extern tfp_vsnprintf

        .global print_semihosting
        .type print_semihosting, %function

        .text
        .cpu arm926ej-s
        .code 32

@***********************************************************************
@*                  Function Definition of print_semihosting
@***********************************************************************
@
@ LOG handles literal-text calls before reaching this formatting entry.
@ Uses tinyprintf and a shared 512-byte buffer, then executes the SWI.
@
print_semihosting:
        @ Make R1-R3 contiguous with any arguments on the caller's stack.
        STMDB    sp!, {r0-r3}
        STMDB    sp!, {r4, lr}

        @ tfp_vsnprintf(buffer, MAX_STR_LEN, format, va_list).
        @ Saved R1 is at SP+12; SP remains 8-byte aligned for the call.
        MOV      r2, r0
        ADD      r3, sp, #12
        LDR      r4, =semihost_buffer
        MOV      r0, r4
        MOV      r1, #MAX_STR_LEN
        BL       tfp_vsnprintf

        MOV      r1, r4

        @ J-Link's GDB I/O path can overwrite User/System SP during the SWI.
        @ Mask interrupts until SP is restored, keeping its value in R4.
        MRS      r2, cpsr
        ORR      r3, r2, #0xC0
        MSR      cpsr_c, r3
        STR      r2, [sp, #8]             @ Reuse the saved R0 slot after formatting.
        MOV      r4, sp
        MOV      r0, #SH_SYS_WRITE0
        SWI      0x123456
        MOV      sp, r4
        LDR      r2, [sp, #8]
        MSR      cpsr_c, r2

        @ LR is saved on the stack even when the caller is in SVC mode.
        LDMIA    sp!, {r4, lr}
        ADD      sp, sp, #16
        BX       lr
        .size print_semihosting, .-print_semihosting
        .ltorg

        @ Shared buffer, matching the previous logger's non-reentrant use.
        .bss
        .align 2
        .type semihost_buffer, %object
semihost_buffer:
        .space MAX_STR_LEN
        .size semihost_buffer, .-semihost_buffer
