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
 * @file    svc_cpu.c
 *
 * @brief   Service for communicating with host CPU.
 */

/// TODO: CPU device driver, command service.

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>
#include <string.h>



#include "ft_error.h"
#include "per_gpio.h"
#include "per_spi.h"
#include "dev_cpu_spi.h"
#include "dev_cpu_ipc.h"

#include "module.h"

#include "knl_profile.h"

/*----- Macros -------------------------------------------------------*/

#define MSG_START 0xf0

/*----- Typedefs -----------------------------------------------------*/

typedef enum { STATE_INIT, STATE_RUN, STATE_ERROR } t_cpu_task_state;

typedef enum {
	PARSE_START,
	PARSE_MSG_TYPE,
	PARSE_MSG_ID,
	PARSE_PAYLOAD_LENGTH,
	PARSE_PAYLOAD
} t_msg_parse_state;

// TODO: Move protocol definition to common module.
//       Union struct for message.
enum e_message_type { MSG_TYPE_MODULE, MSG_TYPE_SYSTEM };

enum e_module_msg_id {
	MODULE_GET_PARAM_VALUE,
	MODULE_SET_PARAM_VALUE,
	MODULE_PARAM_VALUE,
	MODULE_GET_PARAM_NAME,
	MODULE_PARAM_NAME,
};

enum e_system_msg_id {
	SYSTEM_GET_PORT_STATE,
	SYSTEM_SET_PORT_STATE,
	SYSTEM_PORT_STATE,
	SYSTEM_GET_PROFILE,
	SYSTEM_PROFILE,
};

/*----- Static variable definitions ----------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _cpu_init(void);

static void _cpu_receive(u8 byte);

static void _transmit_message(u8 msg_type, u8 msg_id,
                              u8 *payload, u8 length);

static void _handle_message(u8 msg_type, u8 msg_id, u8 *payload,
                            u8 length);

static bool _handle_module_message(u8 msg_id, u8 *payload,
                                   u8 length);

static bool _handle_system_message(u8 msg_id, u8 *payload,
                                   u8 length);

static bool _handle_module_get_param_value(u8 *payload,
                                           u8 length);

static bool _handle_module_set_param_value(u8 *payload,
                                           u8 length);

static bool _handle_module_get_param_name(u8 *payload, u8 length);

static bool _handle_system_get_port_state(void);
static bool _handle_system_set_port_state(u8 *payload, u8 length);

static bool _handle_system_get_profile(void);

static bool _respond_module_param_value(u16 module_id,
                                        u16 param_index,
                                        s32 param_value);

static bool _respond_module_param_name(u16   module_id,
                                       u16   param_index,
                                       char *param_name);

static bool _respond_system_port_state(u16 port_f,
                                       u16 port_g,
                                       u16 port_h);

static bool _respond_system_profile(t_profile stats);

/*----- Extern function implementations ------------------------------*/

void svc_cpu_task(void) {

	static t_cpu_task_state state = STATE_INIT;

	static u8 cpu_byte;

	switch (state) {

	case STATE_INIT:
		_cpu_init();
		state = STATE_RUN;
		break;

		/// TODO: case STATE_HANDSHAKE:

	case STATE_RUN:

		// Handle received bytes.
		if (dev_cpu_spi_rx_dequeue(&cpu_byte)) {
			_cpu_receive(cpu_byte);
		}
		break;

	case STATE_ERROR:
	default:
		PANIC(PANIC_UNHANDLED_STATE);
		break;
	}
}

/*----- Static function implementations ------------------------------*/

static void _cpu_init(void) {

	// Initialise CPU SPI device driver.
	dev_cpu_spi_init();

	dev_cpu_ipc_init();

	/// TODO: Handhsake.

}

/// TODO: Return status.
static void _transmit_message(u8 msg_type, u8 msg_id,
                              u8 *payload, u8 length)
{
	//
	u8 msg_start = MSG_START;

	dev_cpu_spi_tx_enqueue(&msg_start);
	dev_cpu_spi_tx_enqueue(&msg_type);
	dev_cpu_spi_tx_enqueue(&msg_id);
	dev_cpu_spi_tx_enqueue(&length);

	while (length--) {
		dev_cpu_spi_tx_enqueue(payload++);
	}
}

/// TODO: Move to separate module.
static void _cpu_receive(u8 byte)
{
	static t_msg_parse_state state = PARSE_START;

	static u8 msg_type;
	static u8 msg_id;
	static u8 length;
	static u8 payload[0xff];
	static u8 count;

	switch (state) {

		/// TODO: Handshake.

	case PARSE_START:
		if (byte == MSG_START) {
			state = PARSE_MSG_TYPE;
		}
		// TODO: Error handling and reset protocol.
		break;

	case PARSE_MSG_TYPE:
		msg_type = byte;
		state = PARSE_MSG_ID;
		break;

	case PARSE_MSG_ID:
		msg_id = byte;
		state = PARSE_PAYLOAD_LENGTH;
		break;

	case PARSE_PAYLOAD_LENGTH:
		length = byte;
		if (length == 0) {
			count = 0;
			_handle_message(msg_type, msg_id, payload, length);
			state = PARSE_START;
		} else {
			state = PARSE_PAYLOAD;
		}
		break;

	case PARSE_PAYLOAD:
		if (count < length) {
			payload[count] = byte;
			count++;
		}
		// Handle message before returning,
		// else it is not handled until first
		// byte of next message is received.
		if (count >= length) {
			count = 0;
			_handle_message(msg_type, msg_id, payload, length);
			state = PARSE_START;
		}
		break;

	default:
		break;
	}
}

