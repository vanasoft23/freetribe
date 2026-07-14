#include "shared_mem.h"
#include "dsp_common.h"
#include "dev_cpu_ipc.h"
#include "pattern_cache.h"

static uint32_t g_last_seq = 0;
static t_pattern *g_last_ptn = NULL; // @TODO: default pattern? or just check null


static void _on_ack_done(void *ctx, t_ipc_status st);


void ptn_cache_init() {

}

t_pattern *ptn_cache_fetch_latest() {

    t_pattern_ipc *ipc = (t_pattern_ipc*)(void*)&g_shared->ptn_ipc;
    
    uint32_t seq_0 = ipc->ptns[0].seq;
    uint32_t seq_1 = ipc->ptns[1].seq;
    uint32_t latest_seq;
    int      latest_idx;

    if (seq_0 > g_last_seq) {
        latest_seq = seq_0;
        latest_idx = 0;
    } else if (seq_1 > g_last_seq) {
        latest_seq = seq_1;
        latest_idx = 1;
    } else {
        return g_last_ptn; // nothing new under the sun
    }

    t_pattern *ptn = &ipc->ptns[latest_idx];
    ipc->ack.idx = latest_idx;
    ipc->ack.seq = latest_seq;

    // If DSP has caches, ensure they are clean/visible to DMA:
    // @FIXME: dsp_cache_clean(&ack, sizeof(ack));
    
    uint32_t        dst = CPU_MEM + (uint32_t)&ipc->ack - DSP_MEM;
    const uint32_t *src = (const void*)&ipc->ack;
    uint32_t word_count = sizeof(t_pattern_ack) / 4;

    t_ipc_status st = dev_cpu_ipc_transfer(
        dst,
        src,
        word_count,
        _on_ack_done,
        NULL
    );
    if (st != IPC_SUCCESS) {
        // @FIXME: handle error
        return ptn;
    }

    g_last_seq = ptn->seq;
    
    return ptn;
}



static void _on_ack_done(void *ctx, t_ipc_status st) {
    if (st != IPC_SUCCESS) {
        // @FIXME: MUST RETRY!!!!!!!!
    }
}

