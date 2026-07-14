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
 * @file    per_spi.c.
 *
 * @brief   Configuration and handling for SPI peripherals.
 */

/// TODO: SPI_1 needs mutex to prevent DSP and flash access collision.

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "macros.h"
#include "startup.h"

#include "csl_interrupt.h"
#include "csl_spi.h"
#include "hw_types.h"

#include "per_spi.h"

/*----- Macros -------------------------------------------------------*/

#define SPI_TRANSMIT_FLAG SPI_SPIFLG_TXINTFLG

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    uint32_t address;
    uint32_t system_int;
    void (*isr)(void);

    uint8_t *tx_buffer;
    uint32_t tx_length;

    uint8_t *rx_buffer;
    uint32_t rx_length;

    uint32_t dat1_control;

    void (*tx_callback)(void);
    void (*rx_callback)(void);

    bool initialised;

} t_spi;

/*----- Static function prototypes -----------------------------------*/

static void _spi0_isr(void);
static void _spi1_isr(void);
static unsigned int _has_rx_data(uint32_t base_addr);

/*----- Static helper prototypes -------------------------------------*/

static void _spi_set_format_register(uint32_t base_addr, uint32_t data_format,
                                     uint32_t module_clock,
                                     uint32_t spi_clock, uint32_t flags,
                                     uint32_t char_length);
// static void _spi_disable(uint32_t base_addr);
static void _spi_set_dat1_control(t_spi *spi, uint32_t flags,
                                  uint8_t chip_select);
static uint32_t _spi_dat1_control(uint32_t base_addr);
static void _spi_transmit_data1(uint32_t base_addr, uint32_t data);
static void _spi_wait_transmit_ready(uint32_t base_addr);
static void _spi_wait_transmit_buffer_ready(uint32_t base_addr);
static void _spi_wait_receive_ready(uint32_t base_addr);
static void _spi_drain_receive_data(uint32_t base_addr);

/*----- Static variable definitions ----------------------------------*/

static const uint32_t g_base_address[SPI_INSTANCES] = {SPI0_BASE, SPI1_BASE};

static const uint32_t g_system_interrupt[SPI_INSTANCES] = {SYS_INT_SPIINT0,
                                                           SYS_INT_SPIINT1};

static const void *g_isr_address[SPI_INSTANCES] = {&_spi0_isr, &_spi1_isr};

static t_spi g_spi[SPI_INSTANCES];

/*----- Extern function implementations ------------------------------*/

void per_spi_init(t_spi_config *config) {

    t_spi *spi = &g_spi[config->instance];

    spi->address = g_base_address[config->instance];
    spi->system_int = g_system_interrupt[config->instance];
    spi->isr = g_isr_address[config->instance];
    spi->tx_buffer = NULL;
    spi->tx_length = 0;
    spi->rx_buffer = NULL;
    spi->rx_length = 0;
    spi->dat1_control = SPI_SPIDAT1_CSNR;
    spi->tx_callback = NULL;
    spi->rx_callback = NULL;
    spi->initialised = false;

    SPIReset(spi->address);
    SPIOutOfReset(spi->address);
    SPIModeConfigure(spi->address, SPI_MASTER_MODE);
    SPIDefaultCSSet(spi->address, SPI_SPIDEF_CSDEF);
    SPISetPinControl(spi->address, SPI_PIN_CTL_FUNC, config->pin_func);
    if (config->pin_dir) {
        SPISetPinControl(spi->address, 1, config->pin_dir);
    }

    if (config->int_enable) {
        // Set interrupt channel.
        IntChannelSet(spi->system_int, config->int_channel);

        // Register the SPI Isr in the Interrupt Vector Table of AINTC.
        IntRegister(spi->system_int, spi->isr);

        // Enable system interrupt in AINTC.
        IntSystemEnable(spi->system_int);

        SPIIntLevelSet(spi->address, config->int_level);
    }

    spi->initialised = true;
}

void per_spi_unregister_interrupts(uint8_t instance) {

    t_spi *spi = &g_spi[instance];

    // If initialised, and interrupts were enabled, we gotta
    // unregister the interrupt in AINTC.
    if (spi->initialised && spi->isr) {
        IntSystemDisable(spi->system_int);
        IntUnRegister(spi->system_int);
        IntChannelSet(spi->system_int, 0); // reset interrupt channel
        // HWREG(spi->address + SPI_SPILVL) = 0; // is this necessary/helping?
        // HWREG(spi->address + SPI_SPIINT0) = 0; // is this necessary/helping?
        spi->isr = NULL;
    }
    
}

