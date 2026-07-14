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

/* Original work by Texas Instruments, modified by bangcorrupt 2023. */

/*
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file  startup.c
 *
 * @brief Configures the PLL registers to achieve the required Operating
 *        frequency. Power and sleep controller is activated for UART and
 *        Interrupt controller. Interrupt vector is copied to the ARM RAM
 *        before calling main().
 */

/*----- Includes -----------------------------------------------------*/

#include "hw_ddr2_mddr.h"
#include "hw_pllc_AM1808.h"
#include "hw_syscfg0_AM1808.h"
#include "hw_syscfg1_AM1808.h"
#include "hw_types.h"
#include "soc_AM1808.h"

#include "csl_cp15.h"
#include "csl_psc.h"
#include "csl_syscfg.h"

#include "per_ddr.h"

#include "startup.h"

/*----- Macros -------------------------------------------------------*/

#ifndef KERNEL_DDR_CACHEABLE
#define KERNEL_DDR_CACHEABLE 1
#endif

#ifndef KERNEL_DDR_TEXT_CACHEABLE
#define KERNEL_DDR_TEXT_CACHEABLE KERNEL_DDR_CACHEABLE
#endif

#define MMU_SECTION_SIZE 0x100000u
#define MMU_SECTION_MASK (MMU_SECTION_SIZE - 1u)

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static unsigned int const vecTbl[14] = {0xE59FF018,
                                        0xE59FF018,
                                        0xE59FF018,
                                        0xE59FF018,
                                        0xE59FF014,
                                        0xE24FF008,
                                        0xE59FF010,
                                        0xE59FF010,
                                        (unsigned int)start,
                                        (unsigned int)UndefInstHandler,
                                        (unsigned int)vPortYieldProcessor,
                                        (unsigned int)AbortHandler,
                                        (unsigned int)vFreeRTOS_ISR,
                                        (unsigned int)FIQHandler};

// Note: page table CANNOT be wiped from SRAM after boot!
__attribute__((aligned(16 * 1024)))
    static volatile unsigned int page_table[4 * 1024];

/*----- Extern variable definitions ----------------------------------*/

extern uint8_t _kernel_cacheable_start[];
extern uint8_t _kernel_cacheable_end[];

/*----- Extern function prototypes -----------------------------------*/

int main(void);

/*----- Static function prototypes -----------------------------------*/

static void _copy_vector_table(void);
static int _psc_module_enabled(uint32_t base_addr, uint32_t module_id);
static void _config_cache_mmu(void);
static void _ddr_memtest(void);
static void _boot_abort(void);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Boot strap function which enables the PLL(s) and PSC(s) for basic
 *          module(s)
 *
 * @param   none
 *
 * @return  None.
 *
 * This function is the first function that needs to be called in a system.
 * This should be set as the entry point in the linker script if loading the
 * elf binary via a debugger, on the target. This function never returns, but
 * gives control to the application entry point
 **/
void start_boot(void) {

    // Enable write-protection for registers of SYSCFG module.
    SysCfgRegistersLock();

    // Disable write-protection for registers of SYSCFG module.
    SysCfgRegistersUnlock();

    /// TODO: Configure Master Priority Control

    // Detect whether an initialization sequence was already ran.
    // By the bootloader, or debug bringup for example.
    // In this case, there's no need to initialize PSC, clocks, and DDR.
    if (!_psc_module_enabled(SOC_PSC_0_REGS, HW_PSC_CC0)) { 

        _psc0_init();

        _psc1_init();

        // Set the PLL0 to generate 300MHz for ARM.
        _pll0_init(PLL_CLK_SRC, PLL0_MUL, PLL0_PREDIV, PLL0_POSTDIV,
                   PLL0_DIV1, PLL0_DIV3, PLL0_DIV7);

        _pll1_init(PLL1_MUL, PLL1_POSTDIV, PLL1_DIV1, PLL1_DIV2, PLL1_DIV3);

        per_ddr_init();
    }

    _config_cache_mmu();

    // Initialize the vector table with opcodes.
    _copy_vector_table();
    CP15HighVectorEnable();
    CP15ICacheFlush();

    main();

    _boot_abort();
}

// void delay(unsigned int count) {

//     while (count--)
//         ;
// }

