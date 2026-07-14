#include "freetribe.h"
#include "view.h"
#include "view_sequencer.h"
#include "view_step_edit.h"
#include "view_keyboard.h"
#include "view_part_mute.h"
#include "inputs.h"

extern t_sequencer g_sequencer;
// extern t_step_edit g_step_edit;
// extern t_keyboard  g_keyboard;
// extern t_part_mute g_part_mute;

t_view *g_views[NUM_VIEWS] = {
    [VIEW_SEQUENCER]   = (t_view*)&g_sequencer,
    // [VIEW_STEP_EDIT]   = (t_view*)&g_step_edit,
    // [VIEW_KEYBOARD]    = (t_view*)&g_keyboard,
    // [VIEW_PART_MUTE]   = (t_view*)&g_part_mute,
};

t_view   *g_active_view;
bool      g_shift;
int       g_part_idx;
bool      g_selected_ifx;

/*----- Static function prototypes -----------------------------------*/

static void handle_knob(uint8_t index, uint8_t value);
static void handle_button(uint8_t index, bool state);
static void handle_xy_pad(uint8_t x_val, uint8_t y_val);
static void handle_trigger(uint8_t pad, uint8_t vel, bool state);
static void handle_encoder(uint8_t index, uint8_t value);

/*----- Extern function implementations ------------------------------*/

void view_init() {
    ft_register_panel_callback(KNOB_EVENT, handle_knob);
    ft_register_panel_callback(XY_PAD_EVENT, handle_xy_pad);
    ft_register_panel_callback(BUTTON_EVENT, handle_button);
    ft_register_panel_callback(TRIGGER_EVENT, handle_trigger);
    ft_register_panel_callback(ENCODER_EVENT, handle_encoder);
}

void view_select(int id) {
    g_active_view = g_views[id];
    g_active_view->select(g_active_view);
}

void view_draw() {
    g_active_view->draw(g_active_view);
}


void handle_knob(uint8_t index, uint8_t value) {
    g_active_view->handle_knob(g_active_view, index, value);
}

void handle_button(uint8_t index, bool state) {
    switch (index) {

        case BUTTON_PART_PREV: {
            if (state) g_part_idx = (g_part_idx - 1) % 0xF;
        } break;

        case BUTTON_PART_NEXT: {
            if (state) g_part_idx = (g_part_idx + 1) % 0xF;
        } break;

        case BUTTON_SHIFT: {
            g_shift = state;
        } break;

    }
    g_active_view->handle_button(g_active_view, index, state);
}

void handle_xy_pad(uint8_t x_val, uint8_t y_val) {
    g_active_view->handle_xy_pad(g_active_view, x_val, y_val);
}

void handle_trigger(uint8_t pad, uint8_t vel, bool state) {
    g_active_view->handle_trigger(g_active_view, pad, vel, state);
}

void handle_encoder(uint8_t index, uint8_t value) {
    g_active_view->handle_encoder(g_active_view, index, value);
}

