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

   You should have received a copy of the GNU Affero General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    knl_main.c
 *
 * @brief   Kernel bootstrap and FreeRTOS task creation.
 */

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "ft_error.h"

#include "svc_display.h"
#include "svc_dsp.h"
#include "svc_midi.h"
#include "svc_panel.h"
#include "svc_system.h"
#include "svc_systick.h"
//#include "svc_usb.h"

#include "knl_main.h"

/*----- Macros -------------------------------------------------------*/

#define KNL_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

#define SYSTICK_TASK_PRIORITY 2
#define MIDI_TASK_PRIORITY 2
#define PANEL_TASK_PRIORITY 4
#define DSP_TASK_PRIORITY 3
#define DISPLAY_TASK_PRIORITY 2
//#define USB_TASK_PRIORITY 3

#define USER_EVENT_QUEUE_LENGTH 32
#define USER_EVENT_PAYLOAD_SIZE 16
#define USER_EVENT_MAX_PER_PUMP 8

#define SYSTICK_TASK_STACK_WORDS 512
#define MIDI_TASK_STACK_WORDS 512
#define PANEL_TASK_STACK_WORDS 512
#define DSP_TASK_STACK_WORDS 1024
#define DISPLAY_TASK_STACK_WORDS 512
//#define USB_TASK_STACK_WORDS 1024

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    TaskFunction_t entry;
    const char *name;
    uint16_t stack_words;
    UBaseType_t priority;
} t_knl_task_config;

typedef struct {
    t_knl_user_event_handler handler;
    uint8_t payload[USER_EVENT_PAYLOAD_SIZE];
} t_knl_user_event;

/*----- Static variable definitions ----------------------------------*/

static uint32_t g_user_tick_div;

static void (*p_user_tick_callback)(void) = NULL;

static QueueHandle_t g_user_event_queue = NULL;
static TaskHandle_t g_user_task_handle = NULL;
static bool g_user_tick_pending = false;

/*----- Static function prototypes -----------------------------------*/

static void _create_user_event_queue(void);
static void _init_services(void);
static void _register_service_callbacks(void);
static void _create_service_tasks(void);
static void _panel_ack_callback(uint32_t version);
static void _held_buttons_callback(uint32_t *held_buttons);
static void _print_callback(char *text);
static void _systick_callback(uint32_t systick);
static bool _post_user_tick_event(void);
static void _user_tick_event(const void *payload);
static bool _take_user_tick_pending(void);
static void _clear_user_tick_pending(void);
static void _notify_user_task(void);
static void _create_task(const t_knl_task_config *task);

/*----- Static constant definitions ----------------------------------*/

static const t_knl_task_config g_service_tasks[] = {
    // {svc_midi_task, "midi", MIDI_TASK_STACK_WORDS, MIDI_TASK_PRIORITY},
    {svc_panel_task, "panel", PANEL_TASK_STACK_WORDS, PANEL_TASK_PRIORITY},
    {svc_display_task, "display", DISPLAY_TASK_STACK_WORDS, DISPLAY_TASK_PRIORITY},
    {svc_dsp_task, "dsp", DSP_TASK_STACK_WORDS, DSP_TASK_PRIORITY},
//    {svc_usb_task, "usb", USB_TASK_STACK_WORDS, USB_TASK_PRIORITY},
    {svc_systick_task, "systick", SYSTICK_TASK_STACK_WORDS,
     SYSTICK_TASK_PRIORITY},
};

/*----- Extern function implementations ------------------------------*/

void knl_init(void) {

    _create_user_event_queue();
    _init_services();
    _register_service_callbacks();
    _create_service_tasks();
}

void knl_register_user_task(void) {

    g_user_task_handle = xTaskGetCurrentTaskHandle();
}

bool knl_post_user_event(t_knl_user_event_handler handler,
        const void *payload, uint32_t payload_size) {
    t_knl_user_event event = {0};

    if ((g_user_event_queue == NULL) ||
            (handler == NULL) ||
            (payload_size > USER_EVENT_PAYLOAD_SIZE)) {
        return false;
    }

    event.handler = handler;

    if ((payload != NULL) && (payload_size > 0)) {
        memcpy(event.payload, payload, payload_size);
    }

    if (xQueueSend(g_user_event_queue, &event, 0) != pdPASS) {
        return false;
    }

    _notify_user_task();

    return true;
}