void per_spi_set_data_format(t_spi_format *format) {

    uint32_t base_addr = g_base_address[format->instance];
    // bool disable_for_reconfigure =
    //     (HWREG(base_addr + SPI_SPIGCR1) & SPI_SPIGCR1_ENABLE) != 0;

    // if (disable_for_reconfigure) {
    //     _spi_disable(base_addr);
    // }

    _spi_set_format_register(base_addr, format->index, SPI_MODULE_FREQ,
                             format->freq, SPI_DEFAULT_PHASE,
                             format->char_length);

    SPIEnable(base_addr);

    /// TODO: Investigate SPI timings using logic analyser.
}

bool per_spi_initialised(uint8_t instance) {

    return g_spi[instance].initialised;
}

// Select data format, set chip select value and hold.
void per_spi_chip_format(uint8_t instance, uint8_t data_format,
                         uint8_t chip_select, bool cshold) {

    t_spi *spi = &g_spi[instance];
    uint32_t flags = data_format;

    if (cshold) {
        flags |= SPI_CSHOLD;
    }

    _spi_set_dat1_control(spi, flags, chip_select);

}

void per_spi_write_isr(uint8_t instance, uint8_t *buffer, uint32_t length) {

    g_spi[instance].tx_buffer = buffer;
    g_spi[instance].tx_length = length;

    // Enable TX interrupt.
    SPIIntEnable(g_spi[instance].address, SPI_TRANSMIT_INT);
}

void per_spi_write_blocking(uint8_t instance, uint8_t *buffer, uint32_t length) {

    if (!buffer) {
        return;
    }

    uint32_t base_addr = g_spi[instance].address;

    for (uint32_t i = 0; i < length; i++) {

        _spi_wait_transmit_ready(base_addr);

        _spi_transmit_data1(base_addr, buffer[i]);

    }

    _spi_wait_transmit_ready(base_addr);
}

void per_spi_transfer_isr(uint8_t instance, uint8_t *tx_buffer,
                          uint8_t *rx_buffer, uint32_t length) {

    if (tx_buffer != NULL && rx_buffer != NULL && length != 0) {
        g_spi[instance].tx_buffer = tx_buffer;
        g_spi[instance].rx_buffer = rx_buffer;
        g_spi[instance].tx_length = length;
        g_spi[instance].rx_length = length;

        /// TODO: Error interrupts should be enabled in init function.
        ///         Should probably always remain enabled.
        //
        // Enable interrupts.
        SPIIntEnable(g_spi[instance].address,
                     SPI_TRANSMIT_INT | SPI_RECV_INT | SPI_TIMEOUT_INT |
                         SPI_DESYNC_SLAVE_INT);
    }
}

static unsigned int _has_rx_data(uint32_t base_addr) {
    return !(HWREG(base_addr + SPI_SPIBUF) & SPI_SPIBUF_RXEMPTY);
}

void per_spi_transfer_blocking(uint8_t instance, uint8_t *tx_buffer,
                               uint8_t *rx_buffer, uint32_t length) {

    if ((!tx_buffer && !rx_buffer) || length == 0) {
        DEBUG_LOG("per_spi_transfer_blocking ERROR invalid params");
        return;
    }

    uint32_t base_addr = g_spi[instance].address;

    _spi_drain_receive_data(base_addr);

    for (uint32_t i = 0; i < length; i++) {
        uint32_t tx_data = tx_buffer ? tx_buffer[i] : 0;

        _spi_wait_transmit_buffer_ready(base_addr);
        _spi_transmit_data1(base_addr, tx_data);

        _spi_wait_receive_ready(base_addr);
        uint32_t rx_data = SPIDataReceive(base_addr);

        if (rx_buffer) {
            rx_buffer[i] = (uint8_t)rx_data;
        }
    }
}

void per_spi_transfer_blocking_end(uint8_t instance, uint8_t *tx_buffer,
                                   uint8_t *rx_buffer, uint32_t length,
                                   uint8_t data_format,
                                   uint8_t chip_select) {

    if ((!tx_buffer && !rx_buffer) || length == 0) {
        return;
    }

    if (length > 1) {
        per_spi_transfer_blocking(instance, tx_buffer, rx_buffer, length - 1);
    }

    per_spi_chip_format(instance, data_format, chip_select, false);
    per_spi_transfer_blocking(instance,
                              tx_buffer ? &tx_buffer[length - 1] : NULL,
                              rx_buffer ? &rx_buffer[length - 1] : NULL, 1);
}

void per_spi_register_callback(uint8_t instance, t_spi_event event,
                               void (*callback)()) {

    switch (event) {

    case SPI_TX_COMPLETE:
        g_spi[instance].tx_callback = callback;
        break;

    case SPI_RX_COMPLETE:
        g_spi[instance].rx_callback = callback;
        break;

        // case SPI_ERROR:

    default:
        break;
    }
}

/*----- Static function implementations ------------------------------*/

