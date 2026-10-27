#ifndef VIEW_H
#define VIEW_H

#include <stdint.h>

typedef enum {
	VIEW_PART_MUTE,
	VIEW_PART_ERASE,
	VIEW_STEP_EDIT,
	VIEW_SEQUENCER,
	VIEW_KEYBOARD,
	VIEW_CHORD,
	VIEW_STEP_JUMP,
	VIEW_PATTERN_SET,
	NUM_VIEWS
} e_view_id;

typedef struct t_view t_view;

typedef void (*fn_select)(t_view *);
typedef void (*fn_draw)(t_view *);
typedef void (*fn_handle_knob)(t_view *, u8 index, u8 value);
typedef void (*fn_handle_button)(t_view *, u8 index, bool state);
typedef void (*fn_handle_xy_pad)(t_view *, u8 index, bool state);
typedef void (*fn_handle_trigger)(t_view *, u8 pad, u8 vel, bool state);
typedef void (*fn_handle_encoder)(t_view *, u8 index, u8 value);

struct t_view {
	fn_draw               draw;
	fn_select             select;
	fn_handle_knob        handle_knob;
	fn_handle_button      handle_button;
	fn_handle_xy_pad      handle_xy_pad;
	fn_handle_trigger     handle_trigger;
	fn_handle_encoder     handle_encoder;
};

extern bool g_shift;
extern int  g_part_idx;
extern bool g_selected_ifx;

void view_init();
void view_select(int view_id);
void view_draw();


#endif