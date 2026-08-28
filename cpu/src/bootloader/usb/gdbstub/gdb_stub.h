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
 * @file    gdb_stub.h
 *
 * @brief   GDB stub coordinates everything between USB CDC and server,
 *          monitor, RSP session.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef GDB_STUB_H
#define GDB_STUB_H

#include "ft.h"

void gdb_stub_reset(void);

/**
 * @brief   Parse and handle a chunk of bytes received over USB CDC.
 * 
 * @param[in]  data   Raw incoming buffer of USB bytes
 * @param[in]  len    Number of incoming USB bytes
 */
void gdb_stub_rx(const u8 *data, u32 len);

const u8 *gdb_stub_tx_peek(u32 *len);
void gdb_stub_tx_discard(u32 len);

void gdb_stub_service_stop_reply(void);
void gdb_stub_service_console_output(void);
void gdb_stub_print(const char *text);

bool gdb_stub_take_handoff_request(void);

#endif /* GDB_STUB_H */
