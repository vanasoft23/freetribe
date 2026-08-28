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
 * @file    rsp_session.c
 * 
 * @brief   GDB Remote Serial Protocol packet framer.
 *
 * @details 
 * RSP session frames and parses GDB RSP packets over the TinyUSB transport.
 * 
 * Read the Remote Serial Protocol documentation:
 * <a href="https://sourceware.org/gdb/current/onlinedocs/gdb.html/Remote-Protocol.html">
 *  Sourceware GDB RSP documentation
 * </a>
 * <a href="https://developer.apple.com/library/archive/documentation/DeveloperTools/gdb/gdb/gdb_33.html">
 *  Apple GDB RSP documentation
 * </a>
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "str_misc.h"

#include <string.h>

#include "rsp_session.h"

/*----- Macros -------------------------------------------------------*/

#define RSP_TX_CAPACITY ((RSP_PACKET_CAPACITY * 2u) + 5u)

/*----- Types --------------------------------------------------------*/

typedef enum {
	RSP_RX_STATE_WAIT_START,
	RSP_RX_STATE_PAYLOAD,
	RSP_RX_STATE_CHECKSUM_HIGH,
	RSP_RX_STATE_CHECKSUM_LOW,
} rsp_rx_state_t;

typedef struct {
	u8  data[RSP_PACKET_CAPACITY];
	u32 len;
} rsp_packet_t;

typedef struct {
	rsp_rx_state_t state;
	bool           escaped;
	u8             checksum;
	u8             expected_checksum;
	rsp_packet_t   packet;
} rsp_rx_t;

typedef struct {
	u8   data[RSP_TX_CAPACITY];
	u32  len;
	u32  processed;
	u32  retry_offset;
	bool awaiting_ack; // whether still waiting for ACK before we can start a new transfer
} rsp_tx_t;

struct rsp_session {
	rsp_rx_t rx;
	rsp_tx_t tx;
	bool     no_ack;
};

/*----- Globals ------------------------------------------------------*/

rsp_session_t g_rsp_session;

/*----- Static function prototypes -----------------------------------*/

static void _rx_reset(rsp_session_t *rsp);
static void _parse_payload(rsp_session_t *rsp, u8 byte);
static void _parse_checksum_high(rsp_session_t *rsp, u8 byte);
static bool _parse_checksum_low(rsp_session_t *rsp, u8 byte);

static void _queue_ack(rsp_session_t *rsp);
static void _queue_nack(rsp_session_t *rsp);
static bool _tx_append(rsp_tx_t *tx, u8 byte);
static bool _tx_append_escaped(rsp_tx_t *tx, u8 byte, u8 *checksum);
static bool _tx_accept_ack(rsp_tx_t *tx);

/*----- Extern function implementations ------------------------------*/

//
// RX
//

/**
 * @brief   Advance the RSP state machine for each byte in the buffer.
 * 
 * When halted, this receives requests from the GDB client.
 * 
 * @param[in] rsp     RSP session handle
 * @param[in] byte    Byte received from USB CDC input stream
 * 
 * @returns rsp_rx_event_t struct results:
 *            RSP_RX_EVENT_NONE    Nothing meaningful happened
 *            RSP_RX_EVENT_PACKET  Whole packet parsed successfully
 *            RSP_RX_EVENT_ACK     GDB successfully received our reply
 *            RSP_RX_EVENT_NACK    GDB failed to receive our reply:
 *                                  we have to send it again.
 */
rsp_rx_event_t rsp_rx_feed_byte(rsp_session_t *rsp, u8 byte) {

	switch (rsp->rx.state) {

	case RSP_RX_STATE_WAIT_START:
		 if (byte == '$') {
			 _rx_reset(rsp);
			 rsp->rx.state = RSP_RX_STATE_PAYLOAD;
		 }

		 if (byte == '+') {
			 return _tx_accept_ack(&rsp->tx)
				 ? RSP_RX_EVENT_ACK
				 : RSP_RX_EVENT_NONE;
		 }

		 if (byte == '-') {
			 return rsp->tx.awaiting_ack
				 ? RSP_RX_EVENT_NACK
				 : RSP_RX_EVENT_NONE;
		 }
		 break;

	case RSP_RX_STATE_PAYLOAD:
		 _parse_payload(rsp, byte);
		 break;

	case RSP_RX_STATE_CHECKSUM_HIGH:
		 _parse_checksum_high(rsp, byte);
		 break;

	case RSP_RX_STATE_CHECKSUM_LOW:
		 return _parse_checksum_low(rsp, byte)
			 ? RSP_RX_EVENT_PACKET
			 : RSP_RX_EVENT_NONE;

	default: break;
	}

	return RSP_RX_EVENT_NONE;
}

rsp_packet_view_t rsp_rx_packet_view(const rsp_session_t *rsp) {

	rsp_packet_view_t view = {
		.data = rsp->rx.packet.data,
		.len  = rsp->rx.packet.len,
	};
	return view;
}

//
// TX
//

const u8 *rsp_tx_peek(rsp_session_t *rsp, u32 *len) {
	rsp_tx_t *tx = &rsp->tx;

	*len = tx->len - tx->processed;
	return tx->data + tx->processed;
}

void rsp_tx_discard(rsp_session_t *rsp, u32 len) {
	rsp_tx_t *tx = &rsp->tx;

	u32 pending = tx->len - tx->processed;
	if (len > pending)
		len = pending;

	tx->processed += len;

	if ((tx->processed == tx->len) && !tx->awaiting_ack) {
		tx->len       = 0;
		tx->processed = 0;
	}
}

