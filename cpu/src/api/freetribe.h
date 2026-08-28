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

/// TODO: This should expose only those functions required by user applications.
///       For now we just include the service layer directly.
//
/**
 * @file    freetribe.h
 *
 * @brief   Public API for Freetribe application library.
 */

#ifndef FREETRIBE_H
#define FREETRIBE_H

#ifdef __cplusplus
extern "C" {
#endif
  
/*----- Includes -----------------------------------------------------*/

// Internal freetribe headers
#include "ft_types.h"
// #include "ft_macros.h"
// #include "ft_log.h"
// #include "ft_error.h"

#include <midi_fsm.h>

#include "svc_clock.h"
#include "svc_delay.h"
#include "svc_display.h"
#include "svc_dsp.h"
#include "svc_midi.h"
#include "svc_panel.h"
#include "svc_sysex.h"
#include "svc_system.h"
#include "svc_systick.h"

#include "knl_main.h"
#include "usr_main.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable declarations ---------------------------------*/

/*----- Extern function prototypes -----------------------------------*/

void ft_register_tick_callback(u32 divisor, void (*callback)(void));

bool ft_delay(t_delay_state *state);
void ft_start_delay(t_delay_state *state, u32 time);

void ft_put_pixel(u16 pos_x, u16 pos_y, bool state);

s8 ft_fill_frame(
	u16 x_start, u16 y_start, u16 x_end,
	u16 y_end, bool state
);

void ft_register_print_callback(void (*callback)(char *));
void ft_print(char *text);
void ft_printf(const char *format, ...);

void ft_register_midi_callback(
        event_type event,
	t_midi_event_callback callback
);

void ft_send_note_on(char chan, char note, char vel);
void ft_send_note_off(char chan, char note, char vel);
void ft_send_cc(char chan, char index, char val);

/// TODO: Cast void pointers to typedef function pointers.
//
void ft_register_panel_callback(t_panel_event event, void *callback);
void ft_toggle_led(t_led_index led_index);
void ft_set_led(t_led_index led_index, bool state);
void ft_set_trigger_mode(u8 mode);

void ft_set_module_param(u16 module_id, u16 param_index,
						 s32 param_value);

void ft_get_module_param(u16 module_id, u16 param_index);

void ft_register_dsp_callback(u8 msg_type, u8 msg_id, void *callback);

void ft_shutdown(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
