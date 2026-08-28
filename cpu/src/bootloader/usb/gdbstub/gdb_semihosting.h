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
 * @file    gdb_semihosting.h
 *
 * @brief   GDB semihosting service API; Allows for printing to GDB.
 * 
 * @details
 * In order to print to GDB over USB, an SWI exception is raised with
 * a specific number. SWI handling is implemented separately from this
 * configuration API.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef GDB_SEMIHOSTING_H
#define GDB_SEMIHOSTING_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"


bool gdb_semihosting_enable(u32 vector_address);


bool gdb_semihosting_set_io_client(u32 client_mask);

#endif /* GDB_SEMIHOSTING_H */

#endif