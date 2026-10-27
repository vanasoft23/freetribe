#ifndef GDB_MONITOR_FAKE_H
#define GDB_MONITOR_FAKE_H

#include "ft.h"

typedef struct {
    u32  continue_calls;
    u32  step_calls;
    bool step_ok;
    bool frame_ready;
} test_monitor_t;

extern test_monitor_t g_test_monitor;

#endif
