#ifndef CPU_PLAYHEAD_H
#define CPU_PLAYHEAD_H

#include <stdint.h>

typedef struct {
	u32 ticks_ms;
	u32 step;
} t_cpu_playhead;

t_cpu_playhead *cpu_playhead_fetch();
// u32 cpu_playhead_get_step();

#endif