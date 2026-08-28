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
 * @file    svc_panel.c
 *
 * @brief   Interface to panel controls and LED.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <stdio.h>

#include "dev_mcu.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef FREETRIBE_FREERTOS
#include "knl_main.h"
#endif

#include "svc_panel.h"



/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
	STATE_INIT,
	STATE_RUN,
	STATE_ERROR,
} t_panel_task_state;

typedef enum {
	MSG_ID_CONTROL = 0x05,
	MSG_ID_ACK = 0x80,
	MSG_ID_BUTTONS_LSW = 0x91, // Low word of 64 bit held buttons mask.
	MSG_ID_BUTTONS_MSW = 0x92  // High word.
} t_panel_msg_id;

typedef void (*t_button_callback)(u8 button, bool state);
typedef void (*t_encoder_callback)(u8 enc, s8 val);
typedef void (*t_knob_callback)(u8 knob, u8 val);
typedef void (*t_undefined_callback)(void);
typedef void (*t_trigger_callback)(u8 pad, u8 vel, bool state);
typedef void (*t_xy_pad_callback)(u32 x_val, u32 y_val);
typedef void (*t_panel_ack_callback)(u32 version);
typedef void (*t_held_buttons_callback)(u32 *held_buttons);

typedef struct {
	t_panel_event event;
	u32 value0;
	u32 value1;
	u32 value2;
} t_panel_user_event;

/*----- Static variable definitions ----------------------------------*/

static u8 g_led_current_brightness[LED_COUNT] = {0};

static t_button_callback p_button_callback = NULL;
static t_encoder_callback p_encoder_callback = NULL;
static t_knob_callback p_knob_callback = NULL;
static t_undefined_callback p_undefined_callback = NULL;
static t_trigger_callback p_trigger_callback = NULL;
static t_xy_pad_callback p_xy_pad_callback = NULL;
static t_panel_ack_callback p_panel_ack_callback = NULL;
static t_held_buttons_callback p_held_buttons_callback = NULL;

#ifdef FREETRIBE_FREERTOS
static TaskHandle_t g_panel_task_handle = NULL;
#endif

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static bool _panel_init(void);
static bool _panel_parse(u8 *msg);
static void _panel_dispatch_event(const t_panel_user_event *event);
#ifdef FREETRIBE_FREERTOS
static void _panel_user_event_dispatch(const void *payload);
static void _panel_post_user_event(const t_panel_user_event *event);
#else
static void _panel_post_user_event(const t_panel_user_event *event);
#endif
#ifdef FREETRIBE_FREERTOS
static void _panel_rx_notify_from_isr(void);
#endif

/*----- Extern function implementations ------------------------------*/

void svc_panel_task(void *param) {
	(void)param;

#ifdef FREETRIBE_FREERTOS
	g_panel_task_handle = xTaskGetCurrentTaskHandle();
	dev_mcu_register_rx_callback(_panel_rx_notify_from_isr);
#endif

	while (true) {
		svc_panel_process();
#ifdef FREETRIBE_FREERTOS
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#endif
	}
}

void svc_panel_process(void) {

	static t_panel_task_state state = STATE_INIT;
	static u8 panel_msg[5] = {0};

	switch (state) {

	case STATE_INIT:
		if (_panel_init()) {
			state = STATE_RUN;
		}
		// Remain in INIT state until initialisation successful.
		break;

	case STATE_RUN:

		while (dev_mcu_rx_dequeue(panel_msg)) {
			_panel_parse(panel_msg);
		}
		// No error if MCU message not available.
		break;

	case STATE_ERROR:
	default:
		PANIC(PANIC_UNHANDLED_STATE);
		break;
	}
}

void svc_panel_register_callback(t_panel_event event, void *callback) {

	switch (event) {

	case BUTTON_EVENT:
		p_button_callback = (t_button_callback)callback;
		break;

	case ENCODER_EVENT:
		p_encoder_callback = (t_encoder_callback)callback;
		break;

	case KNOB_EVENT:
		p_knob_callback = (t_knob_callback)callback;
		break;

	case UNDEFINED_EVENT:
		p_undefined_callback = (t_undefined_callback)callback;
		break;

	case TRIGGER_EVENT:
		p_trigger_callback = (t_trigger_callback)callback;
		break;

	case XY_PAD_EVENT:
		p_xy_pad_callback = (t_xy_pad_callback)callback;
		break;

	case PANEL_ACK_EVENT:
		p_panel_ack_callback = (t_panel_ack_callback)callback;
		break;

	case HELD_BUTTONS_EVENT:
		p_held_buttons_callback = (t_held_buttons_callback)callback;
		break;

	default:
		break;
	}
}

