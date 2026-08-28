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
 * @file    tud_cdc.c
 *
 * @brief   USB Communications Device Class for GDB USB.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <class/cdc/cdc_device.h>
#include <string.h>
#include <tusb.h>

#include "gdb_monitor.h"
#include "gdb_stub.h"

/*----- Macros -------------------------------------------------------*/

#define TRANSPORT_RX_CHUNK_SIZE 64u
#define CTRL_C                  ((u8)0x03)

/*----- Extern variable definitions ----------------------------------*/

/*----- Static variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _rx_pump(u8 itf);
static void _tx_pump(u8 itf);

/*----- Extern function implementations ------------------------------*/

u32 cdc_isr_count = 0;

void tud_cdc_rx_isr_cb(u8 itf, const u8 *buffer, u32 bufsize)
{
	(void) itf;

	cdc_isr_count++;
	if (gdb_monitor_target_running() &&
		memchr(buffer, CTRL_C, bufsize) != NULL)
	{
		gdb_monitor_request_stop(SIGINT);
	}

}

/**
 * @brief   Invoked when cdc when line state changed e.g
 *          connected/disconnected.
 */
void tud_cdc_line_state_cb(u8 itf, bool dtr, bool rts)
{
	(void) rts;

	// DLOG("ITF=%i DTR=%i RTS=%i", (int)itf, (int)dtr, (int)rts);

	bool connected = dtr;
	if (connected) {
		gdb_monitor_request_stop(SIGTRAP);
		_rx_pump(itf);
		_tx_pump(itf);

	} else {
		gdb_stub_reset();
		tud_cdc_n_write_clear(itf);
		tud_cdc_n_read_flush(itf);
	}
}

/**
 * @brief   Invoked when CDC interface received data from host.
 */
void tud_cdc_rx_cb(u8 itf)
{
	_rx_pump(itf);
	_tx_pump(itf);
}

/**
 * @brief   Invoked when a TX is complete and therefore space becomes
 *          available in TX buffer.
 */
void tud_cdc_tx_complete_cb(u8 itf)
{
	_tx_pump(itf);
	_rx_pump(itf);
}

/**
 * @brief   Invoked on physical disconnect or USB reset.
 */
void cdc_umount_cb(void) {
	gdb_stub_reset();
}

void tud_cdc_rsp_task(void)
{
	if (!tud_cdc_n_connected(0)) {
		return;
	}

	_rx_pump(0);

	gdb_stub_service_stop_reply();

	gdb_stub_service_console_output();

	_tx_pump(0);
}

/*----- Static function implementations ------------------------------*/

static void _rx_pump(u8 itf)
{
	u8 buffer[TRANSPORT_RX_CHUNK_SIZE];

	while (tud_cdc_n_connected(itf)) {

		u32 available = tud_cdc_n_available(itf);
		if (available == 0)
			return;

		u32 wanted = available;
		if (wanted > sizeof(buffer))
			wanted = sizeof(buffer);

		u32 count = tud_cdc_n_read(itf, buffer, wanted);
		if (count == 0)
			return;
		
		gdb_stub_rx(buffer, count);
	}
}

static void _tx_pump(u8 itf)
{

	while (tud_cdc_n_connected(itf)) {

		u32 pending;
		const u8 *data = gdb_stub_tx_peek(&pending);
		if (pending == 0)
			break;

		u32 written = tud_cdc_n_write(itf, data, pending);
		if (written == 0)
			break;

		gdb_stub_tx_discard(written);
	}

	if (tud_cdc_n_connected(itf)) {
		tud_cdc_n_write_flush(itf);
	}
}

/*----- End of file --------------------------------------------------*/
