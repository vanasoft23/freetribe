#include "hw_test.h"

void setUp(void)
{
	
}

void tearDown(void)
{

}


// void test_valid_packet_exposes_payload_view(void)
// {
//     static const u8 packet[] = {'$', '?', '#', '3', 'f'};

//     TEST_ASSERT_EQUAL(RSP_RX_EVENT_PACKET, _feed(
//         packet,
//         (u32)sizeof(packet)
//     ));

//     rsp_packet_view_t view = rsp_rx_packet_view(&g_rsp_session);
//     TEST_ASSERT_EQUAL_UINT32(1u, view.len);
//     TEST_ASSERT_EQUAL_UINT8('?', view.data[0]);
// }