void svc_panel_request_buttons(void) {

	u8 msg[5] = {0};

	msg[0] = 0x91;

	dev_mcu_tx_enqueue(msg);
}

void svc_panel_calib_xy(u32 xcal, u32 ycal)
{
	u8 msg[5];

	msg[0] = 0x85;
	msg[1] = (xcal >> 24) & 0xff;
	msg[2] = (xcal >> 16) & 0xff;
	msg[3] = (xcal >> 8) & 0xff;
	msg[4] = xcal & 0xff;

	dev_mcu_tx_enqueue(msg);

	msg[0] = 0x86;
	msg[1] = (ycal >> 24) & 0xff;
	msg[2] = (ycal >> 16) & 0xff;
	msg[3] = (ycal >> 8) & 0xff;
	msg[4] = ycal & 0xff;

	dev_mcu_tx_enqueue(msg);
}

void svc_panel_set_trigger_mode(u8 mode)
{
	u8 msg[5] = {0};

	if (mode != 0) {
		mode = 1;
	}

	msg[0] = 0x84;
	msg[1] = mode;

	dev_mcu_tx_enqueue(msg);
}

/**
 * Set LED.
 */
void svc_panel_set_led(t_led_index led_index, u8 brightness)
{
	u8 mcu_msg[5] = {0, 0, 0, 0, 0};

	mcu_msg[1] = led_index;
	mcu_msg[2] = brightness;

	dev_mcu_tx_enqueue(mcu_msg);
	g_led_current_brightness[led_index] = brightness;
}

/**
 * Toggle LED.
 */
/// TODO: Toggle to specific brightness.
void svc_panel_toggle_led(t_led_index led_index)
{
	u8 mcu_msg[5] = {0, 0, 0, 0, 0};

	mcu_msg[1] = led_index;

	if (g_led_current_brightness[led_index] != 0) {
		mcu_msg[2] = 0x00;
	} else {
		mcu_msg[2] = 0xff;
	}

	dev_mcu_tx_enqueue(mcu_msg);
	g_led_current_brightness[led_index] = mcu_msg[2];
}

/*----- Static function implementations ------------------------------*/

static bool _panel_init(void)
{
	dev_mcu_init();

	u8 panel_msg[5] = {0};

	// Send initial message to MCU.
	panel_msg[0] = 0x80;

	dev_mcu_tx_enqueue(panel_msg);

	// Block until MCU acknowledges.
	// System is not useful without MCU running.
	while (!dev_mcu_rx_dequeue(panel_msg)) {
		;
	}

	_panel_parse(panel_msg);

	return true;
}

