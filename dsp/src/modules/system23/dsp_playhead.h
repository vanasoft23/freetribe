#ifndef DSP_PLAYHEAD_H
#define DSP_PLAYHEAD_H

typedef struct {
	uint32_t frame;
	uint32_t frames_per_step;
	uint32_t step_idx;
	bool     playing;
} t_playhead;

void dsp_playhead_playstop(bool play);
void dsp_playhead_set_pos(uint32_t pos);
void dsp_playhead_set_tempo(uint32_t bpm);
void dsp_playhead_advance(uint32_t frames);
void dsp_playhead_report();
bool dsp_playhead_should_trigger();

#endif