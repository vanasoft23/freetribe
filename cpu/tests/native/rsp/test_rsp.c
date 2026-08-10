#include <unity.h>

#include "rsp_session.h"

void setUp(void) {
    rsp_session_reset(&g_rsp_session);
}

void tearDown(void) {
}

void test_reset_starts_with_no_pending_output(void) {
    u32 length = 123u;

    (void)rsp_tx_peek(&g_rsp_session, &length);

    TEST_ASSERT_EQUAL_UINT32(0u, length);
    TEST_ASSERT_FALSE(rsp_expect_escaped(&g_rsp_session));
}

void test_bytes_before_a_packet_are_ignored(void) {
    static const u8 noise[] = {'x', '#', '0', '0'};
    u32 length = 123u;

    rsp_rx_feed_byte(&g_rsp_session, noise, (u32)sizeof(noise));
    (void)rsp_tx_peek(&g_rsp_session, &length);

    TEST_ASSERT_EQUAL_UINT32(0u, length);
    TEST_ASSERT_FALSE(rsp_expect_escaped(&g_rsp_session));
}

void test_escape_state_is_preserved_between_receive_chunks(void) {
    static const u8 packet_start[] = {'$', '}'};

    rsp_rx_feed_byte(&g_rsp_session, packet_start, (u32)sizeof(packet_start));

    TEST_ASSERT_TRUE(rsp_expect_escaped(&g_rsp_session));

    rsp_session_reset(&g_rsp_session);

    TEST_ASSERT_FALSE(rsp_expect_escaped(&g_rsp_session));
}

void test_valid_packet_exposes_payload_view(void) {
    static const u8 packet[] = {'$', '?', '#', '3', 'f'};

    TEST_ASSERT_TRUE(rsp_rx_feed_byte(
        &g_rsp_session,
        packet,
        (u32)sizeof(packet)
    ));

    rsp_packet_view_t view = rsp_rx_packet_view(&g_rsp_session);
    TEST_ASSERT_EQUAL_UINT32(1u, view.len);
    TEST_ASSERT_EQUAL_UINT8('?', view.data[0]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_starts_with_no_pending_output);
    RUN_TEST(test_bytes_before_a_packet_are_ignored);
    RUN_TEST(test_escape_state_is_preserved_between_receive_chunks);
    RUN_TEST(test_valid_packet_exposes_payload_view);
    return UNITY_END();
}