static bool _handle_message(u8  msg_type,
                            u8  msg_id,
                            u8 *payload,
                            u8 length)
{
	switch (msg_type) {
	case MSG_TYPE_MODULE:
		return _handle_module_message(msg_id, payload, length);
	case MSG_TYPE_SYSTEM:
		return _handle_system_message(msg_id, payload, length);
	default:
		PANIC(PANIC_UNHANDLED_STATE);
		return false;
	}
}

static bool _handle_module_message(u8  msg_id,
                                   u8 *payload,
                                   u8  length)
{
	switch (msg_id) {
	case MODULE_GET_PARAM_VALUE:
		return _handle_module_get_param_value(payload, length);
	case MODULE_SET_PARAM_VALUE:
		return _handle_module_set_param_value(payload, length);
	case MODULE_GET_PARAM_NAME:
		return _handle_module_get_param_name(payload, length);
	default:
		PANIC(PANIC_UNHANDLED_STATE);
		return false;
	}
}

static bool _handle_system_message(u8  msg_id,
                                   u8 *payload,
                                   u8  length)
{
	switch (msg_id) {
	case SYSTEM_GET_PORT_STATE:
		return _handle_system_get_port_state();
	case SYSTEM_SET_PORT_STATE:
		// return _handle_system_set_port_state(payload, length);
	case SYSTEM_GET_PROFILE:
		return _handle_system_get_profile();
	default:
		PANIC(PANIC_UNHANDLED_STATE);
		return false;
	}
}

static bool _handle_module_get_param_value(u8 *payload,
                                           u8 length)
{
	/// TODO: Union struct for message parsing.
	u16 module_id = (payload[1] << 8) | payload[0];

	u16 param_index = (payload[3] << 8) | payload[2];

	/// TODO: Register callbacks for message handling?
	//
	// module_id not supported yet.
	s32 param_value = module_get_param(param_index);

	return _respond_module_param_value(module_id, param_index, param_value);
}

static bool _handle_module_set_param_value(u8 *payload,
                                           u8 length)
{
	/// TODO: Union struct for message parsing.
	s16 module_id = (payload[1] << 8) | payload[0];

	u16 param_index = (payload[3] << 8) | payload[2];

	/// TODO: Register callbacks for message handling?
	s32 param_value = (payload[7] << 24) | (payload[6] << 16) |
	                  (payload[5] << 8)  | payload[4];

	// module_id not supported yet.
	return module_set_param(param_index, param_value);
}

static bool _handle_module_get_param_name(u8 *payload,
                                          u8 length)
{
	u16 module_id;
	u16 param_index;

	char param_name[MAX_PARAM_NAME_LENGTH];

	module_id = (payload[1] << 8) | payload[0];
	param_index = (payload[3] << 8) | payload[2];

	// module_id not supported yet.
	module_get_param_name(param_index, param_name);

	return _respond_module_param_name(module_id, param_index, param_name);
}

static bool _handle_system_get_port_state(void)
{
	u16 port_f = per_gpio_get_port(PORT_F);
	u16 port_g = per_gpio_get_port(PORT_G);
	u16 port_h = per_gpio_get_port(PORT_H);

	return _respond_system_port_state(port_f, port_g, port_h);
}

static bool _handle_system_get_profile(void)
{
	t_profile stats = knl_profile_stats();

	return _respond_system_profile(stats);
}

static bool _respond_module_param_value(u16 module_id,
                                        u16 param_index,
                                        s32 param_value)
{
	u8 payload[] = {
		module_id & 0xff,   (module_id >> 8) & 0xff,
		param_index & 0xff, (param_index >> 8) & 0xff,
		param_value & 0xff, (param_value >> 8 & 0xff)
	};
	_transmit_message(MSG_TYPE_MODULE,
	                  MODULE_PARAM_VALUE, payload,
	                  sizeof(payload)
	);
	return true;
}

static bool _respond_module_param_name(u16 module_id,
                                       u16 param_index,
                                       char *param_name)
{
	/// TODO: This is jank.

	u32 string_length = strlen(param_name);
	u32 payload_length = string_length + 5;

	u8 payload[payload_length];

	payload[0] = module_id & 0xff;
	payload[1] = (module_id >> 8) & 0xff;
	payload[2] = param_index & 0xff;
	payload[3] = (param_index >> 8) & 0xff;

	memcpy(payload + 4, param_name, string_length);

	// Ensure null termination.
	payload[payload_length - 1] = '\0';

	_transmit_message(MSG_TYPE_MODULE,
	                  MODULE_PARAM_NAME,
	                  payload, sizeof(payload));
	return true;
}

static bool _respond_system_port_state(u16 port_f,
                                       u16 port_g,
                                       u16 port_h)
{
	u8 payload[] = {
		port_f & 0xff, (port_f >> 8) & 0xff,
		port_g & 0xff, (port_g >> 8) & 0xff,
		port_h & 0xff, (port_h >> 8) & 0xff
	};

	_transmit_message(MSG_TYPE_SYSTEM,
	                  SYSTEM_PORT_STATE,
	                  payload, sizeof(payload));
	return true;
}

static bool _respond_system_profile(t_profile stats) {

	u8 payload[] = {
		stats.period & 0xff,         (stats.period >> 8) & 0xff,
		(stats.period >> 16) & 0xff, (stats.period >> 24) & 0xff,
		stats.cycles & 0xff,         (stats.cycles >> 8) & 0xff,
		(stats.cycles >> 16) & 0xff, (stats.cycles >> 24) & 0xff,
	};

	_transmit_message(MSG_TYPE_SYSTEM,
	                  SYSTEM_PROFILE,
	                  payload, sizeof(payload));
	return true;
}

/*----- End of file --------------------------------------------------*/
