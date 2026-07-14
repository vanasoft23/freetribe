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
 * @file    os_compat.h
 *
 * @brief   OS abstraction layer shared between the bare-metal bootloader
 *          and FreeRTOS kernel builds.
 */

#ifndef OS_COMPAT_H
#define OS_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif



#ifdef FREETRIBE_FREERTOS

#include "FreeRTOS.h"
#include "task.h"

#define OS_YIELD()                                                     \
    do {                                                               \
        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {       \
            taskYIELD();                                               \
        }                                                              \
    } while (0)

#else

#define OS_YIELD() ((void)0)

#endif



#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
