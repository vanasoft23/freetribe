#ifndef MMCSD_HW_TEST_H
#define MMCSD_HW_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*hw_test_fn)(void);

void hw_test_begin(void);
void hw_test_run(const char *name, hw_test_fn test);
int hw_test_summary(void);
void hw_test_printf(const char *format, ...);
void hw_test_fail(const char *file, uint32_t line, const char *expression);
void hw_test_fail_u32(const char *file, uint32_t line, uint32_t expected,
                      uint32_t actual);

#define HW_TEST_RUN(test) hw_test_run(#test, test)

#define HW_TEST_ASSERT(condition)                                              \
    do {                                                                       \
        if (!(condition)) {                                                    \
            hw_test_fail(__FILE__, __LINE__, #condition);                      \
            return;                                                            \
        }                                                                      \
    } while (0)

#define HW_TEST_ASSERT_EQ_U32(expected_value, actual_value)                    \
    do {                                                                       \
        uint32_t hw_test_expected = (uint32_t)(expected_value);                \
        uint32_t hw_test_actual = (uint32_t)(actual_value);                    \
        if (hw_test_expected != hw_test_actual) {                              \
            hw_test_fail_u32(__FILE__, __LINE__, hw_test_expected,             \
                             hw_test_actual);                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#endif
