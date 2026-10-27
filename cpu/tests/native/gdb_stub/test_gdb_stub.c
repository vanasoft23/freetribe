#include "gdb_stub.h"
#include "gdb_server.h"
#include "gdb_monitor_fake.h"
#include "rsp_session.h"
#include "str_misc.h"

#include <unity.h>

/* monitor boot 0xc0123400 */
static const char s_boot[] = "qRcmd,626f6f742030786330313233343030";
static u32 s_write_calls;

static bool _write_memory(void *context, u32 address, const u8 *data, u32 length)
{
    (void)context;
    TEST_ASSERT_EQUAL_HEX32(0x1000u, address);
    TEST_ASSERT_EQUAL_UINT32(1u, length);
    TEST_ASSERT_EQUAL_HEX8(0xabu, data[0]);
    s_write_calls++;
    return true;
}

static u32 _packet(const char *payload, u8 *packet)
{
    u32 length = (u32)strlen(payload);
    u8 checksum = 0;
    packet[0] = '$';
    for (u32 i = 0; i < length; ++i) {
        packet[i + 1u] = (u8)payload[i];
        checksum += (u8)payload[i];
    }
    packet[length + 1u] = '#';
    packet[length + 2u] = (u8)hex_digit(checksum >> 4);
    packet[length + 3u] = (u8)hex_digit(checksum);
    return length + 4u;
}

static void _send(const char *payload)
{
    u8 packet[128];
    u32 length = _packet(payload, packet);
    TEST_ASSERT_EQUAL_UINT32(length, gdb_stub_rx(packet, length));
}

static void _control(u8 byte)
{
    TEST_ASSERT_EQUAL_UINT32(1u, gdb_stub_rx(&byte, 1u));
}

static void _expect_tx(const char *expected)
{
    u32 length;
    const u8 *data = gdb_stub_tx_peek(&length);
    TEST_ASSERT_EQUAL_UINT32(strlen(expected), length);
    if (length != 0) {
        TEST_ASSERT_EQUAL_MEMORY(expected, data, length);
    }
}

static void _drain(void)
{
    u32 length;
    (void)gdb_stub_tx_peek(&length);
    gdb_stub_tx_discard(length);
}

void setUp(void)
{
    gdb_stub_reset();
    s_write_calls = 0;
    gdb_server_set_memory_writer(&g_gdb_server, _write_memory, NULL);
}

void tearDown(void)
{
}

void test_continue_runs_once_and_reports_the_next_stop(void)
{
    _send("c");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    _expect_tx("+");
    _drain();
    gdb_stub_service();
    _expect_tx("");

    g_test_monitor.frame_ready = true;
    gdb_stub_service();
    _expect_tx("$S05#b8");
    _drain();
    _control('+');
    gdb_stub_service();
    _expect_tx("");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
}

void test_step_prepares_before_resuming_and_reports_the_next_stop(void)
{
    _send("s");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.step_calls);
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    _drain();
    g_test_monitor.frame_ready = true;
    gdb_stub_service();
    _expect_tx("$S05#b8");
}

void test_failed_step_does_not_resume(void)
{
    g_test_monitor.step_ok = false;
    _send("s");
    _expect_tx("+$E01#a6");
    TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
}

void test_detach_resumes_after_reply_is_queued_without_waiting_for_ack(void)
{
    _send("D");
    _expect_tx("+$OK#9a");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    _drain();
    _control('+');
    g_test_monitor.frame_ready = true;
    gdb_stub_service();
    _expect_tx("");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
}

void test_detach_waits_while_an_earlier_reply_is_unacknowledged(void)
{
    _send("?");
    _drain();
    _send("D");
    gdb_stub_service();
    TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
    _drain();
    _control('+');
    _expect_tx("$OK#9a");
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    gdb_stub_service();
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
}

void test_reply_retry_and_nack_do_not_repeat_memory_writes(void)
{
    _send("?");
    _drain();
    _send("M1000,1:ab");
    _drain();
    gdb_stub_print("x");
    gdb_stub_service();
    gdb_stub_service();
    TEST_ASSERT_EQUAL_UINT32(1u, s_write_calls);
    _expect_tx("");

    _control('+');
    _expect_tx("$OK#9a");
    _drain();
    _control('-');
    _expect_tx("$OK#9a");
    TEST_ASSERT_EQUAL_UINT32(1u, s_write_calls);
    _drain();
    _control('+');
    gdb_stub_service();
    _expect_tx("$O780a#4f");
}

void test_full_tx_buffer_retries_on_drain_without_repeating_the_command(void)
{
    /* Fill TX without requiring an ACK, so only draining it frees space. */
    static u8 payload[RSP_PACKET_CAPACITY];
    memset(payload, '$', sizeof(payload));
    TEST_ASSERT_TRUE(rsp_tx_commit(&g_rsp_session, payload, sizeof(payload), true));
    _send("M1000,1:ab");
    gdb_stub_service();
    TEST_ASSERT_EQUAL_UINT32(1u, s_write_calls);

    gdb_stub_tx_discard(1u);
    TEST_ASSERT_EQUAL_UINT32(1u, s_write_calls);
    _drain();
    _expect_tx("$OK#9a");
    TEST_ASSERT_EQUAL_UINT32(1u, s_write_calls);
}

