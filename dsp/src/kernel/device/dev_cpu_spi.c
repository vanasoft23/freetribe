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
 * @file    dev_cpu_spi.c
 *
 * @brief   Device driver for communicating with CPU via SPI.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_spi.h"

#include "ring_buffer.h"

/*----- Macros -------------------------------------------------------*/

/// TODO: Central header for queue sizes.
//
#define SPI_RX_BUF_LEN 0x200
#define SPI_TX_BUF_LEN 0x200

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/
//
// SPI Rx ring buffer.
static rb_t g_spi_rx_rbd;
static u8 g_spi_rx_rbmem[SPI_RX_BUF_LEN];

// SPI Tx ring buffer.
static rb_t g_spi_tx_rbd;
static u8 g_spi_tx_rbmem[SPI_TX_BUF_LEN];

static u8 g_cpu_spi_rx_byte;
static u8 g_cpu_spi_tx_byte;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static bool _cpu_spi_tx_dequeue(u8 *spi_byte);
static bool _cpu_spi_rx_enqueue(u8 *spi_byte);

static void _cpu_spi_trx_byte(u8 *tx_byte, u8 *rx_byte);

static void _cpu_spi_trx_callback(void);

/*----- Extern function implementations ------------------------------*/

void dev_cpu_spi_init(void)
{
	// Initialise MCU message ring buffers.
	int rb_init_status;
	rb_init_status  = ring_buffer_init(&g_spi_tx_rbd, g_spi_tx_rbmem, sizeof(g_spi_tx_rbmem[0]), ARRAY_SIZE(g_spi_tx_rbmem));
	rb_init_status |= ring_buffer_init(&g_spi_rx_rbd, g_spi_rx_rbmem, sizeof(g_spi_rx_rbmem[0]), ARRAY_SIZE(g_spi_rx_rbmem));

	if (RING_BUFFER_OK != rb_init_status) {
		PANIC(PANIC_GENERIC);
		return;
	}

	per_spi_init();

	per_spi_register_callback(EVT_SPI_TRX_COMPLETE, _cpu_spi_trx_callback);

	// Initialise transfer.
	_cpu_spi_trx_byte(&g_cpu_spi_tx_byte, &g_cpu_spi_rx_byte);
}

bool dev_cpu_spi_tx_enqueue(u8 *spi_byte)
{
	// Overwrite on overflow?
	int status = ring_buffer_put_overwrite(&g_spi_tx_rbd, spi_byte, NULL);
	return status == RING_BUFFER_OK ? true : false;
}

bool dev_cpu_spi_rx_dequeue(u8 *spi_byte)
{
	return RING_BUFFER_OK == ring_buffer_get(&g_spi_rx_rbd, spi_byte);
}

/*----- Static function implementations ------------------------------*/

static bool _cpu_spi_tx_dequeue(u8 *spi_byte)
{
	return RING_BUFFER_OK == ring_buffer_get(&g_spi_tx_rbd, spi_byte);
}

static bool _cpu_spi_rx_enqueue(u8 *spi_byte)
{
	// Overwrite on overflow?
	int status = ring_buffer_put_overwrite(&g_spi_rx_rbd, spi_byte, NULL);
	return status == RING_BUFFER_OK ? true : false;
}

static void _cpu_spi_trx_byte(u8 *tx_byte, u8 *rx_byte)
{
	per_spi_trx_int(tx_byte, rx_byte, 1);
}

static void _cpu_spi_trx_callback(void)
{
	_cpu_spi_rx_enqueue(&g_cpu_spi_rx_byte);

	// Transmit 0 if tx queue empty.
	if (!_cpu_spi_tx_dequeue(&g_cpu_spi_tx_byte)) {
		g_cpu_spi_tx_byte = 0;
	}

	// CPU leads, so always re-enable transfer to catch next byte.
	_cpu_spi_trx_byte(&g_cpu_spi_tx_byte, &g_cpu_spi_rx_byte);
}

/*----- End of file --------------------------------------------------*/
