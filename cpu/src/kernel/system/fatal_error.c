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
 * @file    debug.c
 * 
 * @brief   Provides an interface for debug printing to GDB with string-
 *          formatting.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <stdarg.h>
#include <string.h>

#define TINYPRINTF_DEFINE_TFP_PRINTF 0
#define TINYPRINTF_OVERRIDE_LIBC 0
#include <tinyprintf.h>

#include <ugui.h>
#include "mascot_bmp.h"

#include "arm_frame.h"
#include "per_gpio.h"
#include "dev_lcd.h"
#include "dev_delay.h"
#include "ui/render.h"
#include "string/pathstr.h"
#include "fatal_error.h"
#include "shutdown.h"

/*----- Macros -------------------------------------------------------*/

#define MAX_STR_LEN 512

#define BACK_COLOR 0x0
#define FORE_COLOR 0x1

#define FATAL_LINE_CHARS 21
#define FATAL_LINE_SIZE (FATAL_LINE_CHARS + 1)

/*----- Static variable definitions ----------------------------------*/

static char s_str[MAX_STR_LEN];

fatal_cb_t g_fatal_cb;
volatile arm_frame_t g_fatal_frame;
volatile u32 g_fatal_active;

static UG_DEVICE s_device;
static UG_GUI s_ugui;
static u8 s_framebuffer[0x400];

/*----- Static function prototypes -----------------------------------*/

static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c);
static void _print_frame(const volatile arm_frame_t *frame);
static void _render_default_bluescreen(const char *msg, const volatile arm_frame_t *frame);
static void _draw_bitmap(int x, int y, const u8 *img, UG_COLOR forecolor);
static u16 _read_le16(const u8 *data);
static u32 _read_le32(const u8 *data);
static bool _copy_fatal_line(char *line, const char **message);
static bool _fatal_message_has_content(const char *message);
static void _ellipsize_fatal_line(char *line);
static void _format_fatal_register(char *line, const char *name, u32 value);

/*----- Extern function implementations ------------------------------*/


/// @TODO: execute from safe stack context (in case of stack overflow)
///        use static piece of memory (ARM RAM?) for stack ?
void fatal_error_impl(const char *format, ...)
{
	va_list ap;
	va_list assertion_ap;
	const char *assertion_condition = NULL;
	const char *assertion_file = NULL;
	int assertion_line = 0;
	bool is_assertion;


	if (format == NULL)
		format = "(no message)";

	va_start(ap, format);
	is_assertion = (0 == strcmp(format, ASSERT_ERROR_FORMAT));
	if (is_assertion) {
		va_copy(assertion_ap, ap);
		assertion_condition = va_arg(assertion_ap, const char *);
		assertion_file = va_arg(assertion_ap, const char *);
		assertion_line = va_arg(assertion_ap, int);
		va_end(assertion_ap);
	}
	tfp_vsnprintf(s_str, MAX_STR_LEN, format, ap);
	va_end(ap);

	print_semihosting("\nFATAL ERROR\n");
	print_semihosting(s_str);
	print_semihosting("\n");
	_print_frame(&g_fatal_frame);

	if (is_assertion) {
		tfp_snprintf(
			s_str,
			MAX_STR_LEN,
			"Assertion failed:\n%s\n%s:%d",
			assertion_condition,
			path_extract_filename(assertion_file),
			assertion_line
		);
	}

	if (g_fatal_cb != NULL)
		g_fatal_cb(s_str, &g_fatal_frame);
	else
		_render_default_bluescreen(s_str, &g_fatal_frame);

	while (true) {
		bool held = !per_gpio_get_indexed(PIN_POWER_BUTTON);
		if (held) {
			shutdown();
		}
	}
}




/*----- Static function implementations ------------------------------*/


