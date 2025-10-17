#ifndef PTN_CACHE_H
#define PTN_CACHE_H

#include "pattern.h"
#include "shared_mem.h"

/*----- Functions ----------------------------------------------------*/

void ptn_cache_init();
bool ptn_cache_upload(const t_pattern *src);

static inline volatile t_pattern* ptn_volatile() {
    return &g_shared->patterns[g_shared->inactive_ptn_idx];
}

static inline t_pattern* ptn_writable() {
    return (t_pattern*)(void*)&g_shared->patterns[g_shared->inactive_ptn_idx];
}


#endif