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
 * @file    gdb_server.h
 *
 * @brief   GDB command handler & reply generator
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef GDB_SERVER_H
#define GDB_SERVER_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	const u8 *data;
	u32       len;
} gdb_request_t;

typedef struct {
	u8    *data;
	u32    len;
	u32    capacity;
	bool   overflow;
} gdb_reply_writer_t;

typedef enum {
	GDB_ACTION_NONE,
	GDB_ACTION_CONTINUE,
	GDB_ACTION_STEP,
	GDB_ACTION_DETACH,
	GDB_ACTION_HANDOFF,
} gdb_action_t;

typedef enum {
	GDB_PROTOCOL_UNCHANGED,
	GDB_PROTOCOL_ENTER_NO_ACK,
} gdb_protocol_change_t;

typedef struct {
	bool                  send_reply;
	gdb_action_t          action;
	gdb_protocol_change_t protocol_change;
} gdb_command_result_t;

typedef struct gdb_server gdb_server_t;

/**
 * @brief Copy target memory into a temporary buffer.
 *
 * @return true on success, or false when any byte in the requested range
 *         cannot be read safely.
 */
typedef bool (*gdb_memory_reader_t)(
	void *context,
	u32   address,
	u8   *destination,
	u32   length
);

/**
 * @brief Copy bytes into target memory.
 *
 * @return true on success, or false when any byte in the requested range
 *         cannot be written safely.
 */
typedef bool (*gdb_memory_writer_t)(
	void     *context,
	u32       address,
	const u8 *source,
	u32       length
);

extern gdb_server_t g_gdb_server;

/*----- Functions ----------------------------------------------------*/

gdb_command_result_t gdb_server_handle_req(
	gdb_server_t       *server,
	gdb_request_t       request,
	gdb_reply_writer_t *reply
);

void gdb_server_set_stop(
	gdb_server_t  *server,
	int            sigval
);

/**
 * @brief Override target-memory access (for range validation or testing).
 * @details Passing NULL restores direct target-memory reads.
 */
void gdb_server_set_memory_reader(
	gdb_server_t       *server,
	gdb_memory_reader_t reader,
	void               *context
);

/**
 * @brief Override target-memory writes (for range validation or testing).
 * @details Passing NULL restores direct target-memory writes.
 */
void gdb_server_set_memory_writer(
	gdb_server_t       *server,
	gdb_memory_writer_t writer,
	void               *context
);

#endif /* GDB_SERVER_H */

/*----- End of file --------------------------------------------------*/
