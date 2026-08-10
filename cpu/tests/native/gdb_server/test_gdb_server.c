#include <string.h>

#include <unity.h>

#include "gdb_monitor.h"
#include "gdb_server.h"

volatile u32 gdb_should_stop;
volatile gdb_monitor_t g_monitor;

enum {
    TEST_MEMORY_BASE = 0x1000u,
    REPLY_CAPACITY   = 64u,
};

typedef struct {
    u8   *data;
    u32   length;
    u32   read_calls;
    u32   write_calls;
    bool  reject_read;
    bool  reject_write;
} test_memory_t;

static test_memory_t s_memory;
static u8            s_reply_data[REPLY_CAPACITY];
static gdb_reply_writer_t s_reply;

static bool _read_memory(
    void *context,
    u32   address,
    u8   *destination,
    u32   length
) {
    test_memory_t *memory = context;
    memory->read_calls++;

    if (memory->reject_read ||
        address < TEST_MEMORY_BASE ||
        length > memory->length ||
        address - TEST_MEMORY_BASE > memory->length - length) {
        return false;
    }

    memcpy(destination, memory->data + address - TEST_MEMORY_BASE, length);
    return true;
}

static bool _write_memory(
    void     *context,
    u32       address,
    const u8 *source,
    u32       length
) {
    test_memory_t *memory = context;
    memory->write_calls++;

    if (memory->reject_write ||
        address < TEST_MEMORY_BASE ||
        length > memory->length ||
        address - TEST_MEMORY_BASE > memory->length - length) {
        return false;
    }

    memcpy(memory->data + address - TEST_MEMORY_BASE, source, length);
    return true;
}

static void _handle(const char *packet) {
    gdb_request_t request = {
        .data = (const u8 *)packet,
        .len  = (u32)strlen(packet),
    };
    (void)gdb_server_handle_req(&g_gdb_server, request, &s_reply);
}

static void _assert_reply(const char *expected) {
    u32 expected_length = (u32)strlen(expected);
    TEST_ASSERT_EQUAL_UINT32(expected_length, s_reply.len);
    if (expected_length != 0u) {
        TEST_ASSERT_EQUAL_MEMORY(expected, s_reply.data, expected_length);
    }
}

void setUp(void) {
    static u8 data[40];

    memset(data, 0, sizeof(data));
    data[1] = 0x12u;
    data[2] = 0xabu;
    data[3] = 0xffu;
    data[4] = 0x5eu;

    s_memory = (test_memory_t){
        .data   = data,
        .length = (u32)sizeof(data),
    };
    s_reply = (gdb_reply_writer_t){
        .data     = s_reply_data,
        .capacity = (u32)sizeof(s_reply_data),
    };
    gdb_server_set_memory_reader(&g_gdb_server, _read_memory, &s_memory);
    gdb_server_set_memory_writer(&g_gdb_server, _write_memory, &s_memory);
}

void tearDown(void) {
}

void test_read_memory_returns_two_hex_digits_per_byte(void) {
    _handle("m1001,3");

    _assert_reply("12abff");
    TEST_ASSERT_EQUAL_UINT32(1u, s_memory.read_calls);
}

void test_zero_length_read_does_not_touch_memory(void) {
    _handle("m1000,0");

    _assert_reply("");
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.read_calls);
}

void test_malformed_packets_are_rejected_before_memory_access(void) {
    static const char *packets[] = {
        "m", "m,1", "m1000,", "mxyz,1", "m1000,1x", "m100000000,1"
    };

    for (u32 i = 0u; i < (u32)(sizeof(packets) / sizeof(packets[0])); ++i) {
        _handle(packets[i]);
        _assert_reply("E01");
    }
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.read_calls);
}

void test_oversized_and_wrapping_ranges_are_rejected_before_reading(void) {
    s_reply.capacity = 5u;
    _handle("m1000,3");
    _assert_reply("E22");

    s_reply.capacity = REPLY_CAPACITY;
    _handle("mfffffffe,3");
    _assert_reply("E22");

    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.read_calls);
}

