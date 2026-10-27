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
 * @file    gdb_stub.c
 *
 * @brief   Composes the RSP session and GDB command server.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"
#include "ring_buffer.h"

#include "str_misc.h"
#include "rsp_session.h"
#include "gdb_server.h"
#include "gdb_monitor.h"
#include <stdint.h>
#include "gdb_stub.h"

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
	GDB_ACTION_IDLE,
	GDB_ACTION_WAIT_REPLY_QUEUE,
	GDB_ACTION_WAIT_REPLY_ACK,
	GDB_ACTION_READY,
} gdb_action_state_t;

typedef struct {
	gdb_action_state_t   state;
	gdb_command_result_t command;
	u32                  reply_len;
} gdb_pending_action_t;

/*----- Static variables ---------------------------------------------*/

/*
 * Console output buffer
 */
#define CONSOLE_RB_CAP 2048u
#define CONSOLE_PACKET_BYTES CONSOLE_RB_CAP

static rb_t s_conout_rb;
static u8   s_conout_storage[CONSOLE_RB_CAP];
static u8   s_conout_packet[1u + 2u * CONSOLE_PACKET_BYTES];

static u8                            s_reply_data[RSP_PACKET_CAPACITY];
static bool                          s_stop_reply_pending;
static volatile gdb_pending_action_t s_pending;

/*----- Static function prototypes -----------------------------------*/

static void _handle_packet(void);
static void _service_pending_action(void);
static void _apply_action(void);
static void _service_stop_reply(void);
static void _service_console_output(void);
static bool _try_queue_stop_reply(void);

/*----- Extern function implementations ------------------------------*/

void gdb_stub_reset(void)
{
	s_stop_reply_pending = false;
	s_pending = (gdb_pending_action_t){ .state = GDB_ACTION_IDLE };
	rsp_session_reset(&g_rsp_session);
	gdb_monitor_reset();
	
	rb_err_t err = ring_buffer_init(
		&s_conout_rb,
		s_conout_storage,
		sizeof(u8),
		CONSOLE_RB_CAP
	);
	ASSERT(RING_BUFFER_OK == err);
	(void)err;
}

/**
 * @brief   Parse and handle a chunk of bytes received over USB CDC.
 * 
 * @param[in]  data   Raw incoming buffer of USB bytes
 * @param[in]  len    Number of incoming USB bytes
 */
u32 gdb_stub_rx(const u8 *data, u32 len)
{
	for (u32 i = 0; i < len; i++) {
		/* Retain later packets in the transport, but still accept ACK/NACK. */
		if (s_pending.state != GDB_ACTION_IDLE && data[i] == '$')
			return i;

		rsp_rx_event_t event = rsp_rx_feed_byte(&g_rsp_session, data[i]);
		
		switch (event) {
		case RSP_RX_EVENT_NONE:
			 continue;
		
		case RSP_RX_EVENT_ACK:
			 if (s_pending.state == GDB_ACTION_WAIT_REPLY_ACK) {
				 s_pending.state = GDB_ACTION_READY;
				 _apply_action();
			 }
			 // This ACK may instead free space for a retained reply.
			 _service_pending_action();
			 continue;
		
		case RSP_RX_EVENT_NACK:
			 (void)rsp_tx_retransmit_last(&g_rsp_session);
			 continue;
			 
		case RSP_RX_EVENT_PACKET:
			 _handle_packet();
			 break;
		}
	}
	return len;
}

/**
 * @brief   Get queued GDB RSP response.
 */
const u8 *gdb_stub_tx_peek(u32 *len)
{
	return rsp_tx_peek(&g_rsp_session, len);
}

void gdb_stub_tx_discard(u32 len)
{
	rsp_tx_discard(&g_rsp_session, len);
	_service_pending_action();
}


/*--------------------------------------------------------------------*/

void gdb_stub_service(void)
{
	_service_pending_action();
	if (s_pending.state != GDB_ACTION_IDLE)
		return;

	_service_stop_reply();
	_service_console_output();
}

static void _service_stop_reply(void)
{
	if (!s_stop_reply_pending)
		return;

	if (!gdb_monitor_frame_ready())
		return;

	if (rsp_awaiting_ack(&g_rsp_session))
		return;
	
	if (_try_queue_stop_reply()) {
		s_stop_reply_pending = false;
	}
}

/*--------------------------------------------------------------------*/