void knl_user_event_process(void) {
    t_knl_user_event event;
    uint32_t events_processed = 0;

    if (g_user_event_queue == NULL) {
        return;
    }

    while ((events_processed < USER_EVENT_MAX_PER_PUMP) &&
            (xQueueReceive(g_user_event_queue, &event, 0) == pdPASS)) {
        if (event.handler != NULL) {
            event.handler(event.payload);
        }
        events_processed++;
    }
}

void knl_register_user_tick_callback(uint32_t divisor, void (*callback)(void)) {

    if (callback != NULL) {
        p_user_tick_callback = callback;
        g_user_tick_div = divisor;
    }
}


/*----- Static function implementations ------------------------------*/

static void _create_user_event_queue(void) {

    if (g_user_event_queue != NULL) {
        return;
    }

    g_user_event_queue =
        xQueueCreate(USER_EVENT_QUEUE_LENGTH, sizeof(t_knl_user_event));
    configASSERT(g_user_event_queue != NULL);
}

static void _init_services(void) {

    svc_system_init();

    // Initialise MIDI early to catch kernel print.
    // svc_midi_process();

    // @TODO: print initially using semihosting...?
    // svc_system_print("Welcome to Freetribe!\n");
    // svc_system_print("Hardware initialised.\n");
}

static void _register_service_callbacks(void) {

    svc_system_register_print_callback(_print_callback);
    svc_panel_register_callback(PANEL_ACK_EVENT, _panel_ack_callback);
    svc_panel_register_callback(HELD_BUTTONS_EVENT, _held_buttons_callback);
    svc_systick_register_callback(_systick_callback);
}

static void _create_service_tasks(void) {

    for (uint32_t i = 0; i < KNL_ARRAY_SIZE(g_service_tasks); i++) {
        _create_task(&g_service_tasks[i]);
    }
}

static void _panel_ack_callback(uint32_t version) {

    /// TODO: Store version with get method exposed to user.

    // Clear callback registration.
    svc_panel_register_callback(PANEL_ACK_EVENT, NULL);
}

static void _held_buttons_callback(uint32_t *held_buttons) {

    /// TODO: Store buttons with get method exposed to user.

    // Clear callback registration.
    svc_panel_register_callback(HELD_BUTTONS_EVENT, NULL);
}

static void _print_callback(char *text) { svc_midi_send_string(text); }

static void _systick_callback(uint32_t systick) {
    (void)systick;

    static uint32_t user_tick = 0;

    if (p_user_tick_callback == NULL) {
        return;
    }

    if (user_tick < g_user_tick_div) {
        user_tick++;
        return;
    }

    (void)_post_user_tick_event();
    user_tick = 0;
}

static bool _post_user_tick_event(void) {

    if (!_take_user_tick_pending()) {
        return false;
    }

    if (knl_post_user_event(_user_tick_event, NULL, 0)) {
        return true;
    }

    _clear_user_tick_pending();
    return false;
}

static void _user_tick_event(const void *payload) {
    (void)payload;

    if (p_user_tick_callback != NULL) {
        p_user_tick_callback();
    }

    _clear_user_tick_pending();
}

static bool _take_user_tick_pending(void) {

    bool post_tick = false;

    taskENTER_CRITICAL();
    if (!g_user_tick_pending) {
        g_user_tick_pending = true;
        post_tick = true;
    }
    taskEXIT_CRITICAL();

    return post_tick;
}

static void _clear_user_tick_pending(void) {

    taskENTER_CRITICAL();
    g_user_tick_pending = false;
    taskEXIT_CRITICAL();
}

static void _notify_user_task(void) {

    if (g_user_task_handle != NULL) {
        xTaskNotifyGive(g_user_task_handle);
    }
}

static void _create_task(const t_knl_task_config *task) {
    BaseType_t result;

    result = xTaskCreate(
            task->entry,
            task->name,
            task->stack_words,
            NULL,
            task->priority,
            NULL);
    configASSERT(result == pdPASS);
}

/*----- End of file --------------------------------------------------*/
