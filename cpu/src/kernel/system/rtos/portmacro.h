/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * license and copyright intentionally withheld to promote copying into user code.
 */

#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>

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
#define portYIELD()    asm volatile( "MOV r12, #SYS_YIELD\nSWI 0" ::: "r0 r12" )
#define portNOP()      asm volatile( "NOP" )

/* Scheduler utilities. */

extern void vTaskSwitchContext( void );
// Timer callback; the timer driver acknowledges the hardware interrupt.
extern void vPortTickISR( void *context );



/*-----------------------------------------------------------*/


/* Critical section handling. */

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



void vPortEnterCritical( void );
void vPortExitCritical( void );
#define portENTER_CRITICAL()    vPortEnterCritical()
#define portEXIT_CRITICAL()     vPortExitCritical()



/* Task function macros as described on the FreeRTOS.org WEB site. */
#define portTASK_FUNCTION_PROTO( vFunction, pvParameters )    void vFunction( void * pvParameters ) __attribute__( ( noreturn ) )
#define portTASK_FUNCTION( vFunction, pvParameters )          void vFunction( void * pvParameters )

#ifdef __cplusplus
    }
#endif

#endif /* PORTMACRO_H */