static void _render_default_bluescreen(
	const char *msg,
	const volatile arm_frame_t *frame)
{
	/***
	 *
	 * (Re-)initialize GUI completely.
	 *
	 */
	dev_lcd_set_backlight(false, false, true);
	dev_delay_cycles(5000);
	dev_lcd_reset(true);
	dev_delay_cycles(5000);
	dev_lcd_reset(false);
	dev_delay_cycles(5000);
	dev_lcd_init();
	dev_delay_cycles(5000);

	s_device.x_dim = 128;
	s_device.y_dim = 64;
	s_device.pset = _put_pixel;

	UG_Init(&s_ugui, &s_device);
	UG_FontSelect(FONT_5X8);
	// UG_ConsoleSetArea(4, 32, 123, 40);
	// UG_ConsoleSetBackcolor(BACK_COLOR);
	// UG_ConsoleSetForecolor(FORE_COLOR);
	UG_FillScreen(BACK_COLOR);

	/***
	 *
	 * Draw
	 *
	 */
	char message_line_1[FATAL_LINE_SIZE];
	char message_line_2[FATAL_LINE_SIZE];
	char message_line_3[FATAL_LINE_SIZE];
	char register_line[FATAL_LINE_SIZE];
	const char *message_cursor = msg ? msg : "";
	bool message_continues;

	message_continues = _copy_fatal_line(message_line_1, &message_cursor);
	if (message_continues)
		message_continues = _copy_fatal_line(message_line_2, &message_cursor);
	else
		message_line_2[0] = '\0';

	if (message_continues)
		message_continues = _copy_fatal_line(message_line_3, &message_cursor);
	else
		message_line_3[0] = '\0';

	if (message_continues)
		_ellipsize_fatal_line(message_line_3);

	UG_SelectGUI(&s_ugui);
	UG_FontSelect(FONT_5X8);
	UG_SetForecolor(FORE_COLOR);
	UG_SetBackcolor(BACK_COLOR);
	memset(s_framebuffer, 0, sizeof(s_framebuffer));

	UG_PutString(0, 0, message_line_1);
	UG_PutString(0, 8, message_line_2);
	UG_PutString(0, 16, message_line_3);

	_format_fatal_register(register_line, "PC", frame->pc);
	UG_PutString(0, 24, register_line);
	_format_fatal_register(register_line, "LR", frame->lr);
	UG_PutString(0, 32, register_line);
	_format_fatal_register(register_line, "SP", frame->sp);
	UG_PutString(0, 40, register_line);
	_format_fatal_register(register_line, "R11", frame->r11);
	UG_PutString(0, 48, register_line);
	_format_fatal_register(register_line, "CPSR", frame->cpsr);
	UG_PutString(0, 56, register_line);

	// The register text ends at x=76. Keep x=77 as one pixel of padding.
	_draw_bitmap(78, 24, mascot_bmp, BACK_COLOR);

	dev_lcd_set_frame(s_framebuffer);
}

static bool _copy_fatal_line(char *line, const char **message)
{
	const char *cursor = *message;
	const char *wrap_cursor = NULL;
	u8 length = 0;
	u8 wrap_length = 0;

	while (/*(' ' == *cursor) || */('\t' == *cursor) ||
		   ('\r' == *cursor) || ('\n' == *cursor)) {
		cursor++;
	}

	while (('\0' != *cursor) && ('\r' != *cursor) && ('\n' != *cursor) &&
		   (length < FATAL_LINE_CHARS)) {
		char character = *cursor++;

		if ('\t' == character) {
			character = ' ';
		}

		line[length++] = character;
		if (' ' == character) {
			wrap_cursor = cursor;
			wrap_length = length - 1;
		}
	}

	if ((FATAL_LINE_CHARS == length) && ('\0' != *cursor) &&
		('\r' != *cursor) && ('\n' != *cursor) && (' ' != *cursor) &&
		('\t' != *cursor) && wrap_cursor && (0 != wrap_length)) {
		cursor = wrap_cursor;
		length = wrap_length;
	}

	while ((length > 0) && (' ' == line[length - 1])) {
		length--;
	}
	line[length] = '\0';

	if ('\r' == *cursor) {
		cursor++;
	}
	if ('\n' == *cursor) {
		cursor++;
	}

	*message = cursor;
	return _fatal_message_has_content(cursor);
}

static bool _fatal_message_has_content(const char *message)
{
	while ((' ' == *message) || ('\t' == *message) ||
		   ('\r' == *message) || ('\n' == *message)) {
		message++;
	}

	return '\0' != *message;
}

static void _ellipsize_fatal_line(char *line)
{
	u8 length = strlen(line);

	if (length > (FATAL_LINE_CHARS - 3)) {
		length = FATAL_LINE_CHARS - 3;
	}

	line[length++] = '.';
	line[length++] = '.';
	line[length++] = '.';
	line[length] = '\0';
}

static void _format_fatal_register(char *line, const char *name, u32 value)
{
	static const char hex[] = "0123456789ABCDEF";
	u8 length = 0;

	while (('\0' != *name) && (length < 4)) {
		line[length++] = *name++;
	}
	while (length < 5) {
		line[length++] = ' ';
	}

	for (s8 shift = 28; shift >= 0; shift -= 4) {
		line[length++] = hex[(value >> shift) & 0x0f];
	}
	line[length] = '\0';
}