static void _service_console_output(void)
{
	if (rsp_awaiting_ack(&g_rsp_session))
		return;

	u32 len = (u32)ring_buffer_count(&s_conout_rb);
	if (len == 0u)
		return;
	if (len > CONSOLE_PACKET_BYTES)
		len = CONSOLE_PACKET_BYTES;

	s_conout_packet[0] = 'O';
	for (u32 i = 0; i < len; ++i) {
		u8 c;
		if (ring_buffer_peek(&s_conout_rb, i, &c) != RING_BUFFER_OK)
			return;

		s_conout_packet[1u + 2u * i] = hex_digit(c >> 4);
		s_conout_packet[2u + 2u * i] = hex_digit(c);
	}

	if (rsp_tx_commit(
		&g_rsp_session,
		s_conout_packet,
		1u + 2u * len,
		false
	)) {
		ring_buffer_discard(&s_conout_rb, len);
	}
}

void gdb_stub_print(const char *text)
{
	ASSERT(text != NULL);

	while (*text != '\0') {
		u8 c = (u8)*text++;
		(void)ring_buffer_put_overwrite(&s_conout_rb, &c, NULL);
	}

	u8 newline = '\n';
	(void)ring_buffer_put_overwrite(&s_conout_rb, &newline, NULL);
}

/*--------------------------------------------------------------------*/

bool gdb_stub_take_handoff_request(u32 *entry_addr)
{
	if (s_pending.state != GDB_ACTION_READY
	 || s_pending.command.action != GDB_ACTION_HANDOFF)
		return false;

	*entry_addr = s_pending.command.boot_entry_addr;
	s_pending.state = GDB_ACTION_IDLE;
	return true;
}


/*----- Static function implementations ------------------------------*/

static void _handle_packet(void)
{
	rsp_packet_view_t packet = rsp_rx_packet_view(&g_rsp_session);
	gdb_request_t request = {
		.data = packet.data,
		.len  = packet.len,
	};
	gdb_reply_writer_t reply = {
		.data     = s_reply_data,
		.capacity = sizeof(s_reply_data),
	};
	gdb_command_result_t result =
		gdb_server_handle_req(&g_gdb_server, request, &reply);

	// Never submit a partial reply or execute its deferred action.
	if (reply.overflow) {
		memcpy(s_reply_data, "E01", 3u);
		reply.len = 3u;
		result = (gdb_command_result_t){ .send_reply = true };
	}

	s_pending.command   = result;
	s_pending.reply_len = reply.len;
	s_pending.state = result.send_reply
		? GDB_ACTION_WAIT_REPLY_QUEUE : GDB_ACTION_READY;

	if (result.send_reply)
		_service_pending_action();
	else
		_apply_action();
}

static void _service_pending_action(void)
{
	if (s_pending.state != GDB_ACTION_WAIT_REPLY_QUEUE)
		return;

	bool enter_no_ack =
		s_pending.command.protocol_change == GDB_PROTOCOL_ENTER_NO_ACK;
	if (!rsp_tx_commit(&g_rsp_session, s_reply_data,
	                   s_pending.reply_len, enter_no_ack))
		return;

	if (s_pending.command.action == GDB_ACTION_HANDOFF) {
		s_pending.state = GDB_ACTION_WAIT_REPLY_ACK;
		return;
	}

	s_pending.state = GDB_ACTION_READY;
	_apply_action();
}

static void _apply_action(void)
{
	gdb_action_t action = s_pending.command.action;
	/* A ready handoff is consumed only by the main loop. */
	if (action != GDB_ACTION_HANDOFF)
		s_pending.state = GDB_ACTION_IDLE;

	switch (action) {
	case GDB_ACTION_STEP:
	case GDB_ACTION_CONTINUE:
		s_stop_reply_pending = true;
		gdb_monitor_continue();
		break;
	case GDB_ACTION_DETACH:
	case GDB_ACTION_HANDOFF:
		gdb_monitor_continue();
		break;
	case GDB_ACTION_NONE:
		break;
	}
}

static bool _try_queue_stop_reply(void)
{
	static const u8 stop_request_data[] = { '?' };

	gdb_request_t request = {
		.data = stop_request_data,
		.len  = sizeof(stop_request_data),
	};

	gdb_reply_writer_t reply = {
		.data     = s_reply_data,
		.capacity = sizeof(s_reply_data),
	};

	gdb_command_result_t result = gdb_server_handle_req(
		&g_gdb_server,
		request,
		&reply
	);

	if (!result.send_reply || reply.overflow)
		return false;

	return rsp_tx_commit(
		&g_rsp_session,
		reply.data,
		reply.len,
		false
	);
}

/*----- End of file --------------------------------------------------*/
