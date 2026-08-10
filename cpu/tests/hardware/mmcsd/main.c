#include <stdbool.h>

#include "per_gpio.h"
#include "per_pinmux.h"

#include "hw_test.h"


__attribute__((noinline, noreturn, used))
void mmcsd_hw_test_complete(int result);

static void _test_harness_smoke(void) {
    HW_TEST_ASSERT(true);
    HW_TEST_ASSERT_EQ_U32(0x1802u, 0x1802u);
}

static void _test_sd_card_present(void) {
    bool card_present = !per_gpio_get_indexed(PIN_EMA_A_16__MMCSD0_DAT5);

    HW_TEST_ASSERT(card_present);
}

static int _run_tests(void) {
    /* Apply the normal board pinmux and GPIO directions before card detect. */
    per_pinmux_init();
    per_gpio_init();

    hw_test_begin();
    HW_TEST_RUN(_test_harness_smoke);
    HW_TEST_RUN(_test_sd_card_present);

    return hw_test_summary();
}

int main(void) {
    return _run_tests();
}

/* Called by the shared AM1802 assembly entry after its DDR setup is complete. */
void start_boot(void) {
    mmcsd_hw_test_complete(main());
}

__attribute__((noinline, noreturn, used))
void mmcsd_hw_test_complete(int result) {
    hw_test_printf("[ COMPLETE ] result=%d; halted for debugger\n", result);

    for (;;) {
        asm volatile("nop");
    }
}
