// #include "view.h"
// #include "view_keyboard.h"

// struct t_keyboard {
//     t_view   base;
// };


// static void select(t_keyboard *kb);
// static void draw(t_keyboard *kb);
// static void handle_knob(t_keyboard *kb, u8 index, u8 value);
// static void handle_button(t_keyboard *kb, u8 index, bool state);
// static void handle_xy_pad(t_keyboard *kb, u32 x_val, u32 y_val);
// static void handle_trigger(t_keyboard *kb, u8 pad, u8 vel, bool state);
// static void handle_encoder(t_keyboard *kb, u8 index, u8 value);

// t_keyboard g_keyboard = {
//     .base.draw           = (fn_draw)draw,
//     .base.select         = (fn_select)select,
//     .base.handle_knob    = (fn_handle_knob)handle_knob,
//     .base.handle_button  = (fn_handle_button)handle_button,
//     .base.handle_xy_pad  = (fn_handle_xy_pad)handle_xy_pad,
//     .base.handle_trigger = (fn_handle_trigger)handle_trigger,
//     .base.handle_encoder = (fn_handle_encoder)handle_encoder,
//     .bar                 = 0,
//     .step                = 0,
//     .selected_bar        = 0
// };


// static void select(t_keyboard *seq) {

// }

// static void draw(t_keyboard *seq) {


// }





// static void handle_knob(t_keyboard *kb, u8 index, u8 value) {

//     switch (index) {
		
//         case KNOB_RESONANCE: {
//             ft_set_module_param(0, PARAM_RESONANCE, g_resonance_lut[value]);
//         } break;

//         case KNOB_IFX_EDIT: {
//             u16 param_index = g_selected_ifx
//                 ? (g_shift ? PARAM_IFX1_PARAM1 : PARAM_IFX1_PARAM0)
//                 : (g_shift ? PARAM_IFX0_PARAM1 : PARAM_IFX0_PARAM0);
//             DLOG("knob %i", (int)param_index);
//             ft_set_module_param(0, param_index, u8o_q31(value));
//         }

//     }
// }

// static void handle_button(t_keyboard *kb, u8 index, bool state) {
	
//     switch (index) {
//         case BUTTON_EXIT: ft_shutdown(); break;
//         case BUTTON_BAR_0: g_selected_bar = 0; break;
//         case BUTTON_BAR_1: g_selected_bar = 1; break;
//         case BUTTON_BAR_2: g_selected_bar = 2; break;
//         case BUTTON_BAR_3: g_selected_bar = 3; break;
//         case BUTTON_SHIFT: g_shift = state; break;
//         case BUTTON_IFX_ON: {
//             if (g_shift) {
//                 g_selected_ifx = g_shift;
//             }
//         } break;
//     }

// }

// static void handle_xy_pad(t_keyboard *kb, u32 x_val, u32 y_val) {

// }

// static void handle_trigger(t_keyboard *kb, u8 pad, u8 vel, bool state) {

//     // DLOG("trigger callback: %02X %u %u", pad, vel, state);

//     u8 step_index = pad + (g_selected_bar * 16);
//     if (g_keyboard_steps[step_index].gate > 0) {
//         g_keyboard_steps[step_index].gate = 0; // note is off
//     } else {
//         g_keyboard_steps[step_index].gate = GATE_4TH; // note is on
//     }

// }

// static void handle_encoder(t_keyboard *kb, u8 index, u8 value) {

//     // DLOG("encoder callback: %02X %f", cutoff, note_to_cv(cutoff));

//     static u8 cutoff = 0x7f;

//     switch (index) {

//         case ENCODER_CUTOFF: {
//             if (value == 0x01) {
//                 if (cutoff < 0x7f)
//                     cutoff++;
//             } else {
//                 if (cutoff > 0)
//                     cutoff--;
//             }
//             ft_set_module_param(0, PARAM_CUTOFF, g_cutoff_lut[cutoff]);
//         } break;

//     }

// }
