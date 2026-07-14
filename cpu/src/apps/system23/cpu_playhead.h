#ifndef CPU_PLAYHEAD_H
#define CPU_PLAYHEAD_H

#include <stdint.h>

typedef struct {
    uint32_t ticks_ms;
    uint32_t step;
} t_cpu_playhead;

t_cpu_playhead *cpu_playhead_fetch();
// uint32_t cpu_playhead_get_step();

#endif