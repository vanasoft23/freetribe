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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "dev_mcu.h"

#include "FreeRTOS.h"
#include "task.h"

#ifdef FREETRIBE_FREERTOS
#include "knl_main.h"
#endif

#include "svc_panel.h"

#include "ft_error.h"

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

typedef void (*t_button_callback)(uint8_t button, bool state);
typedef void (*t_encoder_callback)(uint8_t enc, int8_t val);
typedef void (*t_knob_callback)(uint8_t knob, uint8_t val);
typedef void (*t_undefined_callback)(void);
typedef void (*t_trigger_callback)(uint8_t pad, uint8_t vel, bool state);
typedef void (*t_xy_pad_callback)(uint32_t x_val, uint32_t y_val);
typedef void (*t_panel_ack_callback)(uint32_t version);
typedef void (*t_held_buttons_callback)(uint32_t *held_buttons);

typedef struct {
    t_panel_event event;
    uint32_t value0;
    uint32_t value1;
    uint32_t value2;
} t_panel_user_event;

/*----- Static variable definitions ----------------------------------*/

static uint8_t g_led_current_brightness[LED_COUNT] = {0};

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

static t_status _panel_init(void);
static t_status _panel_parse(uint8_t *msg);
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
    static uint8_t panel_msg[5] = {0};

    switch (state) {

    case STATE_INIT:
        if (error_check(_panel_init()) == SUCCESS) {
            state = STATE_RUN;
        } else {
            // Remain in INIT state until initialisation successful.
            break;
        }
        // Fall through and drain any messages that arrived during init.

    case STATE_RUN:

        while (dev_mcu_rx_dequeue(panel_msg) == SUCCESS) {
            _panel_parse(panel_msg);
        }
        // No error if MCU message not available.
        break;

    case STATE_ERROR:
        error_check(UNRECOVERABLE_ERROR);
        break;

    default:
        /// TODO: Record unhandled state.
        if (error_check(UNHANDLED_STATE_ERROR) != SUCCESS) {
            state = STATE_ERROR;
        }
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

    uint8_t msg[5] = {0};

    msg[0] = 0x91;

    dev_mcu_tx_enqueue(msg);
}

void svc_panel_calib_xy(uint32_t xcal, uint32_t ycal) {

    uint8_t msg[5];

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

void svc_panel_set_trigger_mode(uint8_t mode) {

    uint8_t msg[5] = {0};

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
void svc_panel_set_led(t_led_index led_index, uint8_t brightness) {

    uint8_t mcu_msg[5] = {0, 0, 0, 0, 0};

    mcu_msg[1] = led_index;
    mcu_msg[2] = brightness;

    dev_mcu_tx_enqueue(mcu_msg);
    g_led_current_brightness[led_index] = brightness;
}

/**
 * Toggle LED.
 */
/// TODO: Toggle to specific brightness.
void svc_panel_toggle_led(t_led_index led_index) {

    uint8_t mcu_msg[5] = {0, 0, 0, 0, 0};

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

static t_status _panel_init(void) {

    dev_mcu_init();

    t_status result = TASK_INIT_ERROR;

    uint8_t panel_msg[5] = {0};

    // Send initial message to MCU.
    panel_msg[0] = 0x80;

    dev_mcu_tx_enqueue(panel_msg);

    /// TODO: Non blocking / Timeout error?.
    //
    // Block until MCU acknowledges.
    // System is not useful without MCU running.
    while (dev_mcu_rx_dequeue(panel_msg) != SUCCESS) {
#ifdef FREETRIBE_FREERTOS
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#endif
        ;
    }

    _panel_parse(panel_msg);

    result = SUCCESS;

    return result;
}

static t_status _panel_parse(uint8_t *msg) {

    t_status result = PANEL_PARSE_ERROR;
    t_panel_user_event event = {0};
    static uint32_t held_buttons[2];
    uint32_t version;

    switch (msg[0]) {

    case BUTTON_EVENT:
        if (p_button_callback != NULL) {
            event.event = BUTTON_EVENT;
            event.value0 = msg[1];
            event.value1 = (bool)msg[2];
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
        break;

    case ENCODER_EVENT:
        if (p_encoder_callback != NULL) {
            event.event = ENCODER_EVENT;
            event.value0 = msg[1];
            event.value1 = msg[3];
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
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
        result = SUCCESS;
        break;

    case UNDEFINED_EVENT:
        if (p_undefined_callback != NULL) {
            event.event = UNDEFINED_EVENT;
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
        break;

    case TRIGGER_EVENT:
        if (p_trigger_callback != NULL) {
            event.event = TRIGGER_EVENT;
            event.value0 = msg[1];
            event.value1 = msg[3];
            event.value2 = (bool)msg[4];
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
        break;

    case XY_PAD_EVENT:
        if (p_xy_pad_callback != NULL) {
            event.event = XY_PAD_EVENT;
            event.value0 = msg[1] << 8 | msg[2];
            event.value1 = msg[3] << 8 | msg[4];
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
        break;

    case MSG_ID_ACK:
        /// TODO: Is this actually version number?
        version = msg[1] << 0x18 | msg[2] << 0x10 | msg[3] << 0x8 | msg[4];

        if (p_panel_ack_callback != NULL) {
            event.event = PANEL_ACK_EVENT;
            event.value0 = version;
            _panel_post_user_event(&event);
        }
        result = SUCCESS;
        break;

    case MSG_ID_BUTTONS_LSW:
        held_buttons[0] =
            msg[1] << 0x18 | msg[2] << 0x10 | msg[3] << 0x8 | msg[4];
        // Callback not triggered until MSW received.
        result = SUCCESS;
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

        result = SUCCESS;
        break;

    default:
        /// TODO: Handle unknown msg_id.
        result = WARNING;
        break;
    }

    return result;
}

static void _panel_dispatch_event(const t_panel_user_event *event) {

    switch (event->event) {

    case BUTTON_EVENT:
        if (p_button_callback != NULL) {
            p_button_callback((uint8_t)event->value0, (bool)event->value1);
        }
        break;

    case ENCODER_EVENT:
        if (p_encoder_callback != NULL) {
            p_encoder_callback((uint8_t)event->value0,
                               (int8_t)event->value1);
        }
        break;

    case KNOB_EVENT:
        if (p_knob_callback != NULL) {
            p_knob_callback((uint8_t)event->value0, (uint8_t)event->value1);
        }
        break;

    case UNDEFINED_EVENT:
        if (p_undefined_callback != NULL) {
            p_undefined_callback();
        }
        break;

    case TRIGGER_EVENT:
        if (p_trigger_callback != NULL) {
            p_trigger_callback((uint8_t)event->value0,
                               (uint8_t)event->value1,
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
            uint32_t held_buttons[2] = {event->value0, event->value1};
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
