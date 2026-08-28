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
 * @file per_uart.c
 *
 * @brief  Configuration and handling for UART peripherals.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <soc_AM1808.h>
#include <csl_interrupt.h>
#include <hw_types.h>
#include <hw_uart.h>

#include "per_uart.h"

/*----- Macros -------------------------------------------------------*/

#define UART_INSTANCES 2
#define UART_MODULE_FREQ 150000000
#define UART_TX_FIFO_LENGTH 16

#define UART_RX_TRIG_LEVEL_1 (UART_FCR_RXFIFTL_CHAR1 << UART_FCR_RXFIFTL_SHIFT)
#define UART_FIFO_MODE UART_FCR_FIFOEN
#define UART_RX_CLEAR UART_FCR_RXCLR
#define UART_TX_CLEAR UART_FCR_TXCLR

#define UART_THR_TSR_EMPTY UART_LSR_TEMT
#define UART_THR_EMPTY UART_LSR_THRE
#define UART_BREAK_IND UART_LSR_BI
#define UART_FRAME_ERROR UART_LSR_FE
#define UART_PARITY_ERROR UART_LSR_PE
#define UART_OVERRUN_ERROR UART_LSR_OE
#define UART_DATA_READY UART_LSR_DR

#define UART_INT_MODEM_STAT UART_IER_EDSSI
#define UART_INT_LINE_STAT UART_IER_ELSI
#define UART_INT_TX_EMPTY UART_IER_ETBEI
#define UART_INT_RXDATA_CTI UART_IER_ERBI
#define UART_INT_MASK                                                          \
	(UART_INT_MODEM_STAT | UART_INT_LINE_STAT | UART_INT_TX_EMPTY |            \
	 UART_INT_RXDATA_CTI)

#define UART_INTID_TX_EMPTY UART_IIR_INTID_THRE
#define UART_INTID_RX_DATA UART_IIR_INTID_RDA
#define UART_INTID_RX_LINE_STAT UART_IIR_INTID_RLS
#define UART_INTID_CTI UART_IIR_INTID_CTI

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	u32 system_int;
	u32 address;

	u8 *tx_buffer;
	u32 tx_length;

	u8 *rx_buffer;
	u32 rx_length;

	void (*tx_callback)(void);
	void (*rx_callback)(void);
	void (*error_callback)(void);

} t_uart;

/*----- Static function prototypes -----------------------------------*/

static t_uart *_uart_get(u8 instance);
static void _uart_reset(t_uart *uart, u8 instance);
static void _uart_configure_line(t_uart *uart, const t_uart_config *config);
static u32 _uart_word_length(u8 word_length);
static void _uart_configure_fifo(t_uart *uart, const t_uart_config *config);
static void _uart_configure_interrupts(t_uart *uart,
									   const t_uart_config *config);
static void _uart_configure_baud_line(u32 base_addr, u32 uart_clk,
									  u32 baud_rate, u32 line_config,
									  u32 oversample);
static void _uart_enable(u32 base_addr);
static void _uart_disable(u32 base_addr);
static void _uart_enable_fifo(u32 base_addr);
static void _uart_set_fifo_rx_level(u32 base_addr, u32 rx_level);
static void _uart_enable_interrupts(u32 base_addr, u32 int_flags);
static void _uart_disable_interrupts(u32 base_addr, u32 int_flags);
static u8 _uart_get_interrupt_id(u32 base_addr);
static u32 _uart_rx_errors(u32 base_addr);
static void _uart_put_char(u32 base_addr, u8 byte);
static int _uart_get_char(u32 base_addr);
static int _uart_get_char_nonblocking(u32 base_addr);
static void _uart_wait_for_empty(t_uart *uart);
static void _uart_start_tx(t_uart *uart, u8 *buffer, u32 length);
static void _uart_start_rx(t_uart *uart, u8 *buffer, u32 length);
static void _uart_ack_system_interrupt(t_uart *uart);
static void _uart_handle_tx_empty(t_uart *uart);
static void _uart_handle_rx_data(t_uart *uart);
static void _uart_handle_rx_error(t_uart *uart);
static void _uart_dispatch_interrupt(t_uart *uart, u8 int_id);
static void _uart_isr(t_uart *uart);
static void _uart0_isr(void);
static void _uart1_isr(void);

