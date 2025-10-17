

/* Scheduler includes. */
#include "FreeRTOS.h"


/*-----------------------------------------------------------*/

/* ISR to handle manual context switches (from a call to taskYIELD()). */
void vPortYieldProcessor( void ) __attribute__((interrupt("SWI"), naked));

/*
 * The scheduler can only be started from ARM mode, hence the inclusion of this
 * function here.
 */
void vPortISRStartFirstTask( void );
/*-----------------------------------------------------------*/

void vPortISRStartFirstTask( void )
{
    /* Simply start the scheduler.  This is included here as it can only be
    called from ARM mode. */
    portRESTORE_CONTEXT();
}
/*-----------------------------------------------------------*/

/*
 * Called by portYIELD() or taskYIELD() to manually force a context switch.
 *
 * When a context switch is performed from the task level the saved task
 * context is made to look as if it occurred from within the tick ISR.  This
 * way the same restore context function can be used when restoring the context
 * saved from the ISR or that saved from a call to vPortYieldProcessor.
 */
void vPortYieldProcessor( void )
{
    /* Within an IRQ ISR the link register has an offset from the true return
    address, but an SWI ISR does not.  Add the offset manually so the same
    ISR return code can be used in both cases. */
    asm volatile ( "ADD       LR, LR, #4" );

    /* Perform the context switch.  First save the context of the current task. */
    portSAVE_CONTEXT();

    /* Find the highest priority task that is ready to run. */
    asm volatile ( "bl vTaskSwitchContext" );

    /* Restore the context of the new task. */
    portRESTORE_CONTEXT();
}

/*-----------------------------------------------------------*/

extern void _pic_IrqHandler(void);

/*
 * When an IRQ exception is triggered, it is handled by this function,
 * that reads the ISR address at the VIC controller and executes it.
 */
void vFreeRTOS_ISR( void ) __attribute__((naked));
void vFreeRTOS_ISR( void )
{
    portSAVE_CONTEXT();
    _pic_IrqHandler();
    portRESTORE_CONTEXT();
}

/*-----------------------------------------------------------*/

