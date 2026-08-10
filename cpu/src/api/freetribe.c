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
 * @file    freetribe.c
 *
 * @brief   Freetribe application library.
 *
 * A set of wrappers for user applications to access kernel functions.
 */

/// TODO: These should all be inline in freetribe.h.

/*----- Includes -----------------------------------------------------*/

/////// TODO: stdio
#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include "tinyprintf.h"
///////

#include "freetribe.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
    event_type event;
    char chan;
    char data1;
    char data2;
} t_midi_user_event;

/*----- Static variable definitions ----------------------------------*/

static t_midi_event_callback p_midi_callbacks[EVT_MAX] = {0};

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _midi_user_event_dispatch(const void *payload);
static void _post_midi_user_event(event_type event, char chan, char data1,
                                  char data2);
static void _midi_timing_clock_callback(char chan, char data1, char data2);
static void _midi_reserved_f9_callback(char chan, char data1, char data2);
static void _midi_seq_start_callback(char chan, char data1, char data2);
static void _midi_seq_continue_callback(char chan, char data1, char data2);
static void _midi_seq_stop_callback(char chan, char data1, char data2);
static void _midi_reserved_fd_callback(char chan, char data1, char data2);
static void _midi_active_sense_callback(char chan, char data1, char data2);
static void _midi_reset_callback(char chan, char data1, char data2);
static void _midi_note_off_callback(char chan, char data1, char data2);
static void _midi_note_on_callback(char chan, char data1, char data2);
static void _midi_poly_aftertouch_callback(char chan, char data1, char data2);
static void _midi_control_change_callback(char chan, char data1, char data2);
static void _midi_program_change_callback(char chan, char data1, char data2);
static void _midi_aftertouch_callback(char chan, char data1, char data2);
static void _midi_pitch_bend_callback(char chan, char data1, char data2);

static t_midi_event_callback _midi_wrapper_for_event(event_type event);

/*----- Extern function implementations ------------------------------*/

// Tick API
//
/**
 * @brief   Register a callback for userspace tick events.
 *
 * The kernel systick triggers once per millisecond.
 * Set the divisor greater than 0 to trigger the user tick callback less often.
 *
 * @param[in]   divisor     Ratio of kernel ticks to user ticks.
 * @param[in]   callback    Function to call.
 */
void ft_register_tick_callback(u32 divisor, void (*callback)(void)) {

    knl_register_user_tick_callback(divisor, callback);
}

// Delay API
//

/**
 * @brief   Non-blocking delay initialisation.
 *
 * Initialises state of delay to zero.
 *
 * @param[out]  state   State of delay.
 * @param[in]   time    Time in microseconds.
 *
 */
void ft_start_delay(t_delay_state *state, u32 time) {

    delay_start(state, time);
}

/**
 * @brief   Non-blocking delay.
 *
 *  Define a struct of type `t_delay_state`, then pass it to ft_start_delay().
 *
 * @param[out]  state   State of delay.
 *
 * @return      True if at least `state->delay_time` microseconds
 *              have passed since `state->start_time`.
 */
bool ft_delay(t_delay_state *state) { return delay_us(state); }

// Display API
//
//
void ft_put_pixel(u16 pos_x, u16 pos_y, bool state) {

    svc_display_put_pixel(pos_x, pos_y, state);
}

s8 ft_fill_frame(u16 x_start, u16 y_start, u16 x_end,
                     u16 y_end, bool state) {
    return svc_display_fill_frame(x_start, y_start, x_end, y_end, state);
}

// Print API
//
/// TODO: What is going on with print?
//
void ft_register_print_callback(void (*callback)(char *)) {

    svc_system_register_print_callback(callback);
}

/**
 * @brief   Print ASCII string as MIDI system exclusive message.
 *
 * String will be encapsulated with 0xf7...0xf0
 * to ensure it passes through receiving MIDI drivers.
 *
 * @param[in]   text    String to be printed.
 *
 */
void ft_print(char *text) { svc_midi_send_string(text); }

/**
 * @brief   Literally printf girl
 */
void ft_printf(const char *format, ...)
{
    va_list ap;
    static char str[256];

    va_start(ap, format);
    tfp_vsprintf(str, format, ap);
    svc_midi_send_string(str);
    va_end(ap);
}

// Panel API
//
/**
 * @brief   Register a callback for panel control input events.
 *
 * @param[in]   event       Type of event to catch.
 * @param[in]   callback    Function to call.
 *
 */
/// TODO: Separate function for each event type.
//
void ft_register_panel_callback(t_panel_event event, void *callback) {

    svc_panel_register_callback(event, callback);
}

void ft_set_trigger_mode(u8 mode) { svc_panel_set_trigger_mode(mode); }

// MIDI API
//
/**
 * @brief   Register a callback MIDI input events.
 *
 * @param[in]   event       Type of event to catch.
 * @param[in]   callback    Function to call.
 *
 */
/// TODO: Separate function for each event type.
//
void ft_register_midi_callback(event_type event,
                               t_midi_event_callback callback) {

    if ((event < 0) || (event >= EVT_MAX)) {
        return;
    }

    p_midi_callbacks[event] = callback;
    midi_register_event_handler(event,
                                callback ? _midi_wrapper_for_event(event)
                                         : NULL);
}

/**
 * @brief   Send MIDI note on message.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 *
 */
void ft_send_note_on(char chan, char note, char vel) {

    svc_midi_send_note_on(chan, note, vel);
}

/**
 * @brief   Send MIDI note off message.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   note    MIDI note number.
 * @param[in]   vel     MIDI note velocity.
 *
 */
void ft_send_note_off(char chan, char note, char vel) {

    svc_midi_send_note_off(chan, note, vel);
}

