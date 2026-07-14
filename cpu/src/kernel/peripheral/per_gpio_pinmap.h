/**
 * @file per_gpio_pinmap.h
 * @brief All 144 GPIO pins, named by mux function(s)
 *
 * Pin numbers (1–144) are the GPIO Pin Numbers from Table 17-1 of the
 * AM1802 Technical Reference Manual (SPRUH84C).
 *
 * Naming convention
 * -----------------
 *   PIN_<FUNC1>                     — single peripheral function
 *   PIN_<FUNC1>__<FUNC2>            — two mux options
 *   PIN_<FUNC1>__<FUNC2>__<FUNC3>   — three mux options
 *
 *   Functions are listed in PINMUX select-value order (i.e. the order
 *   the silicon enumerates them: mux=1 first, then 2, then 4, then 8).
 *   GPIO-only pins keep the GP<bank>_<bit> form since that IS their identity.
 *
 *   Abbreviation rules (consistent throughout):
 *     MCASP_AXRn  — McASP serializer/deserializer n
 *     MCASP_ACLKX / ACLKR / AFSX / AFSR / AHCLKX / AHCLKR — McASP clocks/syncs
 *     SPIn_SCSm   — SPI n chip-select m
 *     SPIn_SIMO / SOMI / ENA / CLK — SPI data/control lines
 *     UARTn_TXD / RXD / RTS / CTS  — UART n
 *     I2C0_SDA / SCL
 *     MDIO_D / CLK
 *     MII_*       — MII Ethernet signals
 *     EMA_*       — EMIFA external memory signals
 *     MMCSD0_*    — MMC/SD0
 *     TM64Pn_OUT12 / IN12 — 64-bit timer n compare/capture
 *     DEEPSLEEP / RTC_ALARM / AMUTE / USB_REFCLKIN — misc system
 *     CLKOUT / RESETOUT / RTCK / BOOT_n
 *
 * Pin-to-GPIO-bank mapping:
 *   Pins   1–16  →  GP0[0]–GP0[15]
 *   Pins  17–32  →  GP1[0]–GP1[15]
 *   Pins  33–48  →  GP2[0]–GP2[15]
 *   Pins  49–64  →  GP3[0]–GP3[15]
 *   Pins  65–80  →  GP4[0]–GP4[15]
 *   Pins  81–96  →  GP5[0]–GP5[15]
 *   Pins  97–112 →  GP6[0]–GP6[15]
 *   Pins 113–128 →  GP7[0]–GP7[15]
 *   Pins 129–144 →  GP8[0]–GP8[15]
 */

#ifndef HW_GPIO_PINMAP_H
#define HW_GPIO_PINMAP_H

/* ==========================================================================
 * GP0  —  McASP0 serializers / audio clocks & syncs / UART / USB
 * Pins 1–16  =  GP0[0]–GP0[15]
 * ========================================================================== */

/* Pins 1–8: McASP serializer lines only (single function each) */
#define PIN_MCASP_AXR8                                   1   // GP0[0]
#define PIN_MCASP_AXR9                                   2   // GP0[1]
#define PIN_MCASP_AXR10                                  3   // GP0[2]
#define PIN_MCASP_AXR11                                  4   // GP0[3]
#define PIN_MCASP_AXR12                                  5   // GP0[4]
#define PIN_MCASP_AXR13                                  6   // GP0[5]
#define PIN_MCASP_AXR14                                  7   // GP0[6]
#define PIN_MCASP_AXR15                                  8   // GP0[7]

/* Pins 9–12: system/audio signals with UART mux options */
#define PIN_DEEPSLEEP__RTC_ALARM__UART2_CTS              9   // GP0[8]   mux: 0=DEEPSLEEP, 2=RTC_ALARM, 4=UART2_CTS
#define PIN_AMUTE__UART2_RTS                            10   // GP0[9]   mux: 1=AMUTE,     4=UART2_RTS
#define PIN_MCASP_AHCLKX__USB_REFCLKIN__UART1_CTS      11   // GP0[10]  mux: 1=AHCLKX,    2=USB_REFCLKIN, 4=UART1_CTS
#define PIN_MCASP_AHCLKR__UART1_RTS                    12   // GP0[11]  mux: 1=AHCLKR,    4=UART1_RTS

