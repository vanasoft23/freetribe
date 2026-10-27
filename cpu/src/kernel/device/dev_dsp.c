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
 * @file    dev_dsp.c
 *
 * @brief   Device driver for communicating with Blackfin DSP.
 *
 * @details
 * DSP communication happens over two peripherals: EMIFA and SPI1.
 * SPI1 for DSP booting and serial console, but also mutexed with flash!
 * EMIFA is used for (user) communication with DSP.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "ft_error.h"
#include "ring_buffer.h"

#include <ugui_config.h>

#include "per_gpio.h"
#include "per_spi2.h"
#include "dev_dsp.h"

/*----- Macros -------------------------------------------------------*/

#define SPI1 1

/// TODO: Centralised header for queue lengths.
//        ?? why ??
#define DSP_SPI_TX_BUF_LEN 0x100
#define DSP_SPI_RX_BUF_LEN 0x100

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static rb_t s_rx_rb;
static u8 s_rx_rbmem[DSP_SPI_RX_BUF_LEN];

// static rb_t s_tx_rb;
// static u8 s_tx_rbmem[DSP_SPI_TX_BUF_LEN];


// volatile static bool s_tx_complete = false;
volatile static bool s_rx_complete = false;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static int _dsp_spi_tx_dequeue(u8 *p_byte);
static void _dsp_spi_rx_enqueue(u8 *p_byte);

static void _dsp_spi_tx_boot_blocking(u8 *buffer, u32 length);
static void _dsp_spi_wait_boot_ena(void);
static void _dsp_spi_tx_byte(const u8 *p_byte);
static void _dsp_spi_tx_next(void);
static void _dsp_spi_rx_byte(void);

static void _dsp_spi_tx_callback(void);
static void _dsp_spi_rx_callback(void);

static bool _dsp_spi_idle(void);
bool _dsp_spi_enabled(void);

/*----- Extern function implementations ------------------------------*/

void dev_dsp_init(void)
{
	// rb_err_t err;
	// err  = ring_buffer_init(&s_tx_rb, s_tx_rbmem, sizeof(s_tx_rbmem[0]), ARRAY_SIZE(s_tx_rbmem));
	// err |= ring_buffer_init(&s_rx_rb, s_rx_rbmem, sizeof(s_rx_rbmem[0]), ARRAY_SIZE(s_rx_rbmem));
	// if (RING_BUFFER_OK != err)
	// 	PANIC(PANIC_GENERIC);

#if 0
	per_spi2_set_callbacks(_dsp_spi_tx_callback, _dsp_spi_rx_callback);

	s_tx_complete = true;
	s_rx_complete = true;
#endif
}

/**
 * @brief   True puts DSP in reset.
 */
void dev_dsp_reset(bool state)
{
	per_gpio_set_indexed(PIN_DSP_RESET, !state);
}

void dev_dsp_spi_tx_enqueue(u8 *p_byte)
{
	// /// TODO: Should catch overflow error and
	// ///       redesign so this does not happen.
	// //
	// // Overwrite on overflow.
	// ring_buffer_put_overwrite(&s_tx_rb, p_byte, NULL);

	// if (_dsp_spi_idle()) {
	// 	// Start transmission.
	// 	_dsp_spi_tx_next();
	// }
}

// bool dev_dsp_spi_rx_dequeue(u8 *p_byte)
// {
// 	return RING_BUFFER_OK == ring_buffer_get(&s_rx_rb, p_byte);
// }

// bool dev_dsp_spi_tx_complete(void) { return g_dsp_spi_tx_complete; }

void dev_dsp_spi_poll(void)
{
	// if (_dsp_spi_idle()) {
	// 	_dsp_spi_rx_byte();
	// }
	// u8 byte;
	// rb_err_t err = ring_buffer_get(&s_rx_rb, &byte);
	// per_spi2_rx_blocking(SPI1, buffer, length);
}

void dev_dsp_spi_boot(u8 *buffer, u32 length)
{
	per_spi2_tx_blocking(SPI1, buffer, length);

	// while (SPIIntStatus(SPI1_BASE, DSP_SPI_RECV_FLAG))
	// 	;

	// while (length--) {
	// 	_dsp_spi_wait_boot_ena();

	// 	per_spi_write_blocking(DSP_SPI, buffer++, 1);

	// 	_dsp_spi_wait_boot_ena();

	// 	if (SPIIntStatus(SPI1_BASE, DSP_SPI_RECV_FLAG)) {
	// 		SPIDataReceive(SPI1_BASE);
	// 	}
	// }
}


/// /// TODO: Use GPIO interrupt to set flag.
/// //
/// // Return true if SPI enabled.
/// bool _dsp_spi_enabled(void) {
///     //
///     return !per_gpio_get_indexed(PIN_SPI1_ENA);
/// }

// void dev_dsp_spi_transfer(void) {
//
//     if (_dsp_spi_enabled()) {
//
//         if ((g_dsp_spi_rx_complete && g_dsp_spi_tx_complete) &&
//             _dsp_spi_tx_dequeue(&g_dsp_spi_tx_byte) == 0) {
//
//             _dsp_spi_tx_byte(&g_dsp_spi_tx_byte);
//         }
//     }
// }

/*----- Static function implementations ------------------------------*/

// static void _dsp_spi_rx_enqueue(u8 *p_byte)
// {
// 	ring_buffer_put_overwrite(&s_rx_rb, p_byte, NULL);
// }

static void _dsp_spi_wait_boot_ena(void)
{
	/// TODO: SPI ENA timeout/error?
	while (per_gpio_get_indexed(PIN_SPI1_ENA))
		;
}

// static void _dsp_spi_tx_byte(const u8 *p_byte)
// {
// 	*tx_byte = *p_byte;

// 	s_tx_complete = false;
// 	s_rx_complete = false;

// 	per_spi_chip_format(DSP_SPI, DSP_SPI_COMMAND_DATA_FORMAT,
// 	                    DSP_SPI_CHIP_SELECT, DSP_SPI_COMMAND_CSHOLD);

// 	per_spi_transfer_isr(DSP_SPI, tx_byte, rx_byte, 1);
// }

// static void _dsp_spi_tx_next(void)
// {
// 	u8 byte;
// 	rb_err_t err = ring_buffer_get(&s_tx_rb, &byte);
// 	if (RING_BUFFER_OK == err) {
// 		_dsp_spi_tx_byte(&byte);
// 	}
// }

// static void _dsp_spi_rx_byte(void)
// {
// 	u8 dummy = 0x00;

// 	// Transmit dummy byte to receive.
// 	_dsp_spi_tx_byte(&dummy);
// }

#if 0
static void _tx_isr_callback(void)
{
	s_tx_complete = true;
	ring_buffer_discard(&s_tx_rbd, 1);
#if FREETRIBE_FREERTOS
	// @TODO: wake up task from ISR
#endif
}

static void _rx_isr_callback(void)
{
	_dsp_spi_rx_enqueue(dsp_spi_rx_byte_mem);

	s_rx_complete = true;

	if (s_tx_complete == true) {
		_dsp_spi_tx_next();
	}
}
#endif


// static bool _dsp_spi_idle(void)
// {
// 	return s_tx_complete && s_rx_complete;
// }

/*----- End of file --------------------------------------------------*/
