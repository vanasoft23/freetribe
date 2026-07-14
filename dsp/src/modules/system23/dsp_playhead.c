#include "dsp_common.h"
#include "dev_cpu_ipc.h"
#include "shared_mem.h"
#include "dsp_playhead.h"

#define PLAYHEAD_STATE ((t_dsp_playhead_state*)(void*)&g_shared->dsp_playhead_state)

static t_playhead s_playhead;
static uint32_t s_ipc_next_seq = 0;


/** @brief Convert frame position to step index. */
inline uint32_t step_to_frame(uint32_t step_num) {
    return s_playhead.frames_per_step * step_num;
}

inline uint32_t rounded_div32(uint32_t a, uint32_t b) {
    return (a + b / 2) / b;
}

/*----- Control Commands ---------------------------------------------*/

void dsp_playhead_playstop(bool play) {

    s_playhead.playing = play;

}

void dsp_playhead_set_pos(uint32_t step_num) {

    s_playhead.frame = step_to_frame(step_num);

}

void dsp_playhead_set_tempo(uint32_t bpm) {

    uint32_t frames_per_minute = SAMPLERATE * 60;
    s_playhead.frames_per_step = rounded_div32(frames_per_minute, bpm);

}

/*----- Operations ---------------------------------------------------*/

void dsp_playhead_advance(uint32_t frames) {

    s_playhead.frame += frames;
    
}


void dsp_playhead_report() {

    t_dsp_playhead_state src = {
        .start_seq       = s_ipc_next_seq,
        .frame_pos       = s_playhead.frame,
        .frames_per_step = s_playhead.frames_per_step,
        .end_seq         = s_ipc_next_seq
    };
    s_ipc_next_seq++;

    uint32_t dest = CPU_MEM + (uint32_t)PLAYHEAD_STATE - DSP_MEM;
    t_ipc_status st = dev_cpu_ipc_transfer(
        dest,
        (const uint32_t*)&src,
        sizeof(t_dsp_playhead_state) / 4,
        NULL,
        NULL
    );
    // @TODO: error check

}

/*----- Getters ------------------------------------------------------*/

bool dsp_playhead_should_trigger() {

    return (s_playhead.frame == 0);

}