static inline void _spi_isr(t_spi *spi) {

    // Cause of SPI interrupt.
    uint8_t int_id = 0;

#if NESTED_INTERRUPTS
    // System interrupt already cleared in IRQHandler.
#else
    // Clears the system interrupt status of SPI1 in AINTC.
    IntSystemStatusClear(spi->system_int);
#endif

    // Handle all pending interrupts.
    while ((int_id = SPIInterruptVectorGet(spi->address))) {

        switch (int_id) {

        // Tx interrupt.
        case SPI_TX_BUF_EMPTY:

            /// TODO: This test of length may not be necessary
            ///       as interrupt is only enabled if length > 0.
            //
            if (spi->tx_length--) {
                // Write byte to SPI
                _spi_transmit_data1(spi->address, *spi->tx_buffer++);

                if (spi->tx_length == 0) {
                    // Disable the Tx interrupt if buffer is empty.
                    SPIIntDisable(spi->address, SPI_TRANSMIT_INT);

                    if (spi->tx_callback != NULL) {
                        spi->tx_callback();
                    }
                }
            }
            break;

        // Rx interrupt.
        case SPI_RECV_FULL:

            if (spi->rx_length--) {
                // Read byte from SPI
                *spi->rx_buffer++ = SPIDataReceive(spi->address);

                if (spi->rx_length == 0) {
                    // Disable the Rx interrupt if buffer full.
                    SPIIntDisable(spi->address, SPI_RECV_INT);

                    if (spi->rx_callback != NULL) {
                        spi->rx_callback();
                    }
                }
            }
            break;

        /// TODO: Interrogate source of error and trigger callback.
        case SPI_ERR:

            while (true)
                ;

            break;

        default:
            break;
        }
    }

    return;
}

static void _spi0_isr(void) { _spi_isr(&g_spi[0]); }

static void _spi1_isr(void) { _spi_isr(&g_spi[1]); }


/*----- Static helper implementations --------------------------------*/

static void _spi_set_format_register(uint32_t base_addr, uint32_t data_format,
                                     uint32_t module_clock,
                                     uint32_t spi_clock, uint32_t flags,
                                     uint32_t char_length) {

    uint32_t prescale = (module_clock / spi_clock) - 1;
    uint32_t value =
        (SPI_SPIFMT_PRESCALE & (prescale << SPI_SPIFMT_PRESCALE_SHIFT)) |
        (flags & ~(SPI_SPIFMT_PRESCALE | SPI_SPIFMT_CHARLEN)) |
        (char_length & SPI_SPIFMT_CHARLEN);

    HWREG(base_addr + SPI_SPIFMT(data_format)) = value;
}

// static void _spi_disable(uint32_t base_addr) {

//     HWREG(base_addr + SPI_SPIGCR1) &= ~SPI_SPIGCR1_ENABLE;
// }

static void _spi_set_dat1_control(t_spi *spi, uint32_t flags,
                                  uint8_t chip_select) {

    uint32_t base_addr = spi->address;
    uint32_t default_cs = HWREG(base_addr + SPI_SPIDEF) & SPI_SPIDEF_CSDEF;
    uint32_t active_cs =
        ((uint32_t)(chip_select ^ default_cs) << SPI_SPIDAT1_CSNR_SHIFT) &
        SPI_SPIDAT1_CSNR;
    uint32_t data_format =
        (flags & (SPI_SPIDAT1_DFSEL >> SPI_SPIDAT1_DFSEL_SHIFT)) <<
        SPI_SPIDAT1_DFSEL_SHIFT;
    uint32_t control_flags =
        flags & (SPI_SPIDAT1_CSHOLD | SPI_SPIDAT1_WDEL);

    // Apply DAT1 control with the next real transfer, not as a standalone write.
    spi->dat1_control = control_flags | data_format | active_cs;
}

static uint32_t _spi_dat1_control(uint32_t base_addr) {

    for (uint32_t i = 0; i < SPI_INSTANCES; i++) {
        if (g_spi[i].address == base_addr) {
            return g_spi[i].dat1_control;
        }
    }

    return SPI_SPIDAT1_CSNR;
}

static void _spi_transmit_data1(uint32_t base_addr, uint32_t data) {

    HWREG(base_addr + SPI_SPIDAT1) =
        _spi_dat1_control(base_addr) | (data & SPI_SPIDAT1_TXDATA);

}

static void _spi_wait_transmit_ready(uint32_t base_addr) {

    while (!SPIIntStatus(base_addr, SPI_TRANSMIT_FLAG))
        ;
}

static void _spi_wait_transmit_buffer_ready(uint32_t base_addr) {

    while (HWREG(base_addr + SPI_SPIBUF) & SPI_SPIBUF_TXFULL)
        ;
}

static void _spi_wait_receive_ready(uint32_t base_addr) {

    while (!_has_rx_data(base_addr))
        ;
}

static void _spi_drain_receive_data(uint32_t base_addr) {

    while (_has_rx_data(base_addr)) {
        SPIDataReceive(base_addr);
    }
}


/*----- End of file --------------------------------------------------*/