/**
 * @brief   Send MIDI control change message.
 *
 * @param[in]   chan    MIDI channel.
 * @param[in]   index   MIDI CC number.
 * @param[in]   val     MIDI CC value.
 *
 */
void ft_send_cc(char chan, char index, char val) {

    svc_midi_send_cc(chan, index, val);
}

// LED API
//
/**
 * @brief   Toggle LED into the opposite state.
 *
 * @param[in]   led_index   Index of LED to toggle.
 *
 */
void ft_toggle_led(t_led_index led_index) { svc_panel_toggle_led(led_index); }

/**
 * @brief   Set LED.
 *
 * @param[in]   led_index   Index of LED to toggle.
 *
 */
void ft_set_led(t_led_index led_index, bool state) {
    svc_panel_set_led(led_index, state);
}

// DSP Command API
//
/**
 * @brief   Set parameter value in DSP audio module.
 *
 * @param[in]   module_id   Index of module to address.
 * @param[in]   param_index Index of parameter to set.
 * @param[in]   param_value Value of parameter.
 *
 */
void ft_set_module_param(u16 module_id, u16 param_index,
                         s32 param_value) {

    svc_dsp_set_module_param(module_id, param_index, param_value);
}

/**
 * @brief   Request parameter value from DSP audio module.
 *
 * First register a callback for MODULE_PARAM_VALUE event,
 * then call this to request the data from the DSP.
 *
 * @param[in]   module_id   Index of module to address.
 * @param[in]   param_index Index of parameter to get.
 *
 */
void ft_get_module_param(u16 module_id, u16 param_index) {

    svc_dsp_get_module_param(module_id, param_index);
}

void ft_register_dsp_callback(u8 msg_type, u8 msg_id,
                              void *callback) {

    svc_dsp_register_callback(msg_type, msg_id, callback);
}

/**
 * @brief   Shutdown the system.
 */
void ft_shutdown(void) { svc_system_shutdown(); }

/*----- Static function implementations ------------------------------*/

#define MIDI_USER_CALLBACK(name, event_id)                                      \
    static void name(char chan, char data1, char data2) {                       \
        _post_midi_user_event(event_id, chan, data1, data2);                    \
    }

MIDI_USER_CALLBACK(_midi_timing_clock_callback, EVT_SYS_REALTIME_TIMING_CLOCK)
MIDI_USER_CALLBACK(_midi_reserved_f9_callback, EVT_SYS_REALTIME_RESERVED_F9)
MIDI_USER_CALLBACK(_midi_seq_start_callback, EVT_SYS_REALTIME_SEQ_START)
MIDI_USER_CALLBACK(_midi_seq_continue_callback, EVT_SYS_REALTIME_SEQ_CONTINUE)
MIDI_USER_CALLBACK(_midi_seq_stop_callback, EVT_SYS_REALTIME_SEQ_STOP)
MIDI_USER_CALLBACK(_midi_reserved_fd_callback, EVT_SYS_REALTIME_RESERVED_FD)
MIDI_USER_CALLBACK(_midi_active_sense_callback, EVT_SYS_REALTIME_ACTIVE_SENSE)
MIDI_USER_CALLBACK(_midi_reset_callback, EVT_SYS_REALTIME_RESET)
MIDI_USER_CALLBACK(_midi_note_off_callback, EVT_CHAN_NOTE_OFF)
MIDI_USER_CALLBACK(_midi_note_on_callback, EVT_CHAN_NOTE_ON)
MIDI_USER_CALLBACK(_midi_poly_aftertouch_callback, EVT_CHAN_POLY_AFTERTOUCH)
MIDI_USER_CALLBACK(_midi_control_change_callback, EVT_CHAN_CONTROL_CHANGE)
MIDI_USER_CALLBACK(_midi_program_change_callback, EVT_CHAN_PROGRAM_CHANGE)
MIDI_USER_CALLBACK(_midi_aftertouch_callback, EVT_CHAN_AFTERTOUCH)
MIDI_USER_CALLBACK(_midi_pitch_bend_callback, EVT_CHAN_PITCH_BEND)

static void _midi_user_event_dispatch(const void *payload) {
    const t_midi_user_event *event = (const t_midi_user_event *)payload;

    if ((event->event < 0) || (event->event >= EVT_MAX)) {
        return;
    }

    if (p_midi_callbacks[event->event] != NULL) {
        p_midi_callbacks[event->event](event->chan, event->data1,
                                       event->data2);
    }
}

static void _post_midi_user_event(event_type event, char chan, char data1,
                                  char data2) {
    t_midi_user_event user_event = {
        .event = event, .chan = chan, .data1 = data1, .data2 = data2};

    if ((event < 0) || (event >= EVT_MAX) ||
        (p_midi_callbacks[event] == NULL)) {
        return;
    }

    (void)knl_post_user_event(_midi_user_event_dispatch, &user_event,
                              sizeof(user_event));
}

static t_midi_event_callback _midi_wrapper_for_event(event_type event) {
    static const t_midi_event_callback wrappers[EVT_MAX] = {
        _midi_timing_clock_callback,
        _midi_reserved_f9_callback,
        _midi_seq_start_callback,
        _midi_seq_continue_callback,
        _midi_seq_stop_callback,
        _midi_reserved_fd_callback,
        _midi_active_sense_callback,
        _midi_reset_callback,
        _midi_note_off_callback,
        _midi_note_on_callback,
        _midi_poly_aftertouch_callback,
        _midi_control_change_callback,
        _midi_program_change_callback,
        _midi_aftertouch_callback,
        _midi_pitch_bend_callback,
    };

    if ((event < 0) || (event >= EVT_MAX)) {
        return NULL;
    }

    return wrappers[event];
}

/*----- End of file --------------------------------------------------*/