/* Pins 13–16: McASP frame-sync and bit-clock lines (single function each) */
#define PIN_MCASP_AFSX                                  13   // GP0[12]
#define PIN_MCASP_AFSR                                  14   // GP0[13]
#define PIN_MCASP_ACLKX                                 15   // GP0[14]
#define PIN_MCASP_ACLKR                                 16   // GP0[15]

/* ==========================================================================
 * GP1  —  SPI1 chip-selects / UART / I2C0 / Timer / SPI0 / MDIO / McASP / MII
 * Pins 17–32  =  GP1[0]–GP1[15]
 * ========================================================================== */

/* Pins 17–20: SPI1 chip-selects doubled as UART TX/RX lines */
#define PIN_SPI1_SCS2__UART1_TXD                        17   // GP1[0]   mux: 1=SPI1_SCS[2], 2=UART1_TXD
#define PIN_SPI1_SCS3__UART1_RXD                        18   // GP1[1]   mux: 1=SPI1_SCS[3], 2=UART1_RXD
#define PIN_SPI1_SCS4__UART2_TXD                        19   // GP1[2]   mux: 1=SPI1_SCS[4], 2=UART2_TXD
#define PIN_SPI1_SCS5__UART2_RXD                        20   // GP1[3]   mux: 1=SPI1_SCS[5], 2=UART2_RXD

/* Pins 21–22: SPI1 chip-selects / I2C0 data+clock / Timer outputs */
#define PIN_SPI1_SCS6__I2C0_SDA__TM64P3_OUT12          21   // GP1[4]   mux: 1=SPI1_SCS[6], 2=I2C0_SDA,  4=TM64P3_OUT12
#define PIN_SPI1_SCS7__I2C0_SCL__TM64P2_OUT12          22   // GP1[5]   mux: 1=SPI1_SCS[7], 2=I2C0_SCL,  4=TM64P2_OUT12

/* Pins 23–24: SPI0 chip-selects / Timer in+out / MDIO
   NOTE: mux=0 (reset default) selects the timer capture input on these two pins */
#define PIN_SPI0_SCS0__TM64P1_OUT12__MDIO_D            23   // GP1[6]   mux: 0=TM64P1_IN12, 1=SPI0_SCS[0], 2=TM64P1_OUT12, 8=MDIO_D
#define PIN_SPI0_SCS1__TM64P0_OUT12__MDIO_CLK          24   // GP1[7]   mux: 0=TM64P0_IN12, 1=SPI0_SCS[1], 2=TM64P0_OUT12, 8=MDIO_CLK

/* Pin 25: SPI0 clock / MII receive clock */
#define PIN_SPI0_CLK__MII_RXCLK                         25   // GP1[8]   mux: 1=SPI0_CLK, 8=MII_RXCLK

/* Pins 26–32: McASP serializers / MII transmit signals */
#define PIN_MCASP_AXR1__MII_TXD1                        26   // GP1[9]   mux: 1=AXR1,  8=MII_TXD[1]
#define PIN_MCASP_AXR2__MII_TXD2                        27   // GP1[10]  mux: 1=AXR2,  8=MII_TXD[2]
#define PIN_MCASP_AXR3__MII_TXD3                        28   // GP1[11]  mux: 1=AXR3,  8=MII_TXD[3]
#define PIN_MCASP_AXR4__MII_COL                         29   // GP1[12]  mux: 1=AXR4,  8=MII_COL
#define PIN_MCASP_AXR5__MII_TXCLK                       30   // GP1[13]  mux: 1=AXR5,  8=MII_TXCLK
#define PIN_MCASP_AXR6__MII_TXEN                        31   // GP1[14]  mux: 1=AXR6,  8=MII_TXEN
#define PIN_MCASP_AXR7                                  32   // GP1[15]  single function

/* ==========================================================================
 * GP2  —  EMIFA control + bank-address / SPI1 data+clock / Timer capture
 * Pins 33–48  =  GP2[0]–GP2[15]
 * ========================================================================== */