void delay(unsigned int count) {
    asm volatile(
        "1:\n"
        "subs %0, %0, #1\n"
        "bne 1b\n"
        : "+r"(count)
        :
        : "cc"
    );
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief  Returns true if already enabled
 */
static int _psc_module_enabled(uint32_t base_addr, uint32_t module_id) {
    uint32_t state = HWREG(base_addr + (module_id + PSC_PDSTAT0) * 4) & 0x3f;
    return (3 == state);
}

/// TODO: Move peripheral power up to driver init function.
/*
 *  Configure PSC0:
 */
void _psc0_init(void) {

    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_CC0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_TC0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_TC1, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_EMIFA, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_SPI0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_MMCSD0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_AINTC, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_ARM_RAMROM, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_UART0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_SCR0_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_SCR1_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_SCR2_SS, 0, PSC_MDCTL_NEXT_ENABLE);

    // Initialize HW_PSC_ARM domain only if it's not yet enabled,
    // otherwise boot crashes.
    if (!_psc_module_enabled(SOC_PSC_0_REGS, HW_PSC_ARM))
        PSCModuleControl(SOC_PSC_0_REGS, HW_PSC_ARM, 0, PSC_MDCTL_NEXT_ENABLE);

}

/// TODO: Move peripheral power up to driver init function.
/*
 *  Configure PSC1:
 */
void _psc1_init(void) {

    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_CC1, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_USB0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_GPIO, 0, PSC_MDCTL_NEXT_ENABLE);
    // PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_DDR2_MDDR, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_MCASP0, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SPI1, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_UART1, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_UART2, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_TC2, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SCRF0_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SCRF1_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SCRF6_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SCRF7_SS, 0, PSC_MDCTL_NEXT_ENABLE);
    PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SCRF8_SS, 0, PSC_MDCTL_NEXT_ENABLE);

    // Initialize HW_PSC_SHRAM domain only if it's not yet enabled,
    // otherwise boot crashes.
    if (!_psc_module_enabled(SOC_PSC_1_REGS, HW_PSC_SHRAM))
        PSCModuleControl(SOC_PSC_1_REGS, HW_PSC_SHRAM, 0, PSC_MDCTL_NEXT_ENABLE);
    
}

/*
 * @brief   This function Configures the PLL0 registers.
 *          PLL Register are set to achieve the desired frequencies.
 *
 * @param   clk_src
 * @param   pllm             This value is assigned to the PLLMultipler
 * register.
 * @param   prediv           This value is assigned to the PLLMultipler
 * register.
 * @param   postdiv          This value is assigned to the PLL_Postdiv register.
 * @param   div1             This value is assigned to the PLL_DIV1 register.
 * @param   div3             This value is assigned to the PLL_DIV3 register.
 * @param   div7             This value is assigned to the PLL_DIV7 register.
 *
 * @return  Int              Returns success or failure
 */
void _pll0_init(unsigned char clk_src, unsigned char pllm,
                unsigned char prediv, unsigned char postdiv,
                unsigned char div1, unsigned char div3,
                unsigned char div7) {

    // Clear lock bit.
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP0) &=
        ~SYSCFG_CFGCHIP0_PLL_MASTER_LOCK;

    // PLLENSRC must be cleared before PLLEN bit have any effect
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLENSRC;

    // PLLCTL.EXTCLKSRC bit 9 should be left at 0
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_EXTCLKSRC;

    // PLLEN = 0 put pll in bypass mode
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLEN;

    // wait for 4 counts to switch pll to the bypass mode
    delay(4);

    // Select the Clock Mode bit 8 as On Chip Oscillator
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_CLKMODE;
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) |= (clk_src << 8);

    // Clear the PLLRST to reset the PLL
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLRST;

    // Disable PLL out
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) |= PLLC_PLLCTL_PLLDIS;

    // PLL initialization sequece, power up the PLL
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLPWRDN;

    // Enable PLL out.
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLDIS;

    // Wait for 2000 counts
    delay(2000);

    // Program the required multiplier value
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLM) = pllm;

    HWREG(SOC_PLLC_0_REGS + PLLC_PREDIV) = PLLC_PREDIV_PREDEN | prediv;
    HWREG(SOC_PLLC_0_REGS + PLLC_POSTDIV) = PLLC_POSTDIV_POSTDEN | postdiv;

    // Check for the GOSTAT bit in PLLSTAT to clear to 0
    // to indicate that no GO operation is currently in progress
    while (HWREG(SOC_PLLC_0_REGS + PLLC_PLLSTAT) & PLLC_PLLSTAT_GOSTAT)
        ;

    // divider values are assigned
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV1) = PLLC_PLLDIV1_D1EN | div1;

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV2) =
        PLLC_PLLDIV2_D2EN | (((div1 + 1) * 2) - 1);

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV4) =
        PLLC_PLLDIV4_D4EN | (((div1 + 1) * 4) - 1);

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV6) = PLLC_PLLDIV6_D6EN | div1;

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV3) = PLLC_PLLDIV3_D3EN | div3;

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLDIV7) = PLLC_PLLDIV7_D7EN | div7;

    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCMD) |= PLLC_PLLCMD_GOSET;

    // Wait for the Gostat bit in PLLSTAT to clear to 0
    // (completion of phase alignment)
    while (HWREG(SOC_PLLC_0_REGS + PLLC_PLLSTAT) & PLLC_PLLSTAT_GOSTAT)
        ;

    // Wait for 200 counts
    delay(200);

    // set the PLLRST bit in PLLCTL to 1,bring the PLL out of reset
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) |= PLLC_PLLCTL_PLLRST;

    // Wait for 0x960 counts
    delay(0x960);

    // removing pll from bypass mode
    HWREG(SOC_PLLC_0_REGS + PLLC_PLLCTL) |= PLLC_PLLCTL_PLLEN;

    // set PLL lock bit
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP0) |=
        (0x01 << SYSCFG_CFGCHIP0_PLL_MASTER_LOCK_SHIFT) &
        SYSCFG_CFGCHIP0_PLL_MASTER_LOCK;

    /// TODO: Is this the default value?
    ///         Could drive directly from PLL
    ///         to increase speed.
    //
    // Not set in factory firmware.
    // EMIFA driven by PLL0_SYSCLK3
    // HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP3) &= CLK_PLL0_SYSCLK3;
}