void test_reader_can_reject_an_unsafe_range(void) {
    s_memory.reject_read = true;

    _handle("m1000,1");

    _assert_reply("E14");
    TEST_ASSERT_EQUAL_UINT32(1u, s_memory.read_calls);
}

void test_write_memory_decodes_hex_and_returns_ok(void) {
    _handle("M1001,3:12aBff");

    _assert_reply("OK");
    TEST_ASSERT_EQUAL_HEX8(0x12u, s_memory.data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xabu, s_memory.data[2]);
    TEST_ASSERT_EQUAL_HEX8(0xffu, s_memory.data[3]);
    TEST_ASSERT_EQUAL_UINT32(1u, s_memory.write_calls);
}

void test_zero_length_write_does_not_touch_memory(void) {
    _handle("M1000,0:");

    _assert_reply("OK");
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.write_calls);
}

void test_large_write_is_applied_in_bounded_chunks(void) {
    _handle(
        "M1000,21:"
        "000102030405060708090a0b0c0d0e0f"
        "101112131415161718191a1b1c1d1e1f20"
    );

    _assert_reply("OK");
    for (u32 i = 0u; i < 33u; ++i) {
        TEST_ASSERT_EQUAL_HEX8((u8)i, s_memory.data[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(2u, s_memory.write_calls);
}

void test_malformed_write_packets_are_rejected_before_memory_access(void) {
    static const char *packets[] = {
        "M", "M,1:00", "M1000,:00", "M1000,1", "M1000,1:",
        "M1000,1:0", "M1000,1:0000", "M1000,1:gg",
        "M100000000,1:00"
    };

    for (u32 i = 0u; i < (u32)(sizeof(packets) / sizeof(packets[0])); ++i) {
        _handle(packets[i]);
        _assert_reply("E01");
    }
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.write_calls);
}

void test_invalid_late_hex_digit_cannot_cause_a_partial_write(void) {
    _handle(
        "M1000,21:"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "gg"
    );

    _assert_reply("E01");
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.write_calls);
}

void test_wrapping_write_range_is_rejected_before_memory_access(void) {
    _handle("Mfffffffe,3:000000");

    _assert_reply("E22");
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.write_calls);
}

void test_writer_can_reject_an_unsafe_range(void) {
    s_memory.reject_write = true;

    _handle("M1000,1:ff");

    _assert_reply("E14");
    TEST_ASSERT_EQUAL_HEX8(0x00u, s_memory.data[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, s_memory.write_calls);
}

void test_write_is_skipped_when_success_reply_cannot_fit(void) {
    s_reply.capacity = 1u;

    _handle("M1000,1:ff");

    TEST_ASSERT_TRUE(s_reply.overflow);
    TEST_ASSERT_EQUAL_UINT32(0u, s_reply.len);
    TEST_ASSERT_EQUAL_HEX8(0x00u, s_memory.data[0]);
    TEST_ASSERT_EQUAL_UINT32(0u, s_memory.write_calls);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_read_memory_returns_two_hex_digits_per_byte);
    RUN_TEST(test_zero_length_read_does_not_touch_memory);
    RUN_TEST(test_malformed_packets_are_rejected_before_memory_access);
    RUN_TEST(test_oversized_and_wrapping_ranges_are_rejected_before_reading);
    RUN_TEST(test_reader_can_reject_an_unsafe_range);
    RUN_TEST(test_write_memory_decodes_hex_and_returns_ok);
    RUN_TEST(test_zero_length_write_does_not_touch_memory);
    RUN_TEST(test_large_write_is_applied_in_bounded_chunks);
    RUN_TEST(test_malformed_write_packets_are_rejected_before_memory_access);
    RUN_TEST(test_invalid_late_hex_digit_cannot_cause_a_partial_write);
    RUN_TEST(test_wrapping_write_range_is_rejected_before_memory_access);
    RUN_TEST(test_writer_can_reject_an_unsafe_range);
    RUN_TEST(test_write_is_skipped_when_success_reply_cannot_fit);
    return UNITY_END();
}
