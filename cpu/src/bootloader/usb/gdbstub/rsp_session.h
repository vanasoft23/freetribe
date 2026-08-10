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
 * @file    rsp_session.h
 * 
 * @brief   GDB Remote Serial Protocol packet framer public API.
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

#ifndef RSP_SESSION_H
#define RSP_SESSION_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*----- Macros -------------------------------------------------------*/

#define RSP_PACKET_CAPACITY 0x4000u // hardcoded in PacketSize!

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    const u8 *data;
    u32       len;
} rsp_packet_view_t;

typedef enum {
    RSP_RX_EVENT_NONE = 0,
    RSP_RX_EVENT_PACKET,
    RSP_RX_EVENT_ACK,
    RSP_RX_EVENT_NACK,
} rsp_rx_event_t;

typedef struct rsp_session rsp_session_t;

/*----- Globals ------------------------------------------------------*/

extern rsp_session_t g_rsp_session;

/*----- Functions ----------------------------------------------------*/

rsp_rx_event_t rsp_rx_feed_byte(rsp_session_t *session, u8 byte);
rsp_packet_view_t rsp_rx_packet_view(const rsp_session_t *session);

const u8 *rsp_tx_peek(rsp_session_t *session, u32 *len);
void      rsp_tx_discard(rsp_session_t *session, u32 len);
bool      rsp_tx_commit(rsp_session_t *session, const u8 *data, u32 len, bool enter_no_ack);
bool      rsp_tx_retransmit_last(rsp_session_t *session);

void rsp_session_reset(rsp_session_t *session);
bool rsp_expect_escaped(rsp_session_t *session);

#endif /* RSP_SESSION_H */

/*----- End of file --------------------------------------------------*/