/* Pins 33–42: EMIFA control and bank-address lines (single function each) */
#define PIN_EMA_CS_0                                    33   // GP2[0]
#define PIN_EMA_WAIT_1                                  34   // GP2[1]
#define PIN_EMA_WEN_DQM_1                               35   // GP2[2]
#define PIN_EMA_WEN_DQM_0                               36   // GP2[3]
#define PIN_EMA_CAS                                     37   // GP2[4]
#define PIN_EMA_RAS                                     38   // GP2[5]
#define PIN_EMA_SDCKE                                   39   // GP2[6]
#define PIN_EMA_CLK                                     40   // GP2[7]
#define PIN_EMA_BA_0                                    41   // GP2[8]
#define PIN_EMA_BA_1                                    42   // GP2[9]

/* Pins 43–46: SPI1 data and clock lines (single function each) */
#define PIN_SPI1_SIMO                                   43   // GP2[10]
#define PIN_SPI1_SOMI                                   44   // GP2[11]
#define PIN_SPI1_ENA                                    45   // GP2[12]
#define PIN_SPI1_CLK                                    46   // GP2[13]

/* Pins 47–48: SPI1 chip-selects / Timer capture inputs
   NOTE: mux=0 (reset default) selects the timer capture input */
#define PIN_SPI1_SCS0__TM64P3_IN12                     47   // GP2[14]  mux: 0=TM64P3_IN12, 1=SPI1_SCS[0]
#define PIN_SPI1_SCS1__TM64P2_IN12                     48   // GP2[15]  mux: 0=TM64P2_IN12, 1=SPI1_SCS[1]

/* ==========================================================================
 * GP3  —  EMIFA data bus [15:8] / control lines
 * Pins 49–64  =  GP3[0]–GP3[15]
 * All single-function; names match the existing EMIFA defines exactly.
 * ========================================================================== */
#define PIN_EMA_D_8                                     49   // GP3[0]
#define PIN_EMA_D_9                                     50   // GP3[1]
#define PIN_EMA_D_10                                    51   // GP3[2]
#define PIN_EMA_D_11                                    52   // GP3[3]
#define PIN_EMA_D_12                                    53   // GP3[4]
#define PIN_EMA_D_13                                    54   // GP3[5]
#define PIN_EMA_D_14                                    55   // GP3[6]
#define PIN_EMA_D_15                                    56   // GP3[7]
#define PIN_EMA_WAIT_0                                  57   // GP3[8]
#define PIN_EMA_A_RW                                    58   // GP3[9]
#define PIN_EMA_OE                                      59   // GP3[10]
#define PIN_EMA_WE                                      60   // GP3[11]
#define PIN_EMA_CS_5                                    61   // GP3[12]
#define PIN_EMA_CS_4                                    62   // GP3[13]
#define PIN_EMA_CS_3                                    63   // GP3[14]
#define PIN_EMA_CS_2                                    64   // GP3[15]

/* ==========================================================================
 * GP4  —  EMIFA address [22:16] + MMCSD0 / MMCSD0 clock / EMIFA data [7:0]
 * Pins 65–80  =  GP4[0]–GP4[15]
 * ========================================================================== */

/* Pins 65–71: EMIFA upper address bits shared with MMCSD0 data/cmd */
#define PIN_EMA_A_16__MMCSD0_DAT5                       65   // GP4[0]   mux: 1=EMA_A[16], 2=MMCSD0_DAT[5]
#define PIN_EMA_A_17__MMCSD0_DAT4                       66   // GP4[1]   mux: 1=EMA_A[17], 2=MMCSD0_DAT[4]
#define PIN_EMA_A_18__MMCSD0_DAT3                       67   // GP4[2]   mux: 1=EMA_A[18], 2=MMCSD0_DAT[3]
#define PIN_EMA_A_19__MMCSD0_DAT2                       68   // GP4[3]   mux: 1=EMA_A[19], 2=MMCSD0_DAT[2]
#define PIN_EMA_A_20__MMCSD0_DAT1                       69   // GP4[4]   mux: 1=EMA_A[20], 2=MMCSD0_DAT[1]
#define PIN_EMA_A_21__MMCSD0_DAT0                       70   // GP4[5]   mux: 1=EMA_A[21], 2=MMCSD0_DAT[0]
#define PIN_EMA_A_22__MMCSD0_CMD                        71   // GP4[6]   mux: 1=EMA_A[22], 2=MMCSD0_CMD

