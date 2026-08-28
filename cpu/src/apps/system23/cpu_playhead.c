#include "ft.h"

#include "cpu_playhead.h"
#include "shared_mem.h"

#define SAMPLERATE 48000

static t_cpu_playhead s_playhead;
static u32 s_step_idx;

/** @brief Convert frame position to milliseconds.  */
inline u32 frames_to_ms(u32 frame_pos) {
	return (frame_pos * 1000) / SAMPLERATE;
}

t_cpu_playhead *cpu_playhead_fetch() {

	t_dsp_playhead_state *src = (t_dsp_playhead_state*)(void*)&g_shared->dsp_playhead_state;
	t_dsp_playhead_state local;
	memcpy(&local, src, sizeof(t_dsp_playhead_state));
	if (local.start_seq != local.end_seq) {
		DLOG("Torn playhead read");
		return &s_playhead;
	}
	DLOG("PLAYHEAD FETCH SUCCESS");

	s_playhead.ticks_ms = frames_to_ms(local.frame_pos);
	s_playhead.step     = local.frame_pos * local.frames_per_step;

	return &s_playhead;
}


// u32 cpu_playhead_get_step() {
//     return s_step_idx;
// }
