/*----- Includes -----------------------------------------------------*/

#include "freetribe.h"
#include "macros.h"

#include "view.h"
#include "playback.h"
#include "lut.h"
#include "pattern_cache.h"
#include "cpu_playhead.h"


/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Initialise application.
 *
 * @return status   Status code indicating success:
 *                  - SUCCESS
 *                  - WARNING
 *                  - ERROR
 */
t_status app_init(void) {

    lut_init();
    
    ptn_cache_init();
    playback_init(180);
    view_init();
    view_select(VIEW_SEQUENCER);

    ft_print("System23 initialised.");
    return SUCCESS;
}

/**
 * @brief   Run application.
 */
void app_run(void) {

    view_draw();

    // static int timah;
    // if (timah++ % 15 == 0) {
    //     ft_printf("Sturen");
    //     if (IPC_QUEUE_FULL == dev_dsp_ipc_read(0x00000060, test_terug, 64, _test, (void*)0x23AC1D23)) {
    //         ft_printf("IPC_QUEUE_FULL");
    //     }
    //     // if (IPC_QUEUE_FULL == dev_dsp_ipc_transfer(0x00000060, (const uint32_t*)0xC0004000, 71, _test, (void*)0x23AC1D23)) {
    //     //     ft_printf("IPC_QUEUE_FULL");
    //     // }
    // }

}

// static uint32_t test_terug[64];
// static void _test(void *ctx, t_ipc_status status) {
//     if (IPC_FAILED == status) {
//         ft_printf("IPC_FAILED callback");
//         return;
//     }

//     ft_printf("terug gekregen: %08X", (uint32_t)ctx);
//     for (int32_t p = 0; p < 64; p += 8) {
//         ft_printf("%04X %04X %04X %04X %04X %04X %04X %04X",
//             *(volatile uint16_t*)(test_terug+p+0),
//             *(volatile uint16_t*)(test_terug+p+1),
//             *(volatile uint16_t*)(test_terug+p+2),
//             *(volatile uint16_t*)(test_terug+p+3),
//             *(volatile uint16_t*)(test_terug+p+4),
//             *(volatile uint16_t*)(test_terug+p+5),
//             *(volatile uint16_t*)(test_terug+p+6),
//             *(volatile uint16_t*)(test_terug+p+7)
//         );
//     }
// }


