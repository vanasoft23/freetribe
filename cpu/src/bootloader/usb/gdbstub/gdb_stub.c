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

#include "rsp_session.h"
#include "gdb_server.h"
#include "gdb_monitor.h"
#include "gdb_stub.h"

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    GDB_HANDOFF_IDLE,
    GDB_HANDOFF_WAIT_ACK,
    GDB_HANDOFF_READY,
} gdb_handoff_state_t;

/*----- Static variables ---------------------------------------------*/

static u8                           s_reply_data[RSP_PACKET_CAPACITY];
static bool                         s_stop_reply_pending;
static volatile gdb_handoff_state_t s_handoff_state;

/*----- Static function prototypes -----------------------------------*/

static bool _try_queue_stop_reply(void);

/*----- Extern function implementations ------------------------------*/

void gdb_stub_reset(void) {
    s_stop_reply_pending = false;
    s_handoff_state      = GDB_HANDOFF_IDLE;
    rsp_session_reset(&g_rsp_session);
    gdb_monitor_init();
}

/**
 * @brief   Parse and handle a chunk of bytes received over USB CDC.
 * 
 * @param[in]  data   Raw incoming buffer of USB bytes
 * @param[in]  len    Number of incoming USB bytes
 */
void gdb_stub_rx(const u8 *data, u32 len) {

    for (u32 i = 0; i < len; i++) {

        // Advance RSP parser state machine by 1 byte
        rsp_rx_event_t event =
            rsp_rx_feed_byte(&g_rsp_session, data[i]);
        
        switch (event) {
        case RSP_RX_EVENT_NONE:
             continue;
        
        case RSP_RX_EVENT_ACK:
             if (s_handoff_state == GDB_HANDOFF_WAIT_ACK) {
                 s_handoff_state = GDB_HANDOFF_READY;
                 gdb_monitor_continue(); // leave ISR
             }
             continue;
        
        case RSP_RX_EVENT_NACK:
             (void)rsp_tx_retransmit_last(&g_rsp_session);
             continue;
             
        case RSP_RX_EVENT_PACKET:
             break; // fall through
        }

        /// @TODO: REFACTOR!!11
        // We got a packet, delegate it's handling to GDB server
        rsp_packet_view_t packet = rsp_rx_packet_view(&g_rsp_session);
        gdb_request_t request = {
            .data = packet.data,
            .len  = packet.len,
        };
        gdb_reply_writer_t reply = {
            .data     = s_reply_data,
            .len      = 0,
            .capacity = sizeof(s_reply_data),
            .overflow = false,
        };

        gdb_command_result_t result =
            gdb_server_handle_req(
                &g_gdb_server,
                request,
                &reply
            );
        
        // Maybe we got a reply back.
        // If so, then let RSP module turn it into a valid RSP packet.
        // RSP module will then defer packet transfer to CDC.
        bool reply_queued = false;
        if (result.send_reply) {
            /// NOTE: no ACK mode (QStartNoAckMode) is not supported yet
            bool enter_no_ack =
                result.protocol_change == GDB_PROTOCOL_ENTER_NO_ACK;

            reply_queued = rsp_tx_commit(
                &g_rsp_session,
                reply.data,
                reply.len,
                enter_no_ack
            );

            if (!reply_queued) {
                ASSERT(reply_queued); // TEMPORARY -> WILL CRASH KERNEL!
                // @TODO: gracefully recover by reconnecting GDB transport
                // Do not execute an action whose reply was not queued.
                continue;
            }
        }

        // GDB server handled the packet, and maybe requires us to take
        // additional actions.
        switch (result.action) {
        case GDB_ACTION_NONE: break;
        case GDB_ACTION_STEP:
             s_stop_reply_pending = true;
             gdb_monitor_continue();
             break;
        case GDB_ACTION_CONTINUE:
             s_stop_reply_pending = true;
             gdb_monitor_continue();
             break;
        case GDB_ACTION_HANDOFF:
             if (reply_queued) {
                 s_handoff_state = GDB_HANDOFF_WAIT_ACK;
             }
             break;
        case GDB_ACTION_DETACH:
             gdb_monitor_continue();
             break;
        default: break;
        }

    }

}

/**
 * @brief   Get queued GDB RSP response.
 */
const u8 *gdb_stub_tx_peek(u32 *len) {
    return rsp_tx_peek(&g_rsp_session, len);
}

void gdb_stub_tx_discard(u32 len) {
    rsp_tx_discard(&g_rsp_session, len);
}


/*--------------------------------------------------------------------*/

void gdb_stub_service_stop_reply(void) {
    if (s_stop_reply_pending) {
        if (gdb_monitor_frame_ready() && _try_queue_stop_reply()) {
            s_stop_reply_pending = false;
        }
    }
}

/*--------------------------------------------------------------------*/

bool gdb_stub_take_handoff_request(void) {

    if (s_handoff_state != GDB_HANDOFF_READY)
        return false;

    s_handoff_state = GDB_HANDOFF_IDLE;
    return true;
}


/*----- Static function implementations ------------------------------*/

static bool _try_queue_stop_reply(void) {

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

    if (!result.send_reply) {
        return false;
    }

    return rsp_tx_commit(
        &g_rsp_session,
        reply.data,
        reply.len,
        false
    );
}

/*----- End of file --------------------------------------------------*/