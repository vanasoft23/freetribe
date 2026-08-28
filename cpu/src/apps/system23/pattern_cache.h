#ifndef PTN_CACHE_H
#define PTN_CACHE_H

#include "pattern.h"
#include "shared_mem.h"

extern u32 g_next_seq;

/*----- Functions ----------------------------------------------------*/

void ptn_cache_init();
bool ptn_cache_update();


static inline volatile t_pattern *ptn_readonly() {
	int idx = g_next_seq & 1;
	return &g_shared->ptn_ipc.ptns[idx];
}

static inline t_pattern *ptn() {
	int        idx = g_next_seq & 1;
	t_pattern *ptn = (t_pattern*)(void*)&g_shared->ptn_ipc.ptns[idx];
	return ptn;
}

#endif