/* Pin 72: MMCSD0 clock only */
#define PIN_MMCSD0_CLK                                  72   // GP4[7]   single function

/* Pins 73–80: EMIFA data bus lower byte (single function each) */
#define PIN_EMA_D_0                                     73   // GP4[8]
#define PIN_EMA_D_1                                     74   // GP4[9]
#define PIN_EMA_D_2                                     75   // GP4[10]
#define PIN_EMA_D_3                                     76   // GP4[11]
#define PIN_EMA_D_4                                     77   // GP4[12]
#define PIN_EMA_D_5                                     78   // GP4[13]
#define PIN_EMA_D_6                                     79   // GP4[14]
#define PIN_EMA_D_7                                     80   // GP4[15]

/* ==========================================================================
 * GP5  —  EMIFA address [15:0] / MMCSD0 data [7:6] on upper two pins
 * Pins 81–96  =  GP5[0]–GP5[15]
 * ========================================================================== */

/* Pins 81–94: EMIFA lower address bits (single function each) */
#define PIN_EMA_A_0                                     81   // GP5[0]
#define PIN_EMA_A_1                                     82   // GP5[1]
#define PIN_EMA_A_2                                     83   // GP5[2]
#define PIN_EMA_A_3                                     84   // GP5[3]
#define PIN_EMA_A_4                                     85   // GP5[4]
#define PIN_EMA_A_5                                     86   // GP5[5]
#define PIN_EMA_A_6                                     87   // GP5[6]
#define PIN_EMA_A_7                                     88   // GP5[7]
#define PIN_EMA_A_8                                     89   // GP5[8]
#define PIN_EMA_A_9                                     90   // GP5[9]
#define PIN_EMA_A_10                                    91   // GP5[10]
#define PIN_EMA_A_11                                    92   // GP5[11]
#define PIN_EMA_A_12                                    93   // GP5[12]
#define PIN_EMA_A_13                                    94   // GP5[13]

/* Pins 95–96: EMIFA A14/A15 shared with MMCSD0 extended data lines */
#define PIN_EMA_A_14__MMCSD0_DAT7                       95   // GP5[14]  mux: 1=EMA_A[14], 2=MMCSD0_DAT[7]
#define PIN_EMA_A_15__MMCSD0_DAT6                       96   // GP5[15]  mux: 1=EMA_A[15], 2=MMCSD0_DAT[6]

/* ==========================================================================
 * GP6  —  GPIO-only bank (mostly) / CLKOUT / RESETOUT
 * Pins 97–112  =  GP6[0]–GP6[15]
 * ========================================================================== */
#define PIN_GP6_0                                       97   // GP6[0]   GPIO only
#define PIN_GP6_1                                       98   // GP6[1]   GPIO only
#define PIN_GP6_2                                       99   // GP6[2]   GPIO only
#define PIN_GP6_3                                      100   // GP6[3]   GPIO only
#define PIN_GP6_4                                      101   // GP6[4]   GPIO only
#define PIN_GP6_5                                      102   // GP6[5]   GPIO only
#define PIN_GP6_6                                      103   // GP6[6]   GPIO only
#define PIN_GP6_7                                      104   // GP6[7]   GPIO only
#define PIN_GP6_8                                      105   // GP6[8]   GPIO only
#define PIN_GP6_9                                      106   // GP6[9]   GPIO only
#define PIN_GP6_10                                     107   // GP6[10]  GPIO only
#define PIN_GP6_11                                     108   // GP6[11]  GPIO only
#define PIN_GP6_12                                     109   // GP6[12]  GPIO only
#define PIN_GP6_13                                     110   // GP6[13]  GPIO only
#define PIN_CLKOUT                                     111   // GP6[14]  single function
#define PIN_RESETOUT                                   112   // GP6[15]  single function

/* ==========================================================================
 * GP7  —  Boot configuration straps / GPIO-only
 * Pins 113–128  =  GP7[0]–GP7[15]
 * NOTE: BOOT[n] pins are sampled at reset; they are free GPIO afterwards.
 * ========================================================================== */
