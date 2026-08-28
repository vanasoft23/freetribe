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
 * @file    dev_mcu.c
 *
 * @brief   Device driver for communicating with panel board MCU.
 */

/// TODO: Support MCU flash modes.
///       Re-initialise with different ring buffers and callbacks.

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "ring_buffer.h"

#include "per_uart.h"

#include "dev_mcu.h"

/*----- Macros -------------------------------------------------------*/

/// TODO: Centralised header for interrupt priorities and queue sizes.

#define MCU_UART UART_0

#define MCU_UART_INT_CHANNEL 7

#define MCU_TX_BUF_LEN 0x200
#define MCU_RX_BUF_LEN 0x200
#define MCU_MSG_LEN 0x5

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

// MCU RX ring buffer.
static rb_t mcu_rx_rbd;
static u8 mcu_rx_rbmem[MCU_RX_BUF_LEN][MCU_MSG_LEN];

// MCU TX ring buffer.
static rb_t mcu_tx_rbd;
static u8 mcu_tx_rbmem[MCU_TX_BUF_LEN][MCU_MSG_LEN];

static u8 g_mcu_rx_msg[MCU_MSG_LEN];

static bool g_mcu_tx_complete = true;

static void (*p_mcu_rx_callback)(void) = NULL;

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static int _mcu_tx_dequeue(u8 *mcu_msg);
static void _mcu_rx_enqueue(u8 *mcu_msg);

static void _mcu_tx_msg(u8 *mcu_msg);
static void _mcu_rx_msg(void);

static void _mcu_tx_callback(void);
static void _mcu_rx_callback(void);
static void _mcu_error_callback(void);

/*----- Extern function implementations ------------------------------*/

/// TODO: Return status code.
//
void dev_mcu_init(void) {

	static t_uart_config uart_cfg = {.instance = MCU_UART,
									 .baud = 115200,
									 .word_length = 8,
									 .int_enable = true,
									 .int_channel = MCU_UART_INT_CHANNEL,
									 .fifo_enable = true,
									 .oversample = OVERSAMPLE_13};
	// Initialise MCU message ring buffers.
	if (ring_buffer_init(&mcu_tx_rbd, mcu_tx_rbmem, sizeof(mcu_tx_rbmem[0]), ARRAY_SIZE(mcu_tx_rbmem)) == RING_BUFFER_OK &&
		ring_buffer_init(&mcu_rx_rbd, mcu_rx_rbmem, sizeof(mcu_rx_rbmem[0]), ARRAY_SIZE(mcu_rx_rbmem)) == RING_BUFFER_OK) {

		// Initialise UART.
		per_uart_init(&uart_cfg);

		// Register Tx callback.
		per_uart_register_callback(MCU_UART, UART_TX_COMPLETE, _mcu_tx_callback);

		// Register Rx callback.
		per_uart_register_callback(MCU_UART, UART_RX_COMPLETE, _mcu_rx_callback);

		// Register error callback.
		per_uart_register_callback(MCU_UART, UART_RX_ERROR, _mcu_error_callback);

		g_mcu_tx_complete = true;

		// Enable Rx callback.
		_mcu_rx_msg();
	}
}

void dev_mcu_tx_enqueue(u8 *mcu_msg) {

	// Overwrite on overflow.
	ring_buffer_put_overwrite(&mcu_tx_rbd, mcu_msg, NULL);

	if (g_mcu_tx_complete) {

		// Start transmission.
		_mcu_tx_callback();
	}
}

bool dev_mcu_rx_dequeue(u8 *mcu_msg) {

	return RING_BUFFER_OK == ring_buffer_get(&mcu_rx_rbd, mcu_msg);
}

void dev_mcu_register_rx_callback(void (*callback)(void)) {

	p_mcu_rx_callback = callback;
}

/*----- Static function implementations ------------------------------*/

static int _mcu_tx_dequeue(u8 *mcu_msg) {

	return ring_buffer_get(&mcu_tx_rbd, mcu_msg);
}

static void _mcu_rx_enqueue(u8 *mcu_msg) {

	// Overwrite on overflow.
	ring_buffer_put_overwrite(&mcu_rx_rbd, mcu_msg, NULL);

	if (p_mcu_rx_callback != NULL) {
		p_mcu_rx_callback();
	}
}

static void _mcu_tx_msg(u8 *mcu_msg) {

	g_mcu_tx_complete = false;
	per_uart_transmit_int(MCU_UART, mcu_msg, MCU_MSG_LEN);
}

static void _mcu_rx_msg(void) {
	//
	per_uart_receive_int(MCU_UART, g_mcu_rx_msg, MCU_MSG_LEN);
}

static void _mcu_tx_callback(void) {
	//
	static u8 mcu_msg[MCU_MSG_LEN];

	// Send next queued message.
	if (_mcu_tx_dequeue(mcu_msg) == 0) {
		_mcu_tx_msg(mcu_msg);

	} else {
		g_mcu_tx_complete = true;
	}
}

static void _mcu_rx_callback(void) {

	_mcu_rx_enqueue(g_mcu_rx_msg);

	_mcu_rx_msg();
}

static void _mcu_error_callback(void) {

	_mcu_rx_msg(); // re-arm interrupt

}

/*----- End of file --------------------------------------------------*/
