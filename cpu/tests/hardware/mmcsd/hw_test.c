#include "hw_test.h"

#include <stdarg.h>

#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include "tinyprintf.h"

#define SEMIHOST_SYS_WRITE0 0x04u
#define SEMIHOST_BUFFER_SIZE 512u

static char s_semihost_buffer[SEMIHOST_BUFFER_SIZE];
static uint32_t s_tests_passed;
static uint32_t s_tests_failed;
static bool s_current_test_failed;

static void _semihost_write0(const char *text) {
    register uint32_t operation asm("r0") = SEMIHOST_SYS_WRITE0;
    register const char *parameter asm("r1") = text;

    asm volatile("swi 0x123456"
                 : "+r"(operation), "+r"(parameter)
                 :
                 : "memory", "cc");
}

void hw_test_printf(const char *format, ...) {
    va_list args;

    va_start(args, format);
    tfp_vsnprintf(s_semihost_buffer, sizeof(s_semihost_buffer), format, args);
    va_end(args);

    _semihost_write0(s_semihost_buffer);
}

void hw_test_begin(void) {
    s_tests_passed = 0;
    s_tests_failed = 0;
    s_current_test_failed = false;

    hw_test_printf("[==========] AM1802 MMC/SD hardware tests\n");
}

void hw_test_run(const char *name, hw_test_fn test) {
    s_current_test_failed = false;
    hw_test_printf("[ RUN      ] %s\n", name);

    test();

    if (s_current_test_failed) {
        s_tests_failed++;
        hw_test_printf("[  FAILED  ] %s\n", name);
        return;
    }

    s_tests_passed++;
    hw_test_printf("[       OK ] %s\n", name);
}

void hw_test_fail(const char *file, uint32_t line, const char *expression) {
    s_current_test_failed = true;
    hw_test_printf("             %s:%u: assertion failed: %s\n", file,
                   (unsigned int)line, expression);
}

void hw_test_fail_u32(const char *file, uint32_t line, uint32_t expected,
                      uint32_t actual) {
    s_current_test_failed = true;
    hw_test_printf(
        "             %s:%u: expected 0x%08X, actual 0x%08X\n", file,
        (unsigned int)line, (unsigned int)expected, (unsigned int)actual
    );
}

int hw_test_summary(void) {
    hw_test_printf(
        "[==========] %u passed, %u failed\n", (unsigned int)s_tests_passed,
        (unsigned int)s_tests_failed
    );

    return (int)s_tests_failed;
}
