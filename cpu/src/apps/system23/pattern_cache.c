/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "macros.h"

#include "pattern.h"
#include "shared_mem.h"
#include <string.h>

#include "dev_dsp_ipc.h"

/*----- Macros -------------------------------------------------------*/

static t_step s_initial_part0_steps[64] = {
    { GATE_FULL , 58 + 0, false, false },
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

/*----- Local function declarations ----------------------------------*/

static void _init_debug_pattern();

/*----- Global function definitions ----------------------------------*/

// bool pattern_upload(const t_pattern *src);


void ptn_cache_init() {
    _init_debug_pattern();
}


bool ptn_cache_upload(const t_pattern *src) {
    memcpy(ptn_writable(), src, sizeof(t_pattern));

    // @TODO: Compute CRC32

    g_shared->pattern_ready = true; 

    // dev_dsp_ipc_read

}

/*----- Local function definitions -----------------------------------*/

static void _init_debug_pattern() {
    volatile t_step *dst = ptn_ptr()->parts[0].steps;
    memcpy((void*)dst, s_initial_part0_steps, sizeof(s_initial_part0_steps));
}