/**
 * @brief This function Configures the PLL1 registers.
 * PLL Register are set to achieve the desired frequencies.
 *
 * @param   clk_src
 *          pllm             This value is assigned to the PLL1Multipler
 register.
 *          postdiv          This value is assigned to the PLL1_Postdiv
 register.
 *          div1             This value is assigned to the PLL1_Div1 register.
 *          div2             This value is assigned to the PLL1_Div2 register.
 *          div3             This value is assigned to the PLL1_Div3 register.
 *
 * @return  Int          Returns Success or Failure,depending on the execution
**/
void _pll1_init(unsigned char pllm, unsigned char postdiv,
                unsigned char div1, unsigned char div2,
                unsigned char div3) {
    // Clear PLL lock bit
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP3) &=
        ~SYSCFG_CFGCHIP3_PLL1_MASTER_LOCK;

    // PLLENSRC must be cleared before PLLEN has any effect
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLENSRC;

    // PLLCTL.EXTCLKSRC bit 9 should be left at 0
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_EXTCLKSRC;

    // Set PLLEN=0 to put in bypass mode
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLEN;

    // wait for 4 cycles to allow PLLEN mux
    // switches properly to bypass clock
    delay(4);

    // Clear PLLRST bit to reset the PLL
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLRST;

    // Disable the PLL output
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= PLLC_PLLCTL_PLLDIS;

    // PLL initialization sequence
    // Power up the PLL by setting PWRDN bit set to 0
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLPWRDN;

    // Enable the PLL output
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) &= ~PLLC_PLLCTL_PLLDIS;

    delay(2000);

    // Multiplier value is set
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLM) = pllm;

    HWREG(SOC_PLLC_1_REGS + PLLC_POSTDIV) = PLLC_POSTDIV_POSTDEN | postdiv;

    while (HWREG(SOC_PLLC_1_REGS + PLLC_PLLSTAT) & PLLC_PLLCMD_GOSET)
        ;
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLDIV1) = PLLC_PLLDIV1_D1EN | div1;
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLDIV2) = PLLC_PLLDIV2_D2EN | div2;
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLDIV3) = PLLC_PLLDIV3_D3EN | div3;

    // Set the GOSET bit in PLLCMD to 1 to initiate a new divider transition
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCMD) |= PLLC_PLLCMD_GOSET;

    while (HWREG(SOC_PLLC_1_REGS + PLLC_PLLSTAT) & PLLC_PLLSTAT_GOSTAT)
        ;

    // Wait for the Gostat bit in PLLSTAT to clear to 0
    // (completion of phase alignment).
    delay(200);

    // set the PLLRST bit in PLLCTL to 1,bring the PLL out of reset
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) |= PLLC_PLLCTL_PLLRST;
    delay(0x960);

    // Removing PLL from bypass mode
    HWREG(SOC_PLLC_1_REGS + PLLC_PLLCTL) |= PLLC_PLLCTL_PLLEN;

    // set PLL lock bit
    HWREG(SOC_SYSCFG_0_REGS + SYSCFG0_CFGCHIP3) |=
        (0x1 << SYSCFG_CFGCHIP3_PLL1_MASTER_LOCK_SHIFT) &
        SYSCFG_CFGCHIP3_PLL1_MASTER_LOCK;
}

