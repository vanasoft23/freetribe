#include "ft.h"
#include "freetribe.h"

#include "view.h"
#include "view_sequencer.h"

#include "param_defs.h"
#include "param_scale.h"
#include "lut.h"
#include "inputs.h"
#include "pattern.h"
#include "pattern_cache.h"
#include "shared_mem.h"
#include "cpu_playhead.h"

/*----- Macros -------------------------------------------------------*/

/*----- Typedefs -----------------------------------------------------*/

struct t_sequencer {
    t_view   base;
    int      bar;
    int      selected_bar;
};

/*----- Static function prototypes -----------------------------------*/

static void select(t_sequencer *seq);
static void draw(t_sequencer *seq);
static void handle_knob(t_sequencer *seq, u8 index, u8 value);
static void handle_button(t_sequencer *seq, u8 index, bool state);
static void handle_xy_pad(t_sequencer *seq, u32 x_val, u32 y_val);
static void handle_trigger(t_sequencer *seq, u8 pad, u8 vel, bool state);
static void handle_encoder(t_sequencer *seq, u8 index, u8 value);

/*----- Variables ----------------------------------------------------*/

t_sequencer g_sequencer = {
    .base.draw           = (fn_draw)draw,
    .base.select         = (fn_select)select,
    .base.handle_knob    = (fn_handle_knob)handle_knob,
    .base.handle_button  = (fn_handle_button)handle_button,
    .base.handle_xy_pad  = (fn_handle_xy_pad)handle_xy_pad,
    .base.handle_trigger = (fn_handle_trigger)handle_trigger,
    .base.handle_encoder = (fn_handle_encoder)handle_encoder,
    .bar                 = 0,
    .selected_bar        = 0
};

/*----- Extern function implementations ------------------------------*/

static void select(t_sequencer *seq) {

}

static void draw(t_sequencer *seq) {

    t_cpu_playhead *phd = cpu_playhead_fetch();

    // step in pad
    if (seq->bar == seq->selected_bar) {
        for (u8 i = 0; i <= 15; i++) {
            if (i != phd->step)
                ft_set_led(LED_PAD_0_BLUE + (2*i), false);
        }
        ft_set_led(LED_PAD_0_BLUE + (2*phd->step), true);
    } else {
        for (u8 i = 0; i <= 15; i++)
            ft_set_led(LED_PAD_0_BLUE + (2*i), false);
    }

    // draw steps
    for (u8 i = 0; i <= 15; i++) {
        int  step_idx   = 16 * seq->selected_bar + i;
        bool has_note   = pattern_has_note(ptn_readonly(), g_part_idx, step_idx);
        ft_set_led(LED_PAD_0_RED + (2*i), has_note);
    }

    // bars
    for (u8 i = 0; i <= 3; i++) {
        ft_set_led(LED_BAR_0_BLUE + i, (i == seq->bar));
    }
    for (u8 i = 0; i <= 3; i++) {
        ft_set_led(LED_BAR_0_RED + i, (i == seq->selected_bar));
    }

}



static void handle_knob(t_sequencer *seq, u8 index, u8 value) {

    switch (index) {
        
        case KNOB_RESONANCE: {
            ft_set_module_param(0, PARAM_RESONANCE, g_resonance_lut[value]);
        } break;

        case KNOB_IFX_EDIT: {
            u16 param_index = g_selected_ifx
                ? (g_shift ? PARAM_IFX1_PARAM1 : PARAM_IFX1_PARAM0)
                : (g_shift ? PARAM_IFX0_PARAM1 : PARAM_IFX0_PARAM0);
            DEBUG_LOG("knob %i", (int)param_index);
            ft_set_module_param(0, param_index, u8o_q31(value));
        }

    }
}

static void handle_button(t_sequencer *seq, u8 index, bool state) {
    
    switch (index) {

        case BUTTON_EXIT: ft_shutdown(); break;
        case BUTTON_BAR_0: seq->selected_bar = 0; break;
        case BUTTON_BAR_1: seq->selected_bar = 1; break;
        case BUTTON_BAR_2: seq->selected_bar = 2; break;
        case BUTTON_BAR_3: seq->selected_bar = 3; break;
        case BUTTON_IFX_ON: {
            if (g_shift) {
                g_selected_ifx = g_shift;
            }
        } break;

        case BUTTON_PLAY: {
            ft_set_module_param(0, TRANSPORT_PLAY, 0);
        } break;
        case BUTTON_STOP: {
            ft_set_module_param(0, TRANSPORT_STOP, 0);
        } break;
        
    }

}

static void handle_xy_pad(t_sequencer *seq, u32 x_val, u32 y_val) {

}

static void handle_trigger(t_sequencer *seq, u8 pad, u8 vel, bool state) {

    // DEBUG_LOG("trigger callback: %02X %u %u", pad, vel, state);
    
    int step_index = pad + (seq->selected_bar * 16);
    if (state)
        pattern_place_note(ptn(), g_part_idx, step_index);
    else
        pattern_remove_note(ptn(), g_part_idx, step_index);
    ptn_cache_update();
    
}

static void handle_encoder(t_sequencer *seq, u8 index, u8 value) {

    // DEBUG_LOG("encoder callback: %02X %f", cutoff, note_to_cv(cutoff));

    static u8 cutoff = 0x7f;

    switch (index) {

        case ENCODER_CUTOFF: {
            if (value == 0x01) {
                if (cutoff < 0x7f)
                    cutoff++;
            } else {
                if (cutoff > 0)
                    cutoff--;
            }
            ft_set_module_param(0, PARAM_CUTOFF, g_cutoff_lut[cutoff]);
        } break;

    }

}
