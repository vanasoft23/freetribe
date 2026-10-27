#include "dev_lcd.h"
#include "per_gpio.h"
#include "shutdown.h"


// copy from startup.c
static void _delay(unsigned int count)
{
	asm volatile(
		"1:\n"
		"subs %0, %0, #1\n"
		"bne 1b\n"
		: "+r"(count)
		:
		: "cc"
	);
}


// no idea where I should put this
void shutdown(void)
{
	per_gpio_set_indexed(PIN_SHUTDOWN, false);
	dev_lcd_set_backlight(false, true, false);

	// u32 i = 0;
	// bool rgb[3][3] = {
	// 	{ true , false, false },
	// 	{ false, true , false },
	// 	{ false, false, true  },
	// };

	// for (;;)
	// {
	// 	dev_lcd_set_backlight(rgb[i][0], rgb[i][1], rgb[i][2]);
	// 	i = (i + 1) % 3;
	// 	_delay(800000);
	// }
}



