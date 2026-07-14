#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stdbool.h>
#include "pattern.h"
#include "macros.h"

/*----- Macros -------------------------------------------------------*/

#define CPU_MEM 0xC0000000U
#define DSP_MEM 0x00000000U

/*----- Typedefs -----------------------------------------------------*/

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t       idx;
    uint32_t       seq;
} t_pattern_ack;

typedef struct __attribute__((packed, aligned(4))) {
    t_pattern      ptns[2];
    t_pattern_ack  ack;
} t_pattern_ipc;

typedef struct __attribute__((packed, aligned(4))) {
    uint32_t       start_seq;
    uint32_t       frame_pos;
    uint32_t       frames_per_step;
    uint32_t       end_seq;
} t_dsp_playhead_state;

typedef struct __attribute__((packed, aligned(4))) {
    t_pattern_ipc         ptn_ipc;
    t_dsp_playhead_state  dsp_playhead_state;
} t_shared_mem;

// #ifdef BLACKFIN
// STATIC_ASSERT((sizeof(t_shared_mem) % 4) == 0, shared_mem_multiple_of_4)
// STATIC_ASSERT((sizeof(t_shared_mem) % 4) == 0, pattern_ipc_multiple_of_4)
// #endif

#ifdef BLACKFIN
static volatile t_shared_mem *g_shared = (volatile t_shared_mem*)DSP_MEM;
#else
static volatile t_shared_mem *g_shared = (volatile t_shared_mem*)CPU_MEM;
#endif

#endif