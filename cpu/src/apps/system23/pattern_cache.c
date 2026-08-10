/*----- Includes -----------------------------------------------------*/

#include <csl_cp15.h>

#include "ft.h"
#include "pattern.h"
#include "shared_mem.h"

#include "freetribe.h"
#include "dev_dsp_ipc.h"

#include "pattern_cache.h"


/*----- Macros -------------------------------------------------------*/

#ifdef DEBUG
static t_step s_initial_part0_steps[64] = {
    { GATE_FULL , 58  + 0, false, false },
    { GATE_QTER , 58 + 0, false, false },
    { GATE_QTER , 58 + 1, false, true  },
    { GATE_FULL , 58 + 1, false, false },
    { GATE_FULL , 58 + 0, false, false },
    { GATE_HALF , 58 + 0, false, false },
    { GATE_FULL , 58 +12, false, false },
    { GATE_FULL , 58 +12, false, false },
    { GATE_FULL , 58 -12, false, false },
    { GATE_FULL , 58 -12, false, false },
    { GATE_HALF , 58 + 1, false, false },
    { GATE_HALF , 58 + 1, false, false },
    { GATE_QTER , 58 +12, false, false },
    { GATE_QTER , 58 +13, false, false },
    { GATE_FULL , 58 +10, false, false },
    { GATE_HALF , 58 +10, false, false },
};
#endif

/*----- Extern variable declarations ---------------------------------*/

u32 g_next_seq     = 1;

/*----- Static variable declarations ---------------------------------*/

static u32 s_last_ack_seq = 0;

/*----- Local function declarations ----------------------------------*/

static void _init_debug_pattern();
static void _handle_acknowledgements();
static bool _upload_pattern();
static void _on_upload_done(void *ctx, t_ipc_status st);

/*----- Global function definitions ----------------------------------*/

void ptn_cache_init() {
    
#ifdef DEBUG
    _init_debug_pattern();
#endif

}

bool ptn_cache_update() {

    _handle_acknowledgements();
    return _upload_pattern();

}

/*----- Local function definitions -----------------------------------*/

#ifdef DEBUG
static void _init_debug_pattern() {

    volatile t_step *dst = ptn()->parts[0].steps;
    memcpy((void*)dst, s_initial_part0_steps, sizeof(s_initial_part0_steps));

}
#endif

/** @brief Handle acknowledge signal sent by DSP first. */
static void _handle_acknowledgements() {

    t_pattern_ack *ack = (t_pattern_ack*)(void*)&g_shared->ptn_ipc.ack;
    t_pattern_ack local_ack;
    
    memcpy(&local_ack, ack, sizeof(local_ack));

    if (local_ack.seq > s_last_ack_seq)
        s_last_ack_seq = local_ack.seq;
    
}

static bool _upload_pattern() {

    int             idx = g_next_seq & 1;
    t_pattern      *ptn = (t_pattern*)(void*)&g_shared->ptn_ipc.ptns[idx];

    u32        dst = DSP_MEM + (u32)ptn - CPU_MEM;
    const u32 *src = (const u32*)ptn;
    int      word_count = sizeof(t_pattern);

    CP15DCacheCleanBuff((u32)src, sizeof(t_pattern));

    t_ipc_status st = dev_dsp_ipc_transfer(
        dst,
        src,
        word_count,
        _on_upload_done,
        NULL
    );

    if (st != IPC_SUCCESS) {
        // @TODO: handle error
        return false;
    }

    g_next_seq++;
    return true;
}

static void _on_upload_done(void *ctx, t_ipc_status st) {
    
    if (st != IPC_SUCCESS) {
        DEBUG_LOG("upload failed");
        return;
    }

    

}

