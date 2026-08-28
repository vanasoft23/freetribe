#if 0
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
 * @file    gdb_semihosting.c
 *
 * @brief   GDB semihosting service; Allows for printing to GDB.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "gdb_semihosting.h"


typedef struct {
	bool enabled;
	u32  vector_address;
	u8   io_client;
} gdb_semihosting_config_t;

static gdb_semihosting_config_t s_config;


bool gdb_semihosting_enable(u32 vector_address) {
	if ((vector_address != 0x00000008u) &&
		(vector_address != 0xffff0008u)) {
		return false;
	}

	s_config.vector_address = vector_address;
	s_config.enabled = true;
	return true;
}

bool gdb_semihosting_set_io_client(u32 client_mask) {
	// This stub only has the GDB connection; no telnet client. 
	if (client_mask != 2u) {
		return false;
	}

	s_config.io_client = (u8)client_mask;
	return true;
}

/*----- End of file --------------------------------------------------*/
#endif