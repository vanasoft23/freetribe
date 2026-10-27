#include "per_gpio.h"
#include "per_aintc.h"
#include "per_pinmux.h"
#include "per_spi2.h"

#include "hw_test.h"


__attribute__((noinline, noreturn, used))
void mmcsd_hw_test_complete(int result);

// static void _test_harness_smoke(void)
// {
//     HW_TEST_ASSERT(true);
//     HW_TEST_ASSERT_EQ_U32(0x1802u, 0x1802u);
// }

// static void _test_sd_card_present(void)
// {
//     bool card_present = !per_gpio_get_indexed(PIN_EMA_A_16__MMCSD0_DAT5);

//     HW_TEST_ASSERT(card_present);
// }

static void _test_lcd(void);

/** start_boot should be called already at this point. */
int main(void)
{
	// Board bringup
	per_pinmux_init();
	per_gpio_init();
	per_aintc_init();

	_test_lcd();

	hw_test_begin();

	// HW_TEST_RUN(test_spi);
	// HW_TEST_RUN(_test_harness_smoke);
	// HW_TEST_RUN(_test_sd_card_present);

	return hw_test_summary();
}

// __attribute__((noinline, noreturn, used))
// void mmcsd_hw_test_complete(int result)
// {
//     hw_test_printf("[ COMPLETE ] result=%d; halted for debugger\n", result);

//     for (;;) {
//         asm volatile("nop");
//     }
// }


static void _lcd_mode(u8 mode)
{
	per_gpio_set_indexed(PIN_SPI0_SCS2__UART0_RTS__MII_RXD0, mode);
}


static void _test_lcd(void)
{
	_lcd_mode(0); // COMMAND
	const u8 init_sequence[] = {
		0xa2, 0xa0, 0xc8, 0x40, 0x2f, 0xf8, 0x00,
		0x25, 0x81, 0x22, 0xa4, 0xa6, 0xaf
	};
	per_spi2_select(0, SPI_LCD_FORMAT, 1, true);
	per_spi2_write_blocking(0, init_sequence, sizeof((init_sequence)));
	per_spi2_select(0, SPI_LCD_FORMAT, 1, false);
}