static void _draw_bitmap(int x, int y, const u8 *img, UG_COLOR forecolor)
{
	UG_COLOR backcolor = forecolor ^ 1;
	s32 width;
	s32 signed_height;
	u32 height;
	u32 pixel_offset;
	u32 row_stride;
	const u8 *palette;
	bool one_is_foreground;

	if ((NULL == img) || ('B' != img[0]) || ('M' != img[1]) ||
		(40u != _read_le32(img + 14)) ||
		(1u != _read_le16(img + 26)) ||
		(1u != _read_le16(img + 28)) ||
		(0u != _read_le32(img + 30))) {
		return;
	}

	width = (s32)_read_le32(img + 18);
	signed_height = (s32)_read_le32(img + 22);

	if ((width <= 0) || (width > s_device.x_dim) ||
		(0 == signed_height) || (signed_height > s_device.y_dim) ||
		(signed_height < -s_device.y_dim)) {
		return;
	}

	height = (signed_height < 0) ? (u32)-signed_height : (u32)signed_height;
	pixel_offset = _read_le32(img + 10);
	row_stride = (((u32)width + 31u) / 32u) * 4u;
	palette = img + 54;

	// A set BMP bit selects palette entry 1. Map its brighter entry to
	// forecolor so images with a reversed black/white palette still work.
	one_is_foreground =
		((u16)palette[4] + palette[5] + palette[6]) >=
		((u16)palette[0] + palette[1] + palette[2]);

	for (u32 image_y = 0; image_y < height; image_y++) {
		u32 source_y = (signed_height > 0) ?
			(height - image_y - 1u) : image_y;
		const u8 *source_row = img + pixel_offset + (source_y * row_stride);

		for (u32 image_x = 0; image_x < (u32)width; image_x++) {
			int draw_x = x + image_x;
			int draw_y = y + image_y;
			bool bit_is_set;
			bool pixel_on;

			if ((draw_x < 0) || (draw_x >= s_device.x_dim) ||
				(draw_y < 0) || (draw_y >= s_device.y_dim)) {
				continue;
			}

			bit_is_set = 0 !=
				(source_row[image_x >> 3] & (0x80u >> (image_x & 7u)));
			pixel_on = (bit_is_set == one_is_foreground);

			_put_pixel(
				(UG_S16)draw_x,
				(UG_S16)draw_y,
				pixel_on ? forecolor : backcolor
			);
		}
	}

}


static u16 _read_le16(const u8 *data)
{
	return (u16)data[0] | ((u16)data[1] << 8);
}


static u32 _read_le32(const u8 *data)
{
	return (u32)data[0] |
		((u32)data[1] << 8) |
		((u32)data[2] << 16) |
		((u32)data[3] << 24);
}



static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c)
{
	u16 byte_index;
	u16 bit_index;
	u8 byte;

	if ((pos_x < 0) || (pos_x >= s_device.x_dim)
	 || (pos_y < 0) || (pos_y >= s_device.y_dim)) {
		return;
	}

	byte_index = pos_x + ((pos_y >> 3) << 7);
	bit_index = pos_y & 7;
	byte = s_framebuffer[byte_index];

	s_framebuffer[byte_index] =
		(byte & ~(1UL << bit_index)) | (((bool)c) << bit_index);
}


static void _print_frame(const volatile arm_frame_t *frame)
{
	char line[80];

	tfp_snprintf(
		line,
		sizeof(line),
		"R0=%08X R1=%08X R2=%08X R3=%08X\n",
		(unsigned int)frame->r0,
		(unsigned int)frame->r1,
		(unsigned int)frame->r2,
		(unsigned int)frame->r3
	);
	print_semihosting(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R4=%08X R5=%08X R6=%08X R7=%08X\n",
		(unsigned int)frame->r4,
		(unsigned int)frame->r5,
		(unsigned int)frame->r6,
		(unsigned int)frame->r7
	);
	print_semihosting(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R8=%08X R9=%08X R10=%08X R11=%08X\n",
		(unsigned int)frame->r8,
		(unsigned int)frame->r9,
		(unsigned int)frame->r10,
		(unsigned int)frame->r11
	);
	print_semihosting(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"R12=%08X SP=%08X LR=%08X PC=%08X\n",
		(unsigned int)frame->r12,
		(unsigned int)frame->sp,
		(unsigned int)frame->lr,
		(unsigned int)frame->pc
	);
	print_semihosting(line);

	tfp_snprintf(
		line,
		sizeof(line),
		"CPSR=%08X\n",
		(unsigned int)frame->cpsr
	);
	print_semihosting(line);
}


#ifdef DEBUG
inline void illegal_instruction(void)
{
	asm volatile (".word 0xe7f000f0\n");
}
#endif