/**
 * @returns true on success, false on failure
 */
bool rsp_tx_commit(rsp_session_t *rsp, const u8 *data, u32 payload_len, bool enter_no_ack) {
	rsp_tx_t *tx = &rsp->tx;
	u32 start = tx->len;

	if (tx->awaiting_ack)
		return false;

	// Enforce the advertised maximum unescaped payload size
	if (payload_len > RSP_PACKET_CAPACITY)
		return false;

	// Packet start char
	if (!_tx_append(tx, '$'))
		return false;

	// Payload
	u8 checksum = 0;
	for (u32 i = 0; i < payload_len; i++) {
		if (!_tx_append_escaped(tx, data[i], &checksum)) {
			tx->len = start; // discard appended tx buffer bytes
			return false;
		}
	}

	// Checksum
	if (!_tx_append(tx, '#') ||
	    !_tx_append(tx, hex_digit(checksum >> 4)) ||
	    !_tx_append(tx, hex_digit(checksum)))\
	{
		tx->len = start; // discard appended tx buffer bytes
		return false;
	}

	if (enter_no_ack) {
		rsp->no_ack = true;
	} else if (!rsp->no_ack) {
		tx->retry_offset = start;
		tx->awaiting_ack = true;
	}

	return true;
}

bool rsp_tx_retransmit_last(rsp_session_t *rsp)
{
	rsp_tx_t *tx = &rsp->tx;

	if (!tx->awaiting_ack || (tx->processed != tx->len)) {
		return false;
	}

	tx->processed = tx->retry_offset;
	return true;
}

bool rsp_awaiting_ack(rsp_session_t *session)
{
	return session->tx.awaiting_ack;
}

//
// Session / misc
//

void rsp_session_reset(rsp_session_t *rsp)
{
	
	memset(rsp, 0, sizeof(*rsp));
}

bool rsp_expect_escaped(rsp_session_t *rsp)
{
	return rsp->rx.escaped;
}

/*----- RX state handling --------------------------------------------*/

static void _rx_reset(rsp_session_t *rsp)
{

	memset(&rsp->rx, 0, sizeof(rsp->rx));
	// state machine automagically goes to RSP_RX_STATE_WAIT_START

}


static void _parse_payload(rsp_session_t *rsp, u8 byte)
{

	rsp_rx_t *rx = &rsp->rx;

	if (rx->escaped) {
		rx->checksum += byte;
		byte ^= 0x20;
		rx->escaped = false;

	} else {
		if (byte == '#') {
			rx->state = RSP_RX_STATE_CHECKSUM_HIGH;
			return;
		}

		rx->checksum += byte;

		if (byte == '}') {
			rx->escaped = true;
			return;
		}
	}

	if (rx->packet.len >= sizeof(rx->packet.data)) {
		_queue_nack(rsp);
		_rx_reset(rsp);
		return;
	}

	rx->packet.data[rx->packet.len++] = byte;
}

static void _parse_checksum_high(rsp_session_t *rsp, u8 byte)
{

	int value = hex_nibble(byte);
	if (value < 0) {
		_queue_nack(rsp);
		_rx_reset(rsp);
		return;
	}
	rsp->rx.expected_checksum = (u8)(value << 4);

	rsp->rx.state = RSP_RX_STATE_CHECKSUM_LOW;
}

/**
 * @returns true if packet was successfully parsed.
 */
static bool _parse_checksum_low(rsp_session_t *rsp, u8 byte)
{

	rsp_rx_t *rx = &rsp->rx;

	int nibble = hex_nibble(byte);
	if (nibble < 0)
		goto reject_packet;

	rx->expected_checksum |= (u8)nibble;

	if (rx->checksum != rx->expected_checksum)
		goto reject_packet;

	if (!rsp->no_ack)
		_queue_ack(rsp);
	rx->state = RSP_RX_STATE_WAIT_START;
	return true;

reject_packet:
	if (!rsp->no_ack)
		_queue_nack(rsp);
	_rx_reset(rsp);
	return false;
}


/*----- TX -----------------------------------------------------------*/

/**
 * @brief   Our packet successfully transmitted.
 * 
 * Invoked when server sent '+' (ACK) between packets.
 * 
 * @param[in] rsp   RSP session handle
 */
static void _queue_ack(rsp_session_t *rsp)
{
	(void)_tx_append(&rsp->tx, '+');
}

/**
 * @brief   Our packet failed transmission.
 * 
 * Invoked when server sent '-' (NACK) between packets.
 * 
 * @param[in] rsp   RSP session handle
 */
static void _queue_nack(rsp_session_t *rsp)
{
	(void)_tx_append(&rsp->tx, '-');
}

static bool _tx_append(rsp_tx_t *tx, u8 byte)
{

	if (tx->len >= sizeof(tx->data))
		return false;

	tx->data[tx->len++] = byte;
	return true;
}

static bool _tx_append_escaped(rsp_tx_t *tx, u8 byte, u8 *checksum)
{

	if (byte == '$' || byte == '#' || byte == '}' || byte == '*') {
		if (!_tx_append(tx, '}'))
			return false;

		*checksum += '}';
		byte ^= 0x20u;
	}

	if (!_tx_append(tx, byte))
		return false;

	*checksum += byte;
	return true;
}

static bool _tx_accept_ack(rsp_tx_t *tx)
{
	if (!tx->awaiting_ack) {
		return false;
	}

	tx->len          = 0;
	tx->processed    = 0;
	tx->retry_offset = 0;
	tx->awaiting_ack = false;
	return true;
}


/*----- End of file --------------------------------------------------*/
