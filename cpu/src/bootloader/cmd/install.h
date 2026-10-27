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
 * @file    install.h
 *
 * @brief   API for flashing AIS boot section, which contains the SBL.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

#ifndef INSTALL_H
#define INSTALL_H

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
	BOOTSECT_INSTALL_SUCCESS,
	BOOTSECT_INSTALL_FAIL_BACKUP_READ,
	BOOTSECT_INSTALL_FAIL_REPAIRED,
	BOOTSECT_INSTALL_FAIL_CORRUPTED,
	BOOTSECT_INSTALL_FAIL_IMAGE,
	BOOTSECT_UNKNOWN_ERROR
} bootsect_res_t;

/*----- Extern function prototypes -----------------------------------*/

void           create_sbl_ddr_snapshot(void);
bootsect_res_t install_sbl_to_flash(void);

#endif /* INSTALL_H */