static bool _panel_parse(u8 *msg)
{
	t_panel_user_event event = {0};
	static u32 held_buttons[2];
	u32 version;

	DLOG("_panel_parse: %u, %u, %u, %u, %u",
	     (unsigned int)msg[0],
	     (unsigned int)msg[1],
	     (unsigned int)msg[2],
	     (unsigned int)msg[3],
	     (unsigned int)msg[4]);

	switch (msg[0]) {

	case BUTTON_EVENT:
		if (p_button_callback != NULL) {
			event.event = BUTTON_EVENT;
			event.value0 = msg[1];
			event.value1 = (bool)msg[2];
			_panel_post_user_event(&event);
		}
		break;

	case ENCODER_EVENT:
		if (p_encoder_callback != NULL) {
			event.event = ENCODER_EVENT;
			event.value0 = msg[1];
			event.value1 = msg[3];
			_panel_post_user_event(&event);
		}
		break;

	case KNOB_EVENT:
		// In continuous mode, trigger pads use KNOB_EVENT message ID.
		if (msg[1] >= 0x11) {
			if (p_trigger_callback != NULL) {
				event.event = TRIGGER_EVENT;
				event.value0 = msg[1];
				event.value1 = msg[3];
				event.value2 = (bool)msg[4];
				_panel_post_user_event(&event);
			}

		} else {
			if (p_knob_callback != NULL) {
				event.event = KNOB_EVENT;
				event.value0 = msg[1];
				event.value1 = msg[3];
				_panel_post_user_event(&event);
			}
		}
		break;

	case UNDEFINED_EVENT:
		if (p_undefined_callback != NULL) {
			event.event = UNDEFINED_EVENT;
			_panel_post_user_event(&event);
		}
		break;

	case TRIGGER_EVENT:
		if (p_trigger_callback != NULL) {
			event.event = TRIGGER_EVENT;
			event.value0 = msg[1];
			event.value1 = msg[3];
			event.value2 = (bool)msg[4];
			_panel_post_user_event(&event);
		}
		break;

	case XY_PAD_EVENT:
		if (p_xy_pad_callback != NULL) {
			event.event = XY_PAD_EVENT;
			event.value0 = msg[1] << 8 | msg[2];
			event.value1 = msg[3] << 8 | msg[4];
			_panel_post_user_event(&event);
		}
		break;

	case MSG_ID_ACK:
		/// TODO: Is this actually version number?
		version = msg[1] << 0x18 | msg[2] << 0x10 | msg[3] << 0x8 | msg[4];

		if (p_panel_ack_callback != NULL) {
			event.event = PANEL_ACK_EVENT;
			event.value0 = version;
			_panel_post_user_event(&event);
		}
		break;

	case MSG_ID_BUTTONS_LSW:
		held_buttons[0] =
			msg[1] << 0x18 | msg[2] << 0x10 | msg[3] << 0x8 | msg[4];
		// Callback not triggered until MSW received.
		break;

	case MSG_ID_BUTTONS_MSW:
		held_buttons[1] =
			msg[1] << 0x18 | msg[2] << 0x10 | msg[3] << 0x8 | msg[4];

		if (p_held_buttons_callback != NULL) {
			event.event = HELD_BUTTONS_EVENT;
			event.value0 = held_buttons[0];
			event.value1 = held_buttons[1];
			_panel_post_user_event(&event);
		}

		/// TODO: Clear state?
		// held_buttons[0] = 0;
		// held_buttons[1] = 0;

		break;

	default:
		/// TODO: Handle unknown msg_id.
		DLOG("Unknown msg_id");
		return false;
	}

	return true;
}

static void _panel_dispatch_event(const t_panel_user_event *event)
{
	switch (event->event) {

	case BUTTON_EVENT:
		if (p_button_callback != NULL) {
			p_button_callback((u8)event->value0, (bool)event->value1);
		}
		break;

	case ENCODER_EVENT:
		if (p_encoder_callback != NULL) {
			p_encoder_callback((u8)event->value0,
							   (s8)event->value1);
		}
		break;

	case KNOB_EVENT:
		if (p_knob_callback != NULL) {
			p_knob_callback((u8)event->value0, (u8)event->value1);
		}
		break;

	case UNDEFINED_EVENT:
		if (p_undefined_callback != NULL) {
			p_undefined_callback();
		}
		break;

	case TRIGGER_EVENT:
		if (p_trigger_callback != NULL) {
			p_trigger_callback((u8)event->value0,
							   (u8)event->value1,
							   (bool)event->value2);
		}
		break;

	case XY_PAD_EVENT:
		if (p_xy_pad_callback != NULL) {
			p_xy_pad_callback(event->value0, event->value1);
		}
		break;

	case PANEL_ACK_EVENT:
		if (p_panel_ack_callback != NULL) {
			p_panel_ack_callback(event->value0);
		}
		break;

	case HELD_BUTTONS_EVENT:
		if (p_held_buttons_callback != NULL) {
			u32 held_buttons[2] = {event->value0, event->value1};
			p_held_buttons_callback(held_buttons);
		}
		break;

	default:
		break;
	}
}

#ifdef FREETRIBE_FREERTOS
static void _panel_user_event_dispatch(const void *payload) {

	_panel_dispatch_event((const t_panel_user_event *)payload);
}

static void _panel_post_user_event(const t_panel_user_event *event) {

	(void)knl_post_user_event(_panel_user_event_dispatch, event,
							  sizeof(*event));
}
#else
static void _panel_post_user_event(const t_panel_user_event *event) {

	_panel_dispatch_event(event);
}
#endif

#ifdef FREETRIBE_FREERTOS
static void _panel_rx_notify_from_isr(void) {

	BaseType_t higher_priority_task_woken = pdFALSE;

	vTaskNotifyGiveFromISR(g_panel_task_handle,
							&higher_priority_task_woken);

	portYIELD_FROM_ISR(higher_priority_task_woken);
	
}
#endif

/*----- End of file --------------------------------------------------*/
