@ Minimal exception vectors for the standalone hardware-test image.

        .syntax unified
        .arm
        .text

        .global UndefInstHandler
        .type UndefInstHandler, %function
UndefInstHandler:
        b       .
        .size UndefInstHandler, . - UndefInstHandler

        .global SWIHandler
        .type SWIHandler, %function
SWIHandler:
        movs    pc, lr
        .size SWIHandler, . - SWIHandler

        .global AbortHandler
        .type AbortHandler, %function
AbortHandler:
        b       .
        .size AbortHandler, . - AbortHandler

        .global IRQHandler
        .type IRQHandler, %function
IRQHandler:
        b       .
        .size IRQHandler, . - IRQHandler

        .global FIQHandler
        .type FIQHandler, %function
FIQHandler:
        b       .
        .size FIQHandler, . - FIQHandler

        .end
