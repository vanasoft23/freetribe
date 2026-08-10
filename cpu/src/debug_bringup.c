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
 * @file    debug_bringup.c
 *
 * @brief   Pre-boot initialization of DDR. Required when loading the
 *          Freetribe binary into DDR through GDB from cold start.
 */

/*----- Includes -----------------------------------------------------*/

#include <csl_syscfg.h>

#include "kernel/system/startup.h"

/*----- Macros -------------------------------------------------------*/

#define _STR(x)  #x
#define STR(x)   _STR(x)

#define STACK_ADDR  0xFFFF1FF8   // end of ARM RAM
#define STACK_SIZE  0x200
#define MODE_SVC    0x13
#define I_F_BIT     0xC0

/*----- Extern function prototypes -----------------------------------*/

extern void per_ddr_init(void);

/*----- Extern function implementations ------------------------------*/

__attribute__((naked, noreturn))
void start(void) {

    //
    // set up temporary stack
    //
    asm volatile(
        "ldr   r0, =" STR(STACK_ADDR) "\n\t"             // read the stack address
        "msr   cpsr_c, #" STR(MODE_SVC | I_F_BIT) "\n\t" // change to SVC mode
        "mov   sp, r0\n\t"                               // write the stack pointer
        :
        :
        : "r0", "memory"
    );

    SysCfgRegistersUnlock();

    _psc0_init();

    _psc1_init();

    _pll0_init(PLL_CLK_SRC, PLL0_MUL, PLL0_PREDIV, PLL0_POSTDIV, PLL0_DIV1, PLL0_DIV3, PLL0_DIV7);

    _pll1_init(PLL1_MUL, PLL1_POSTDIV, PLL1_DIV1, PLL1_DIV2, PLL1_DIV3);

    per_ddr_init();

    //
    // DDR ready breakpoint from which freetribe kernel can be loaded & run
    //
    asm volatile(
        "debug_ddr_ready:\n\t"
        "b       debug_ddr_ready\n\t"
    );

}

/*----- End of file --------------------------------------------------*/