/*----- Static variable definitions ----------------------------------*/

static const u32 g_base_address[UART_INSTANCES] = {SOC_UART_0_REGS,
														SOC_UART_1_REGS};

static const u32 g_system_interrupt[UART_INSTANCES] = {SYS_INT_UARTINT0,
															SYS_INT_UARTINT1};

static const void *g_isr_address[UART_INSTANCES] = {&_uart0_isr, &_uart1_isr};

static t_uart g_uart[UART_INSTANCES];

/*----- Extern variable definitions ----------------------------------*/

/*----- Extern function implementations ------------------------------*/

void per_uart_init(t_uart_config *config) {

	t_uart *uart = _uart_get(config->instance);

	_uart_reset(uart, config->instance);
	_uart_configure_line(uart, config);
	_uart_configure_fifo(uart, config);
	_uart_configure_interrupts(uart, config);

	_uart_enable(uart->address);
}

void per_uart_terminate(u8 instance) {

	t_uart *uart = _uart_get(instance);

	_uart_wait_for_empty(uart);
	_uart_disable(uart->address);
}

void per_uart_transmit(u8 instance, u8 *buffer, u32 length) {

	if (buffer != NULL) {
		while (length--) {
			_uart_put_char(g_uart[instance].address, *buffer++);
		}
	}
}

void per_uart_receive(u8 instance, u8 *buffer, u32 length) {

	if (buffer != NULL) {
		while (length--) {
			*buffer++ = _uart_get_char(g_uart[instance].address);
		}
	}
}

void per_uart_transmit_int(u8 instance, u8 *buffer, u32 length) {

	_uart_start_tx(_uart_get(instance), buffer, length);
}

void per_uart_receive_int(u8 instance, u8 *buffer, u32 length) {

	_uart_start_rx(_uart_get(instance), buffer, length);
}

void per_uart_register_callback(u8 instance, t_uart_event event,
								void (*callback)(void)) {

	switch (event) {
	case UART_TX_COMPLETE:
		g_uart[instance].tx_callback = callback;
		break;

	case UART_RX_COMPLETE:
		g_uart[instance].rx_callback = callback;
		break;

	case UART_RX_ERROR:
		g_uart[instance].error_callback = callback;
		break;

	default:
		break;
	}
}

/*----- Static function implementations ------------------------------*/

static t_uart *_uart_get(u8 instance) {

	return &g_uart[instance];
}

static void _uart_reset(t_uart *uart, u8 instance) {

	uart->address = g_base_address[instance];
	uart->system_int = g_system_interrupt[instance];
	uart->tx_buffer = NULL;
	uart->tx_length = 0;
	uart->rx_buffer = NULL;
	uart->rx_length = 0;
	uart->tx_callback = NULL;
	uart->rx_callback = NULL;
	uart->error_callback = NULL;
}

static void _uart_configure_line(t_uart *uart, const t_uart_config *config) {

	_uart_configure_baud_line(uart->address, UART_MODULE_FREQ, config->baud,
							  _uart_word_length(config->word_length),
							  config->oversample);
}

static u32 _uart_word_length(u8 word_length) {

	switch (word_length) {
	case 5:
		return UART_LCR_WLS_5BITS;

	case 6:
		return UART_LCR_WLS_6BITS;

	case 7:
		return UART_LCR_WLS_7BITS;

	case 8:
	default:
		return UART_LCR_WLS_8BITS;
	}
}

static void _uart_configure_fifo(t_uart *uart, const t_uart_config *config) {

	/// TODO: This only supports 1 byte Rx FIFO.
	//
	if (!config->fifo_enable) {
		return;
	}

	_uart_enable_fifo(uart->address);
	_uart_set_fifo_rx_level(uart->address, UART_RX_TRIG_LEVEL_1);
}

