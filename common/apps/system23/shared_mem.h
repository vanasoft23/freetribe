#ifndef SHARED_MEM_H
#define SHARED_MEM_H

#include <stdint.h>
#include <stdbool.h>
#include "pattern.h"

/*----- Macros -------------------------------------------------------*/

#define CPU_MEM_ADDR 0xC0000000
#define DSP_MEM_ADDR 0x00000000

/*----- Typedefs -----------------------------------------------------*/

typedef struct __attribute__((packed, aligned(4))) {
    
    // Double-buffered patterns
    t_pattern    patterns[2];
    uint32_t     pattern_ready;
    uint32_t     active_ptn_idx;
    uint32_t     inactive_ptn_idx;
    
} t_shared_mem;

#ifdef BLACKFIN
static volatile t_shared_mem *g_shared = (volatile t_shared_mem*)DSP_MEM_ADDR;
#else
static volatile t_shared_mem *g_shared = (volatile t_shared_mem*)CPU_MEM_ADDR;
#endif

#endif