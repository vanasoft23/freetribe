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
 * @file    svc_display.h
 *
 * @brief   Header for svc_display.c.
 */

#ifndef SVC_DISPLAY_H
#define SVC_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void svc_display_task(void *param);
void svc_display_process(void);
void svc_display_put_pixel(u16 pos_x, u16 pos_y, bool state);

s8 svc_display_fill_frame(u16 x_start, u16 y_start,
                              u16 x_end, u16 y_end, bool state);

void svc_display_set_contrast(u8 contrast);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
