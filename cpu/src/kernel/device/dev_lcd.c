/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

 Freetribe is free software: you can redistribute it and/or modify it
under the terms of the GNU Affero General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
                  (at your option) any later version.

     Freetribe is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty
        of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
          See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

/**
 * @file    dev_lcd.c
 *
 * @brief   LCD device driver.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_gpio.h"
#include "per_spi2.h"

#include "dev_lcd.h"

/*----- Macros -------------------------------------------------------*/

#define SPI0 0
#define LCD_SPI_INT_CHANNEL 12 // UNUSED SO FAR

#define LCD_COLUMNS 0x80
#define LCD_PAGES 0x8
#define LCD_PAGE_LINES 0x8
#define LCD_FATAL_SPI_POLL_LIMIT 100000u

#define LCD_BACKLIGHT_RED_PIN   100u // GP6P3
#define LCD_BACKLIGHT_GREEN_PIN 101u // GP6P4
#define LCD_BACKLIGHT_BLUE_PIN  102u // GP6P5

/*----- Typedefs -----------------------------------------------------*/

typedef enum { COMMAND, DATA } lcd_mode_t;

/*----- Static variable definitions ----------------------------------*/


/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static void _lcd_command(u8 command);
static void _lcd_mode(lcd_mode_t mode);
static void _lcd_tx(const u8 *buffer, u32 length);

/*----- Extern function implementations ------------------------------*/

void dev_lcd_init(void)
{
	spi_config_t config = {
		.instance = SPI0
	};
	per_spi2_init(&config);

	_lcd_mode(COMMAND);

	// _lcd_command(0xa2); // 0xa2: LCD bias set
	// _lcd_command(0xa0); // 0xa0: ADC select normal
	// _lcd_command(0xc8); // 0xc8: Common output mode reverse
	// _lcd_command(0x40); // 0x40: Line address 0
	// _lcd_command(0x2f); // 0x2f: Power control: Boost=On, Reg=On, Follow=On
	// _lcd_command(0xf8); // 0xf8: Booster ratio select mode set
	// _lcd_command(0x00); // 0x00: Booster ratio register = 0
	// _lcd_command(0x25); // 0x25: Voltage Regulator Resistor Ratio Set
	//
	// // Contrast setting.
	// _lcd_command(0x81); // 0x81: Electronic volume mode set
	// _lcd_command(0x22); // 0x22: Electronic volume register = 0x22
	//
	// _lcd_command(0xa4); // 0xa4: Display all points = Off
	// _lcd_command(0xa6); // 0xa6: Display Normal/Reverse = Normal
	// _lcd_command(0xaf); // 0xaf: Display on

	u8 lcd_init_cmd[] = {
		0xa2, 0xa0, 0xc8, 0x40, 0x2f, 0xf8, 0x00,
		0x25, 0x81, 0x22, 0xa4, 0xa6, 0xaf
	};

	_lcd_tx(lcd_init_cmd, sizeof((lcd_init_cmd)));
}

// True sets reset pin low.
void dev_lcd_reset(bool state)
{
	per_gpio_set_indexed(PIN_GP6_9, !state);
}

void dev_lcd_set_frame(const u8 *frame_buffer)
{
	u8 page_index;
	const u8 *page_buffer;

	for (page_index = 0; page_index < LCD_PAGES; page_index++) {
		page_buffer = frame_buffer + page_index * LCD_COLUMNS;

		/// TODO: Implement ping-pong frame buffer.
		///       strcmp page buffer and only update changes.

		dev_lcd_set_page(page_index, page_buffer);
	}
}

void dev_lcd_set_page(u8 page_index, const u8 *page_buffer)
{
	_lcd_mode(COMMAND);

	// _lcd_command(0xb0 | page_index); // Set display RAM page address.
	// _lcd_command(0x40);              // Start line.
	// _lcd_command(0x10);              // Column address upper.
	// _lcd_command(0x00);              // Column address lower.

	u8 lcd_page_cmd[] = {0xb0 | page_index, 0x40, 0x10, 0x00};

	_lcd_tx(lcd_page_cmd, sizeof(lcd_page_cmd));

	_lcd_mode(DATA);

	_lcd_tx(page_buffer, LCD_COLUMNS);
}

void dev_lcd_set_contrast(u8 contrast)
{
	contrast = contrast > 0x3f ? 0x3f : contrast;
	contrast = contrast < 0x01 ? 0x01 : contrast;

	_lcd_mode(COMMAND);
	_lcd_command(0x81);
	_lcd_command(contrast);
}

/*
 * @brief   Set LCD backlight.
 *
 * @param   none
 *
 * @return  none
 */
void dev_lcd_set_backlight(bool red, bool green, bool blue)
{
	/// TODO: Define pin index.
	//
	// LCD Backlight on
	per_gpio_set_indexed(LCD_BACKLIGHT_RED_PIN, red);
	per_gpio_set_indexed(LCD_BACKLIGHT_GREEN_PIN, green);
	per_gpio_set_indexed(LCD_BACKLIGHT_BLUE_PIN, blue);

	// Configure LCD
}

/*----- Static function implementations ------------------------------*/

static void _lcd_tx(const u8 *buffer, u32 length)
{
	// /// TODO: DMA frame buffer.

	bool cshold = (length > 1);
	if (cshold) {
		per_spi2_select(SPI0, SPI_LCD_FORMAT, 1, true);
		per_spi2_tx_blocking(SPI0, buffer, length);
		per_spi2_select(SPI0, SPI_LCD_FORMAT, 1, false);
	} else {
		per_spi2_select(SPI0, SPI_LCD_FORMAT, 1, false);
		per_spi2_tx_blocking(SPI0, buffer, length);
	}
}

static void _lcd_mode(lcd_mode_t mode)
{
	per_gpio_set_indexed(PIN_SPI0_SCS2__UART0_RTS__MII_RXD0, mode);
}

static void _lcd_command(u8 command)
{
	_lcd_tx(&command, 1);
}

/*----- End of file --------------------------------------------------*/