static void _uart_configure_interrupts(t_uart *uart,
									   const t_uart_config *config) {

	if (!config->int_enable) {
		return;
	}

	// Set interrupt channel.
	IntChannelSet(uart->system_int, config->int_channel);

	// Registers ISR in Interrupt Vector Table.
	IntRegister(uart->system_int, g_isr_address[config->instance]);

	// Enable system interrupt in AINTC.
	IntSystemEnable(uart->system_int);
}

static void _uart_configure_baud_line(u32 base_addr, u32 uart_clk,
									  u32 baud_rate, u32 line_config,
									  u32 oversample) {

	u32 divisor;

	if (oversample == OVERSAMPLE_13) {
		divisor = uart_clk / (baud_rate * 13);
		HWREG(base_addr + UART_MDR) = UART_MDR_OSM_SEL;
	} else {
		divisor = uart_clk / (baud_rate * 16);
		HWREG(base_addr + UART_MDR) &= ~UART_MDR_OSM_SEL;
	}

	HWREG(base_addr + UART_DLL) = divisor & 0xFF;
	HWREG(base_addr + UART_DLH) = (divisor & 0xFF00) >> 8;
	HWREG(base_addr + UART_LCR) = line_config & UART_LCR_WLS;
}

static void _uart_enable(u32 base_addr) {

	HWREG(base_addr + UART_PWREMU_MGMT) =
		UART_PWREMU_MGMT_URRST | UART_PWREMU_MGMT_UTRST;
}

static void _uart_disable(u32 base_addr) {

	HWREG(base_addr + UART_PWREMU_MGMT) &=
		~(UART_PWREMU_MGMT_FREE | UART_PWREMU_MGMT_URRST |
		  UART_PWREMU_MGMT_UTRST);
}

static void _uart_enable_fifo(u32 base_addr) {

	HWREG(base_addr + UART_FCR) = UART_FIFO_MODE | UART_RX_CLEAR | UART_TX_CLEAR;
}

static void _uart_set_fifo_rx_level(u32 base_addr, u32 rx_level) {

	HWREG(base_addr + UART_FCR) = (rx_level & UART_FCR_RXFIFTL) | UART_FIFO_MODE;
}

static void _uart_enable_interrupts(u32 base_addr, u32 int_flags) {

	HWREG(base_addr + UART_IER) |= int_flags & UART_INT_MASK;
}

static void _uart_disable_interrupts(u32 base_addr, u32 int_flags) {

	HWREG(base_addr + UART_IER) &= ~(int_flags & UART_INT_MASK);
}

static u8 _uart_get_interrupt_id(u32 base_addr) {

	return (HWREG(base_addr + UART_IIR) & UART_IIR_INTID) >>
		   UART_IIR_INTID_SHIFT;
}

static u32 _uart_rx_errors(u32 base_addr) {

	return HWREG(base_addr + UART_LSR) &
		   (UART_OVERRUN_ERROR | UART_PARITY_ERROR | UART_FRAME_ERROR |
			UART_BREAK_IND);
}

static void _uart_put_char(u32 base_addr, u8 byte) {

	const u32 tx_empty = UART_THR_TSR_EMPTY | UART_THR_EMPTY;

	while (tx_empty != (HWREG(base_addr + UART_LSR) & tx_empty))
		;

	HWREG(base_addr + UART_THR) = byte;
}

static int _uart_get_char(u32 base_addr) {

	while ((HWREG(base_addr + UART_LSR) & UART_DATA_READY) == 0)
		;

	return (int)HWREG(base_addr + UART_RBR);
}

static int _uart_get_char_nonblocking(u32 base_addr) {

	if (HWREG(base_addr + UART_LSR) & UART_DATA_READY) {
		return (int)HWREG(base_addr + UART_RBR);
	}

	return -1;
}

static void _uart_wait_for_empty(t_uart *uart) {

	while ((UART_THR_TSR_EMPTY | UART_THR_EMPTY) !=
		   (HWREG(uart->address + UART_LSR) &
			(UART_THR_TSR_EMPTY | UART_THR_EMPTY)))
		;
}