void test_boot_handoff_retains_entry_until_its_reply_is_acknowledged(void)
{
    u32 entry = 0;
    _send(s_boot);
    TEST_ASSERT_FALSE(gdb_stub_take_handoff_request(&entry));
    TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
    _expect_tx("+$OK#9a");
    _drain();
    _control('-');
    _expect_tx("$OK#9a");
    TEST_ASSERT_FALSE(gdb_stub_take_handoff_request(&entry));
    _drain();
    _control('+');
    gdb_stub_service();
    _control('+');
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    TEST_ASSERT_TRUE(gdb_stub_take_handoff_request(&entry));
    TEST_ASSERT_EQUAL_HEX32(0xc0123400u, entry);
    TEST_ASSERT_FALSE(gdb_stub_take_handoff_request(&entry));
}

void test_earlier_ack_only_queues_boot_reply(void)
{
    u32 entry;
    _send("?");
    _drain();
    _send(s_boot);
    _drain();
    _control('+');
    _expect_tx("$OK#9a");
    TEST_ASSERT_FALSE(gdb_stub_take_handoff_request(&entry));
    TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
    _drain();
    _control('+');
    TEST_ASSERT_TRUE(gdb_stub_take_handoff_request(&entry));
    TEST_ASSERT_EQUAL_HEX32(0xc0123400u, entry);
}

void test_later_packet_is_left_unconsumed_until_pending_reply_is_queued(void)
{
    static const u8 packets[] = "$D#44$c#63";
    _send("?");
    _drain();
    u32 consumed = gdb_stub_rx(packets, sizeof(packets) - 1u);
    TEST_ASSERT_EQUAL_UINT32(5u, consumed);
    TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
    _drain();
    _control('+');
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    TEST_ASSERT_EQUAL_UINT32(5u, gdb_stub_rx(packets + consumed, 5u));
    TEST_ASSERT_EQUAL_UINT32(2u, g_test_monitor.continue_calls);
}

void test_ready_handoff_cannot_be_overwritten_by_another_packet(void)
{
    static const u8 input[] = "+$c#63";
    u32 entry;
    _send(s_boot);
    _drain();
    TEST_ASSERT_EQUAL_UINT32(1u, gdb_stub_rx(input, sizeof(input) - 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, g_test_monitor.continue_calls);
    TEST_ASSERT_TRUE(gdb_stub_take_handoff_request(&entry));
    TEST_ASSERT_EQUAL_HEX32(0xc0123400u, entry);
    TEST_ASSERT_EQUAL_UINT32(5u, gdb_stub_rx(input + 1u, 5u));
}

void test_reset_cancels_handoff_at_each_wait_point(void)
{
    for (u32 stage = 0; stage < 3u; ++stage) {
        gdb_stub_reset();
        if (stage == 0) {
            _send("?");
            _drain();
        }
        _send(s_boot);
        _drain();
        if (stage == 2) {
            _control('+');
        }
        gdb_stub_reset();
        _control('+');
        gdb_stub_service();
        u32 entry;
        TEST_ASSERT_FALSE(gdb_stub_take_handoff_request(&entry));
        TEST_ASSERT_EQUAL_UINT32(0u, g_test_monitor.continue_calls);
        _expect_tx("");
        _send("?");
        _expect_tx("+$S05#b8");
    }
}

void test_stop_reply_precedes_console_output(void)
{
    _send("c");
    _drain();
    gdb_stub_print("x");
    g_test_monitor.frame_ready = true;
    gdb_stub_service();
    _expect_tx("$S05#b8");
    _drain();
    _control('+');
    gdb_stub_service();
    _expect_tx("$O780a#4f");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_continue_runs_once_and_reports_the_next_stop);
    RUN_TEST(test_step_prepares_before_resuming_and_reports_the_next_stop);
    RUN_TEST(test_failed_step_does_not_resume);
    RUN_TEST(test_detach_resumes_after_reply_is_queued_without_waiting_for_ack);
    RUN_TEST(test_detach_waits_while_an_earlier_reply_is_unacknowledged);
    RUN_TEST(test_reply_retry_and_nack_do_not_repeat_memory_writes);
    RUN_TEST(test_full_tx_buffer_retries_on_drain_without_repeating_the_command);
    RUN_TEST(test_boot_handoff_retains_entry_until_its_reply_is_acknowledged);
    RUN_TEST(test_earlier_ack_only_queues_boot_reply);
    RUN_TEST(test_later_packet_is_left_unconsumed_until_pending_reply_is_queued);
    RUN_TEST(test_ready_handoff_cannot_be_overwritten_by_another_packet);
    RUN_TEST(test_reset_cancels_handoff_at_each_wait_point);
    RUN_TEST(test_stop_reply_precedes_console_output);
    return UNITY_END();
}
