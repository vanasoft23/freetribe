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
 * @file per_uart.h
 *
 * @brief  Public api for UART peripheral driver.
 */

#ifndef PER_UART_H
#define PER_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

#include "hw_uart.h"

/*----- Macros -------------------------------------------------------*/

#define UART_0 0
#define UART_1 1

#define OVERSAMPLE_13 UART_MDR_OSM_SEL
#define OVERSAMPLE_16 0

/*----- Typedefs -----------------------------------------------------*/

typedef enum { UART_TX_COMPLETE, UART_RX_COMPLETE, UART_RX_ERROR } t_uart_event;

typedef struct {
	u8 instance;
	u32 baud;
	u8 word_length;
	bool int_enable;
	u8 int_channel;
	bool fifo_enable;
	u32 oversample;
} t_uart_config;

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void per_uart_init(t_uart_config *config);

void per_uart_register_callback(u8 instance, t_uart_event event,
								void (*callback)());

void per_uart_transmit(u8 instance, u8 *buffer, u32 length);
void per_uart_transmit_int(u8 instance, u8 *buffer, u32 length);

void per_uart_receive(u8 instance, u8 *buffer, u32 length);
void per_uart_receive_int(u8 instance, u8 *buffer, u32 length);

void per_uart_terminate(u8 instance);

#ifdef __cplusplus
}
#endif
#endif
/*----- End of file --------------------------------------------------*/