static void _copy_vector_table(void) {
    unsigned int *dest = (unsigned int *)0xFFFF0000;
    unsigned int *src = (unsigned int *)vecTbl;
    unsigned int count;

    for (count = 0; count < sizeof(vecTbl) / sizeof(vecTbl[0]); count++) {
        dest[count] = src[count];
    }
}

/**
 * @brief   Reconstructed factory SBL function that configures permissions,
 *          pages, cache, MMU.
 *
 * @details The factory bootloader configures the page table to only use
 *          domain 0, and configures domain 0 as Manager. This boils down to
 *          all memory being freely accessible, thus no permission faults are
 *          ever generated.
 *
 *          Virtual address space layout:
 *          By default, everything is identity mapped and uncached.
 *
 *          0x00000000 → 0xC0000000 vector table in DDR (CACHED)
 *             │
 *             ├───────────────────────────────────────────
 *             │  0x80000000 → SRAM bootloader vector table
 *             ├───────────────────────────────────────────
 *             │  0xC0000000 – 0xC3F00000
 *             │       → DDR (CACHED)
 *             ├───────────────────────────────────────────
 *             │  0xC4000000 – 0xC7F00000
 *             │       → DDR (UNCACHED alias mirror)
 *             ├───────────────────────────────────────────
 *             │  
 *          0xFFFFF000
 */
static void _config_cache_mmu(void) {
    int i;
#if KERNEL_DDR_TEXT_CACHEABLE
    uint32_t cacheable_start;
    uint32_t cacheable_end;
#endif
    const uint32_t FLAGS_DEFAULT = 0x00000c12; // Read+Write, no cache or buffer
    const uint32_t FLAGS_CACHE   = 0x00000c1e; // Read+Write, cache   and buffer

    // Default to identity mapping everything without caching
    // Covers 0x0000–0x0FFF (4096 entries total page table)
    for (i = 0; i <= 0xFFF; i++)
        page_table[i] = (i << 20) | FLAGS_DEFAULT;

    // Only kernel text/rodata is cacheable. Mutable DDR remains uncached.
#if KERNEL_DDR_TEXT_CACHEABLE
    cacheable_start = ((uint32_t)_kernel_cacheable_start) >> 20;
    cacheable_end = (((uint32_t)_kernel_cacheable_end + MMU_SECTION_MASK) >> 20);

    for (i = (int)cacheable_start; i < (int)cacheable_end; i++)
        page_table[i] = (i << 20) | FLAGS_CACHE;
#endif

    // DDR uncached alias mirror from 0xC4000000-0xC7F00000
    // Maps shifted physical base: (i - 0x40)
    for (i = 0xC40; i <= 0xC7F; i++)
        page_table[i] = ((i - 0x40) << 20) | FLAGS_DEFAULT;

    // Bootloader SRAM vector table at 0x80000000 gets cached
    page_table[0x800] = (0x800 << 20) | FLAGS_CACHE;

    // Vector table remap from low address space to DDR
    page_table[0] = FLAGS_CACHE | 0xC0000000;
    
    CP15TtbSet((unsigned int)page_table);
    CP15DomainAccessManager(); // factory firmware only sets Domain 0 to Manager
    CP15ICacheFlush();
    CP15DCacheFlush();
    CP15InvalidateTLB();
    CP15MMUEnable();
    CP15ICacheEnable();
    CP15DCacheEnable();

}



static void _boot_abort(void) {
    while (1)
        ;
}

// void privileged_mode(void) { asm("    SWI   458752"); }
//
// void system_mode(void) {
//     asm("    mrs     r0, CPSR\n\t"
//         "    bic     r0, #0x0F\n\t"
//         "    orr     r0, #0x10\n\t "
//         "    msr     CPSR, r0");
// }
//

/// TODO: Read and write CPSR.
//
// unsigned int read_cpsr(void) {
//     asm volatile("eor     r3, %1, %1, ror #16\n\t"
//                  "bic     r3, r3, #0x00FF0000\n\t"
//                  "mov     %0, %1, ror #8\n\t"
//                  "eor     %0, %0, r3, lsr #8"
//                  : "=r"(val)
//                  : "0"(val)
//                  : "r3");
//     return val;
// }

/*----- End of file --------------------------------------------------*/
