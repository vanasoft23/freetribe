/*
 * FreeRTOS Kernel <DEVELOPMENT BRANCH>
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*-----------------------------------------------------------
* Implementation of functions defined in portable.h for the Atmel ARM7 port.
*----------------------------------------------------------*/


/* Standard includes. */
#include <stdlib.h>

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Hardware includes. */
#include "hw_types.h"
#include "soc_AM1808.h"
#include "csl_interrupt.h"
#include "csl_timer.h"
#include "csl_cpu.h"

/* Freetribe includes. */
#include "per_timer.h"

/*-----------------------------------------------------------*/

/* Constants required to setup the initial stack. */
#define portINITIAL_SPSR           ( ( StackType_t ) 0x1f ) // System mode, ARM mode, IRQ and FIQ enabled
#define portTHUMB_MODE_BIT         ( ( StackType_t ) 0x20 )
#define portINSTRUCTION_SIZE       ( ( StackType_t ) 4 )

/* Constants required to setup the PIT. */
#define port1MHz_IN_Hz             ( 1000000ul )
#define port1SECOND_IN_uS          ( 1000000.0 )

/* Constants required to handle critical sections. */
#define portNO_CRITICAL_NESTING		( ( uint32_t) 0 )
volatile unsigned long ulCriticalNesting = 9999UL;


#define portINT_LEVEL_SENSITIVE    0
#define portPIT_ENABLE             ( ( uint16_t ) 0x1 << 24 )
#define portPIT_INT_ENABLE         ( ( uint16_t ) 0x1 << 25 )
/*-----------------------------------------------------------*/

/* Setup the PIT to generate the tick interrupts. */
static void prvSetupTimerInterrupt( void );

/* The PIT interrupt handler - the RTOS tick. */
static void vPortTickISR( void );


/*-----------------------------------------------------------*/

/*
 * Initialise the stack of a task to look exactly as if a call to
 * portSAVE_CONTEXT had been called.
 *
 * See header file for description.
 */
StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    StackType_t * pxOriginalTOS;

    pxOriginalTOS = pxTopOfStack;

    /* To ensure asserts in tasks.c don't fail, although in this case the assert
     * is not really required. */
    pxTopOfStack--;

    /* Setup the initial stack of the task.  The stack is set exactly as
     * expected by the portRESTORE_CONTEXT() macro. */

    /* First on the stack is the return address - which in this case is the
     * start of the task.  The offset is added to make the return address appear
     * as it would within an IRQ ISR. */
    *pxTopOfStack = ( StackType_t ) pxCode + portINSTRUCTION_SIZE;
    pxTopOfStack--;

    *pxTopOfStack = ( StackType_t ) 0xaaaaaaaa;    /* R14 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) pxOriginalTOS; /* Stack used when task starts goes in R13. */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x12121212;    /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x11111111;    /* R11 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x10101010;    /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x09090909;    /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x08080808;    /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x07070707;    /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x06060606;    /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x05050505;    /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x04040404;    /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x03030303;    /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x02020202;    /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = ( StackType_t ) 0x01010101;    /* R1 */
    pxTopOfStack--;

    /* When the task starts is will expect to find the function parameter in
     * R0. */
    *pxTopOfStack = ( StackType_t ) pvParameters; /* R0 */
    pxTopOfStack--;

    /* The status register is set for system mode, with interrupts enabled. */
    *pxTopOfStack = ( StackType_t ) portINITIAL_SPSR;

#ifdef THUMB_INTERWORK
    {
        /* We want the task to start in thumb mode. */
        *pxTopOfStack |= portTHUMB_MODE_BIT;
    }
#endif

    pxTopOfStack--;

    /* Interrupt flags cannot always be stored on the stack and will
     * instead be stored in a variable, which is then saved as part of the
     * tasks context. */
    *pxTopOfStack = portNO_CRITICAL_NESTING;

    return pxTopOfStack;
}


/*-----------------------------------------------------------*/

void vPortStartFirstTask( void )
{
    /* Simply start the scheduler.  This is included here as it can only be
    called from ARM mode. */
    portRESTORE_CONTEXT();
}

