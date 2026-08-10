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

/**
 * @file    per_aint.c
 *
 * @brief   Peripheral driver for ARM Interrupt Controller.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <soc_AM1808.h>
#include <csl_interrupt.h>
#include <csl_gpio.h>
#include <hw_aintc.h>
#include <hw_types.h>

#include "per_aintc.h"
#include "per_gpio.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

extern void (*fnRAMVectors[NUM_INTERRUPTS])(void);

/*----- Static function prototypes -----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void per_aintc_init(void) {
    
    // Enable interrupt controller.
    IntAINTCInit();

#if NESTED_INTERRUPTS
    // Enable interrupt nesting.
    IntNestModeSet(AINTC_CR_NESTMODE_INDIVIDUAL);
#endif

    // Enable interrupts in CPSR.
    IntMasterIRQEnable();

    // Enable global interrupts in AINTC.
    IntGlobalEnable();

    // Enable host interrupts in AINTC.
    IntIRQEnable();
}

void per_aintc_register_gpio_interrupt(u8 channel,
                                       u8 pin,
                                       u8 int_type,
                                       void (*isr)(void)) {

    u8 bank = per_gpio_bank_from_pin(pin);
    u8 system_int = SYS_INT_GPIOB0 + bank;
    
    GPIODirModeSet(SOC_GPIO_0_REGS, pin, GPIO_DIR_INPUT);
    GPIOIntTypeSet(SOC_GPIO_0_REGS, pin, int_type);
    GPIOBankIntEnable(SOC_GPIO_0_REGS, bank);

    IntChannelSet(system_int, channel);
    IntRegister(system_int, isr);
    IntSystemEnable(system_int);

}

void per_aintc_clear_status_gpio(u8 pin) {
#if NESTED_INTERRUPTS
    // System interrupt already cleared in IRQHandler.
#else
    // Clear interrupt.
    IntSystemStatusClear(SYS_INT_GPIOB0 + per_gpio_bank_from_pin(pin));
#endif
    GPIOPinIntClear(SOC_GPIO_0_REGS, pin);
}

void per_aintc_change_gpio_int_type(u8 pin, u8 int_type) {
    GPIOIntTypeSet(SOC_GPIO_0_REGS, pin, int_type);
}

void _pic_IrqHandler(void) {
    u32 system_int =
        HWREG(SOC_AINTC_0_REGS + AINTC_HIPIR(1)) & AINTC_HIPIR_PRI_INDX;

    if (system_int < NUM_INTERRUPTS && fnRAMVectors[system_int] != NULL)
        fnRAMVectors[system_int]();
}

/*----- Static function implementations ------------------------------*/

/*----- End of file --------------------------------------------------*/
