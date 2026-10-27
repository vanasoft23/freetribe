#include "gdb_monitor.h"
#include "gdb_monitor_fake.h"

test_monitor_t g_test_monitor;

void gdb_monitor_reset(void)
{
    g_test_monitor = (test_monitor_t){
        .step_ok = true,
        .frame_ready = true,
    };
}

void gdb_monitor_continue(void)
{
    g_test_monitor.continue_calls++;
    g_test_monitor.frame_ready = false;
}

bool gdb_monitor_prepare_step(void)
{
    g_test_monitor.step_calls++;
    return g_test_monitor.step_ok;
}

bool gdb_monitor_frame_ready(void)
{
    return g_test_monitor.frame_ready;
}

int gdb_monitor_get_stop_reason(void)
{
    return SIGTRAP;
}

arm_frame_t *gdb_monitor_read_registers(void)
{
    static arm_frame_t frame;
    return &frame;
}

bool gdb_monitor_insert_breakpoint(u32 address)
{
    (void)address;
    return true;
}

bool gdb_monitor_remove_breakpoint(u32 address)
{
    (void)address;
    return true;
}

bool gdb_monitor_read_memory(void *context, u32 address, u8 *data, u32 length)
{
    (void)context;
    (void)address;
    (void)data;
    (void)length;
    return false;
}

bool gdb_monitor_write_memory(void *context, u32 address, const u8 *data, u32 length)
{
    (void)context;
    (void)address;
    (void)data;
    (void)length;
    return false;
}