BaseType_t xPortStartScheduler( void )
{
    /* Start the timer that generates the tick ISR.  Interrupts are disabled
     * here already. */
    prvSetupTimerInterrupt();

    /* Start the first task. */
    vPortStartFirstTask();

    /* Should not get here! */
    return 0;
}

void vPortEndScheduler( void )
{
    /* It is unlikely that the ARM port will require this function as there
     * is nothing to return to.  */
}

/*-----------------------------------------------------------*/


static void vPortTickISR( void )
{

#if NESTED_INTERRUPTS
    // System interrupt already cleared in IRQHandler.
#else
    // Clear interrupt.
    IntSystemStatusClear(SYS_INT_TINT12_1);
#endif
    TimerIntStatusClear(SOC_TMR_2_REGS, TMR_INTSTAT12_TIMER_NON_CAPT);

    
    /* Increment the RTOS tick count, then look for the highest priority
     * task that is ready to run. */
    if( xTaskIncrementTick() != pdFALSE )
    {
        vTaskSwitchContext();
    }

}


static void prvSetupTimerInterrupt( void )
{

    // @TODO: integrate into systick
    #define FREERTOS_TICK_CHAN 11
    t_timer_config cfg = {
        .base_addr = SOC_TMR_2_REGS, // TIMER64P2
        .mode      = TMR_CFG_32BIT_UNCH_CLK_BOTH_INT & ~TMR_TGCR_TIM34RS & ~TMR_TGCR_PLUSEN,
        .period    = 0x5dbf, // 1ms
        .int_flags = TMR_INT_TMR12_NON_CAPT_MODE,
        .int_chan  = FREERTOS_TICK_CHAN,
        .p_isr     = vPortTickISR
    };
    timer_init(cfg);
    
}



/*-----------------------------------------------------------*/

/* The code generated by the GCC compiler uses the stack in different ways at
different optimisation levels.  The interrupt flags can therefore not always
be saved to the stack.  Instead the critical section nesting level is stored
in a variable, which is then saved as part of the stack context. */
void vPortEnterCritical( void )
{
    /* Disable interrupts as per portDISABLE_INTERRUPTS();                          */
    asm volatile (
        "STMDB  SP!, {R0}           \n\t"   /* Push R0.                             */
        "MRS    R0, CPSR            \n\t"   /* Get CPSR.                            */
        "ORR    R0, R0, #0xC0       \n\t"   /* Disable IRQ, FIQ.                    */
        "MSR    CPSR, R0            \n\t"   /* Write back modified value.           */
        "LDMIA  SP!, {R0}" );               /* Pop R0.                              */

    /* Now interrupts are disabled ulCriticalNesting can be accessed
    directly.  Increment ulCriticalNesting to keep a count of how many times
    portENTER_CRITICAL() has been called. */
    ulCriticalNesting++;
}

void vPortExitCritical( void )
{
    if( ulCriticalNesting > portNO_CRITICAL_NESTING )
    {
        /* Decrement the nesting count as we are leaving a critical section. */
        ulCriticalNesting--;

        /* If the nesting level has reached zero then interrupts should be
        re-enabled. */
        if( ulCriticalNesting == portNO_CRITICAL_NESTING )
        {
            /*
             * NOTE:
             * As FIQ is currently not supported, it is not enabled by the macro.
             * If this is necessary, replace #0x80 by #0xC0.
             */

            /* Enable interrupts as per portEXIT_CRITICAL().                    */
            __asm volatile (
                "STMDB  SP!, {R0}       \n\t"   /* Push R0.                     */
                "MRS    R0, CPSR        \n\t"   /* Get CPSR.                    */
                "BIC    R0, R0, #0x80   \n\t"   /* Enable IRQ.                  */
                "MSR    CPSR, R0        \n\t"   /* Write back modified value.   */
                "LDMIA  SP!, {R0}" );           /* Pop R0.                      */
        }
    }
}

/*-----------------------------------------------------------*/



