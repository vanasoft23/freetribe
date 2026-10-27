#ifndef MMCSD_HW_TEST_H
#define MMCSD_HW_TEST_H

#include "ft_types.h"
#include "ft_macros.h"

typedef void (*hw_test_fn)(void);

void hw_test_begin(void);
void hw_test_run(const char *name, hw_test_fn test);
int hw_test_summary(void);
void hw_test_printf(const char *format, ...);
void hw_test_fail(const char *file, u32 line, const char *expression);
void hw_test_fail_u32(const char *file, u32 line, u32 expected,
                      u32 actual);

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
        u32 hw_test_expected = (u32)(expected_value);                          \
        u32 hw_test_actual = (u32)(actual_value);                              \
        if (hw_test_expected != hw_test_actual) {                              \
            hw_test_fail_u32(__FILE__, __LINE__, hw_test_expected,             \
                             hw_test_actual);                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#endif