#define PIN_BOOT_0                                     113   // GP7[0]
#define PIN_BOOT_1                                     114   // GP7[1]
#define PIN_BOOT_2                                     115   // GP7[2]
#define PIN_BOOT_3                                     116   // GP7[3]
#define PIN_BOOT_4                                     117   // GP7[4]
#define PIN_BOOT_5                                     118   // GP7[5]
#define PIN_BOOT_6                                     119   // GP7[6]
#define PIN_BOOT_7                                     120   // GP7[7]
#define PIN_GP7_8                                      121   // GP7[8]   GPIO only
#define PIN_GP7_9                                      122   // GP7[9]   GPIO only - Factory firmware sets rise/fall trigs on this
#define PIN_GP7_10                                     123   // GP7[10]  GPIO only
#define PIN_GP7_11                                     124   // GP7[11]  GPIO only
#define PIN_GP7_12                                     125   // GP7[12]  GPIO only
#define PIN_GP7_13                                     126   // GP7[13]  GPIO only
#define PIN_GP7_14                                     127   // GP7[14]  GPIO only
#define PIN_GP7_15                                     128   // GP7[15]  GPIO only

/* ==========================================================================
 * GP8  —  JTAG / SPI0 data+CS / McASP AXR0 / UART0 / MII receive / GPIO-only
 * Pins 129–144  =  GP8[0]–GP8[15]
 * ========================================================================== */

/* Pin 129: JTAG return clock only */
#define PIN_RTCK                                       129   // GP8[0]   single function

/* Pins 130–133: SPI0 chip-selects / UART0 flow-control+data / MII Rx data */
#define PIN_SPI0_SCS2__UART0_RTS__MII_RXD0            130   // GP8[1]   mux: 1=SPI0_SCS[2], 2=UART0_RTS, 8=MII_RXD[0]
#define PIN_SPI0_SCS3__UART0_CTS__MII_RXD1            131   // GP8[2]   mux: 1=SPI0_SCS[3], 2=UART0_CTS, 8=MII_RXD[1]
#define PIN_SPI0_SCS4__UART0_TXD__MII_RXD2            132   // GP8[3]   mux: 1=SPI0_SCS[4], 2=UART0_TXD, 8=MII_RXD[2]
#define PIN_SPI0_SCS5__UART0_RXD__MII_RXD3            133   // GP8[4]   mux: 1=SPI0_SCS[5], 2=UART0_RXD, 8=MII_RXD[3]

/* Pins 134–135: SPI0 data lines / MII carrier+error signals */
#define PIN_SPI0_SIMO__MII_CRS                         134   // GP8[5]   mux: 1=SPI0_SIMO, 8=MII_CRS
#define PIN_SPI0_SOMI__MII_RXER                        135   // GP8[6]   mux: 1=SPI0_SOMI, 8=MII_RXER

/* Pin 136: McASP serializer 0 / MII transmit data 0
   NOTE: GP8[7] GPIO select is mux value 4h, not 8h — it sits between AXR0 and MII_TXD[0] */
#define PIN_MCASP_AXR0__MII_TXD0                       136   // GP8[7]   mux: 1=AXR0, 8=MII_TXD[0]

/* Pins 137–144: GPIO-only */
#define PIN_GP8_8                                      137   // GP8[8]   GPIO only
#define PIN_GP8_9                                      138   // GP8[9]   GPIO only
#define PIN_GP8_10                                     139   // GP8[10]  GPIO only
#define PIN_GP8_11                                     140   // GP8[11]  GPIO only
#define PIN_GP8_12                                     141   // GP8[12]  GPIO only
#define PIN_GP8_13                                     142   // GP8[13]  GPIO only
#define PIN_GP8_14                                     143   // GP8[14]  GPIO only
#define PIN_GP8_15                                     144   // GP8[15]  GPIO only

/* ==========================================================================
 * Helper macros
 * ========================================================================== */
#define GPIO_ABS(pin)   ((pin) - 1)
#define GPIO_BIT(pin)   (GPIO_ABS(pin) % 32)
#define GPIO_MASK(pin)  (1u << GPIO_BIT(pin))

#endif /* HW_GPIO_PINMAP */