static void _uart_start_tx(t_uart *uart, u8 *buffer, u32 length) {

	if ((buffer == NULL) || (length == 0)) {
		return;
	}

	uart->tx_buffer = buffer;
	uart->tx_length = length;

	_uart_enable_interrupts(uart->address, UART_INT_TX_EMPTY);
}

static void _uart_start_rx(t_uart *uart, u8 *buffer, u32 length) {

	if ((buffer == NULL) || (length == 0)) {
		return;
	}

	uart->rx_buffer = buffer;
	uart->rx_length = length;

	_uart_enable_interrupts(uart->address,
							UART_INT_RXDATA_CTI | UART_INT_LINE_STAT);
}

static void _uart_ack_system_interrupt(t_uart *uart) {

#if NESTED_INTERRUPTS
	// System interrupt already cleared in IRQHandler.
#else
	IntSystemStatusClear(uart->system_int);
#endif
}

static void _uart_handle_tx_empty(t_uart *uart) {

	u8 tx_fifo_level = 0;

	while ((uart->tx_length > 0) && (tx_fifo_level < UART_TX_FIFO_LENGTH)) {
		_uart_put_char(uart->address, *uart->tx_buffer++);
		uart->tx_length--;
		tx_fifo_level++;
	}

	if (uart->tx_length != 0) {
		return;
	}

	_uart_disable_interrupts(uart->address, UART_INT_TX_EMPTY);

	if (uart->tx_callback != NULL) {
		uart->tx_callback();
	}
}

static void _uart_handle_rx_data(t_uart *uart) {

	if (uart->rx_length == 0) {
		return;
	}

	*uart->rx_buffer++ = _uart_get_char(uart->address);
	uart->rx_length--;

	if (uart->rx_length != 0) {
		return;
	}

	_uart_disable_interrupts(uart->address, UART_INT_RXDATA_CTI);

	if (uart->rx_callback != NULL) {
		uart->rx_callback();
	}
}

static void _uart_handle_rx_error(t_uart *uart) {

	while (_uart_rx_errors(uart->address)) {
		_uart_get_char_nonblocking(uart->address);
	}

	_uart_disable_interrupts(uart->address,
							 UART_INT_RXDATA_CTI | UART_INT_LINE_STAT);

	uart->rx_buffer = NULL;
	uart->rx_length = 0;

	// Drop all pending RX bytes; the 5-byte panel framing is no longer trusted.
	HWREG(uart->address + UART_FCR) =
		UART_FIFO_MODE | UART_RX_CLEAR | UART_RX_TRIG_LEVEL_1;

	if (uart->error_callback != NULL) {
		uart->error_callback();
	}
}

static void _uart_dispatch_interrupt(t_uart *uart, u8 int_id) {

	switch (int_id) {
	case UART_INTID_TX_EMPTY:
		_uart_handle_tx_empty(uart);
		break;

	case UART_INTID_CTI:
		/// TODO: Log character timeout.
		//
		// No break, fall through Rx case.

	case UART_INTID_RX_DATA:
		_uart_handle_rx_data(uart);
		break;

	case UART_INTID_RX_LINE_STAT:
		_uart_handle_rx_error(uart);
		break;

	default:
		break;
	}
}

/// TODO: Implement DMA and higher level MCU device driver.
//          5 byte message buffer.
//              DMA continually services UART.
//                  DMA Complete interrupt pushes/pops ring buffer.

/*
 * @brief   Interrupt Service Routine for UART0.
 *          - Transmit buffer empty:
 *              - Read from Tx ring buffer and writes to UART.
 *          - Receive buffer full:
 *              - Read from UART and write to Rx ring buffer.
 *          - Receiver line error:
 *              - Clear byte from RBR if receiver line error has occured.
 */
static inline void _uart_isr(t_uart *uart) {

	_uart_ack_system_interrupt(uart);

	// Clear all pending interrupts.
	u8 int_id;
	while ((int_id = _uart_get_interrupt_id(uart->address))) {
		_uart_dispatch_interrupt(uart, int_id);
	}

	return;
}

static void _uart0_isr(void) { _uart_isr(&g_uart[0]); }

static void _uart1_isr(void) { _uart_isr(&g_uart[1]); }

/*----- End of file --------------------------------------------------*/
