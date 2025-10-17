/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * license and copyright intentionally withheld to promote copying into user code.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

/*-----------------------------------------------------------
 * Port specific definitions.
 *
 * The settings in this file configure FreeRTOS correctly for the
 * given hardware and compiler.
 *
 * These settings should not be altered.
 *-----------------------------------------------------------
 */

/* Type definitions. */
#define portCHAR                 char
#define portLONG                 int32_t
#define portSHORT                int16_t
#define portSTACK_TYPE           uint32_t
#define portBASE_TYPE            portLONG

#define portSTACK_GROWTH         ( -1 )
#define portBYTE_ALIGNMENT       8
#define portPOINTER_SIZE_TYPE    uint32_t
typedef portSTACK_TYPE   StackType_t;
typedef signed char      BaseType_t;
typedef unsigned char    UBaseType_t;

#if ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_16_BITS )
    typedef uint16_t     TickType_t;
    #define portMAX_DELAY    ( TickType_t ) 0xffffU
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_32_BITS )
    typedef uint32_t     TickType_t;
    #define portMAX_DELAY    ( TickType_t ) 0xffffffffU
#elif ( configTICK_TYPE_WIDTH_IN_BITS == TICK_TYPE_WIDTH_64_BITS )
    typedef uint64_t     TickType_t;
    #define portMAX_DELAY    ( TickType_t ) 0xffffffffffffffffU
#else
    #error configTICK_TYPE_WIDTH_IN_BITS set to unsupported tick type width.
#endif



// #define portTICK_PERIOD_MS    ( ( TickType_t ) 1000 / configTICK_RATE_HZ )
#define portYIELD()    asm volatile( "SWI 0" )
#define portNOP()      asm volatile( "NOP" )

/* Scheduler utilities. */

/*
 * portRESTORE_CONTEXT, portRESTORE_CONTEXT, portENTER_SWITCHING_ISR
 * and portEXIT_SWITCHING_ISR can only be called from ARM mode, but
 * are included here for efficiency.  An attempt to call one from
 * THUMB mode code will result in a compile time error.
 */

#define portRESTORE_CONTEXT()                                           \
{                                                                       \
extern volatile void * volatile pxCurrentTCB;                           \
extern volatile uint32_t ulCriticalNesting;                    \
                                                                        \
    /* Set the LR to the task stack. */                                 \
    __asm volatile (                                                    \
    "LDR        R0, =pxCurrentTCB                               \n\t"   \
    "LDR        R0, [R0]                                        \n\t"   \
    "LDR        LR, [R0]                                        \n\t"   \
                                                                        \
    /* The critical nesting depth is the first item on the stack. */    \
    /* Load it into the ulCriticalNesting variable. */                  \
    "LDR        R0, =ulCriticalNesting                          \n\t"   \
    "LDMFD  LR!, {R1}                                           \n\t"   \
    "STR    R1, [R0]                                            \n\t"   \
                                                                        \
    /* Get the SPSR from the stack. */                                  \
    "LDMFD  LR!, {R0}                                           \n\t"   \
    "MSR    SPSR, R0                                            \n\t"   \
                                                                        \
    /* Restore all system mode registers for the task. */               \
    "LDMFD  LR, {R0-R14}^                                       \n\t"   \
    "NOP                                                        \n\t"   \
                                                                        \
    /* Restore the return address. */                                   \
    "LDR       LR, [LR, #+60]                                   \n\t"   \
                                                                        \
    /* And return - correcting the offset in the LR to obtain the */    \
    /* correct address. */                                              \
    "SUBS   PC, LR, #4                                          \n\t"   \
    );                                                                  \
    ( void ) ulCriticalNesting;                                         \
    ( void ) pxCurrentTCB;                                              \
}
/*-----------------------------------------------------------*/

#define portSAVE_CONTEXT()                                              \
{                                                                       \
extern volatile void * volatile pxCurrentTCB;                           \
extern volatile uint32_t ulCriticalNesting;                    \
                                                                        \
    /* Push R0 as we are going to use the register. */                  \
    __asm volatile (                                                    \
    "STMDB  SP!, {R0}                                           \n\t"   \
                                                                        \
    /* Set R0 to point to the task stack pointer. */                    \
    "STMDB  SP,{SP}^                                            \n\t"   \
    "NOP                                                        \n\t"   \
    "SUB    SP, SP, #4                                          \n\t"   \
    "LDMIA  SP!,{R0}                                            \n\t"   \
                                                                        \
    /* Push the return address onto the stack. */                       \
    "STMDB  R0!, {LR}                                           \n\t"   \
                                                                        \
    /* Now we have saved LR we can use it instead of R0. */             \
    "MOV    LR, R0                                              \n\t"   \
                                                                        \
    /* Pop R0 so we can save it onto the system mode stack. */          \
    "LDMIA  SP!, {R0}                                           \n\t"   \
                                                                        \
    /* Push all the system mode registers onto the task stack. */       \
    "STMDB  LR,{R0-LR}^                                         \n\t"   \
    "NOP                                                        \n\t"   \
    "SUB    LR, LR, #60                                         \n\t"   \
                                                                        \
    /* Push the SPSR onto the task stack. */                            \
    "MRS    R0, SPSR                                            \n\t"   \
    "STMDB  LR!, {R0}                                           \n\t"   \
                                                                        \
    "LDR    R0, =ulCriticalNesting                              \n\t"   \
    "LDR    R0, [R0]                                            \n\t"   \
    "STMDB  LR!, {R0}                                           \n\t"   \
                                                                        \
    /* Store the new top of stack for the task. */                      \
    "LDR    R0, =pxCurrentTCB                                   \n\t"   \
    "LDR    R0, [R0]                                            \n\t"   \
    "STR    LR, [R0]                                            \n\t"   \
    );                                                                  \
    ( void ) ulCriticalNesting;                                         \
    ( void ) pxCurrentTCB;                                              \
}

extern void vTaskSwitchContext( void );




/*-----------------------------------------------------------*/


/* Critical section handling. */

#ifdef THUMB_INTERWORK

    void vPortDisableInterruptsFromThumb( void ) __attribute__( ( naked ) );
    void vPortEnableInterruptsFromThumb( void ) __attribute__( ( naked ) );

    #define portDISABLE_INTERRUPTS()    vPortDisableInterruptsFromThumb()
    #define portENABLE_INTERRUPTS()     vPortEnableInterruptsFromThumb()

#else

    static inline void portDISABLE_INTERRUPTS(void)
    {
        asm volatile("    mrs     r0, CPSR\n\t"
                     "    orr     r0, r0, #0x80\n\t"
                     "    msr     CPSR_c, r0");
    }

    static inline void portENABLE_INTERRUPTS(void)
    {
        asm volatile("    mrs     r0, CPSR\n\t"
                     "    bic     r0, r0, #0x80\n\t"
                     "    msr     CPSR_c, r0");
    }

#endif


void vPortEnterCritical( void ) __attribute__( ( naked ) );
void vPortExitCritical( void ) __attribute__( ( naked ) );
#define portENTER_CRITICAL()    vPortEnterCritical()
#define portEXIT_CRITICAL()     vPortExitCritical()



/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters ) __attribute__( ( noreturn ) )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )

#ifdef __cplusplus
    }
#endif

#endif /* PORTMACRO_H */
