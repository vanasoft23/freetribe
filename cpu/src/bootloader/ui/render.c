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
 * @file    gui.c
 * @brief   Passive bootloader view rendered with UGUI.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <string.h>
#include <ugui.h>

#include "dev_lcd.h"
#include "mascot_bmp.h"
#include "render.h"

/*----- Macros -------------------------------------------------------*/

#define DISP_NAME_MAX (20)
#define FILELIST_X (0)
#define FILELIST_WIDTH (128)
#define FILELIST_ROW_HEIGHT (8)
#define FILELIST_ICON_WIDTH (8)
#define POPUP_X (6)
#define POPUP_Y (17)
#define POPUP_WIDTH (120)
#define POPUP_HEIGHT (25)
#define POPUP_MESSAGE_LINE_MAX (64)
#define POPUP_MESSAGE_Y_OFFSET (14)
#define POPUP_OK_X (24)
#define POPUP_CANCEL_X (64)
#define POPUP_OK_WIDTH (20)
#define POPUP_CANCEL_WIDTH (40)

#define BACK_COLOR 0x0
#define FORE_COLOR 0x1

#define BOOT_EFFECT_SEED 0x5A17C0DEu
#define BOOT_EFFECT_BAR_Y 52
#define BOOT_EFFECT_BAR_HEIGHT 12
#define BOOT_EFFECT_SCAN_WIDTH 3

#define FATAL_LINE_CHARS 21
#define FATAL_LINE_SIZE (FATAL_LINE_CHARS + 1)

/*----- Static variable definitions ----------------------------------*/

static const u8 s_folder_icon_8x8[FILELIST_ICON_WIDTH] = {
	0x00, 0x60, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00
};

static UG_DEVICE s_device;
static UG_GUI s_ugui;
static u8 s_framebuffer[0x400];
static u32 s_boot_effect_rng = BOOT_EFFECT_SEED;
static view_t s_last_mode = VIEW_BROWSER;
static bool s_initialised;

/*----- Static function prototypes -----------------------------------*/

static void _draw_browser(const fb_view_t *view);
static void _draw_filelist(const fb_view_t *view, int y_offset);
static void _draw_sdcard_missing_notice(void);
static void _draw_popup(
	const char *title,
	const char *message,
	bool        show_buttons,
	bool        cancel_selected
);
static void _draw_modal_buttons(UG_S16 y_offset, bool cancel_selected);
static void _draw_modal_button(
	UG_S16      x,
	UG_S16      y,
	UG_S16      width,
	const char *label,
	bool        selected
);
static void _draw_selectable_name(
	int              y_offset,
	const fb_item_t *item,
	bool             selected
);
static void _draw_folder_icon(int y_offset, UG_COLOR forecolor);
static void _format_selectable_name(
	const fb_item_t *item,
	char            *disp_name,
	u16         disp_name_size
);
static void _append_text(char *dest, u16 dest_size, const char *src);
static void _refresh_screen(void);
static u32 _boot_effect_rand(void);
static u8 _boot_effect_progress_column(u32 done, u32 total);
static void _draw_boot_effect(u32 done, u32 total);
static void _draw_boot_effect_bar(u8 progress_column);
static void _draw_boot_effect_scan(u8 progress_column);
static void _xor_framebuffer_column(u8 x, u8 pattern);
static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c);
static u16 _read_le16(const u8 *data);
static u32 _read_le32(const u8 *data);
static void _draw_bitmap(int x, int y, const u8 *img, UG_COLOR forecolor);
static bool _copy_fatal_line(char *line, const char **message);
static bool _fatal_message_has_content(const char *message);
static void _ellipsize_fatal_line(char *line);
static void _format_fatal_register(char *line, const char *name, u32 value);

/*----- Extern function implementations ------------------------------*/

void gui_init(void) {

	s_initialised = false;

	s_device.x_dim = 128;
	s_device.y_dim = 64;
	s_device.pset = _put_pixel;

	UG_Init(&s_ugui, &s_device);
	UG_FontSelect(FONT_5X8);
	UG_ConsoleSetArea(4, 32, 123, 40);
	UG_ConsoleSetBackcolor(BACK_COLOR);
	UG_ConsoleSetForecolor(FORE_COLOR);
	UG_FillScreen(BACK_COLOR);

	s_initialised = true;

}

void gui_render(const gui_view_t *view) {

	bool progress_started;

	if ((NULL == view) || (NULL == view->browser)) {
		return;
	}

	progress_started = (VIEW_PROGRESS == view->mode) &&
		(VIEW_PROGRESS != s_last_mode);

	if ((VIEW_PROGRESS != view->mode) || progress_started) {
		_draw_browser(view->browser);
	}

	switch (view->mode) {

	case VIEW_MODAL:
		_draw_popup(
			view->modal.title,
			view->modal.message,
			(view->modal.type != MODAL_UNSKIPPABLE),
			view->modal.cancel_selected
		);
		break;

	case VIEW_PROGRESS:
		if (progress_started) {
			s_boot_effect_rng = BOOT_EFFECT_SEED;
		}
		_draw_boot_effect(view->progress_done, view->progress_total);
		break;

	case VIEW_BROWSER:
	default:
		break;

	}

	if ((VIEW_PROGRESS != view->mode) || progress_started) {
		UG_Update();
	}
	_refresh_screen();
	s_last_mode = view->mode;

}

bool gui_render_fatal(const char *message, const volatile arm_frame_t *frame) {

	char message_line_1[FATAL_LINE_SIZE];
	char message_line_2[FATAL_LINE_SIZE];
	char message_line_3[FATAL_LINE_SIZE];
	char register_line[FATAL_LINE_SIZE];
	const char *message_cursor = message ? message : "";
	bool message_continues;

	if (!s_initialised || !frame) {
		return false;
	}

	message_continues = _copy_fatal_line(message_line_1, &message_cursor);
	if (message_continues) {
		message_continues = _copy_fatal_line(message_line_2, &message_cursor);
	} else {
		message_line_2[0] = '\0';
	}

	if (message_continues) {
		message_continues = _copy_fatal_line(message_line_3, &message_cursor);
	} else {
		message_line_3[0] = '\0';
	}

	if (message_continues) {
		_ellipsize_fatal_line(message_line_3);
	}

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

	return dev_lcd_try_set_frame(s_framebuffer);
}

/*----- Static function implementations ------------------------------*/

static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c) {

	u16 byte_index;
	u16 bit_index;
	u8 byte;

	if ((pos_x < 0) || (pos_x >= s_device.x_dim) ||
		(pos_y < 0) || (pos_y >= s_device.y_dim)) {
		return;
	}

	byte_index = pos_x + ((pos_y >> 3) << 7);
	bit_index = pos_y & 7;
	byte = s_framebuffer[byte_index];

	s_framebuffer[byte_index] =
		(byte & ~(1UL << bit_index)) | (((bool)c) << bit_index);

}

static void _refresh_screen(void) {
	dev_lcd_set_frame(s_framebuffer);
}



static u16 _read_le16(const u8 *data) {
	return (u16)data[0] | ((u16)data[1] << 8);
}


static u32 _read_le32(const u8 *data) {
	return (u32)data[0] |
		((u32)data[1] << 8) |
		((u32)data[2] << 16) |
		((u32)data[3] << 24);
}


static void _draw_bitmap(int x, int y, const u8 *img, UG_COLOR forecolor) {

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


static void _draw_browser(const fb_view_t *view) {

	_draw_filelist(view, 0);

	if (!view->sdcard_inserted) {
		_draw_sdcard_missing_notice();
	}

}

static void _draw_filelist(const fb_view_t *view, int y_offset) {

	for (u16 i = 0; i < view->count; i++) {
		_draw_selectable_name(y_offset, &view->items[i], i == view->selected);
		y_offset += FILELIST_ROW_HEIGHT;
	}

	if (y_offset < s_device.y_dim) {
		UG_FillFrame(
			FILELIST_X,
			y_offset,
			FILELIST_X + FILELIST_WIDTH - 1,
			s_device.y_dim - 1,
			BACK_COLOR
		);
	}

}

static void _draw_sdcard_missing_notice(void) {

	UG_SetForecolor(FORE_COLOR);
	UG_SetBackcolor(BACK_COLOR);
	UG_PutString(FILELIST_X, FILELIST_ROW_HEIGHT * 2, "No SD card");
	UG_PutString(FILELIST_X, FILELIST_ROW_HEIGHT * 3, "detected");

}

static void _draw_popup(
	const char *title,
	const char *message,
	bool        show_buttons,
	bool        cancel_selected
) {

	char popup_line[POPUP_MESSAGE_LINE_MAX];
	u8 message_lines = 1;
	UG_S16 popup_height = POPUP_HEIGHT;
	UG_S16 text_y = POPUP_Y + POPUP_MESSAGE_Y_OFFSET;
	UG_S16 text_y_max;
	UG_S16 button_y = 0;
	UG_S16 title_x;
	u16 title_width;

	if (NULL == title) {
		title = "";
	}
	if (NULL == message) {
		message = "";
	}

	for (const char *cursor = message; *cursor; cursor++) {
		if ('\n' == *cursor) {
			message_lines++;
		}
	}

	popup_height += (message_lines - 1) * FILELIST_ROW_HEIGHT;
	if (show_buttons) {
		popup_height += FILELIST_ROW_HEIGHT;
	}
	if ((POPUP_Y + popup_height) > s_device.y_dim) {
		popup_height = s_device.y_dim - POPUP_Y;
	}

	if (show_buttons) {
		button_y = POPUP_Y + popup_height - FILELIST_ROW_HEIGHT;
		text_y_max = button_y - FILELIST_ROW_HEIGHT;
	} else {
		text_y_max = POPUP_Y + popup_height - FILELIST_ROW_HEIGHT;
	}

	UG_FillFrame(
		POPUP_X + 1,
		POPUP_Y + 1,
		POPUP_X + POPUP_WIDTH - 1,
		POPUP_Y + popup_height - 1,
		FORE_COLOR
	);
	UG_DrawFrame(
		POPUP_X,
		POPUP_Y,
		POPUP_X + POPUP_WIDTH - 1,
		POPUP_Y + popup_height - 1,
		BACK_COLOR
	);
	UG_DrawFrame(
		POPUP_X - 1,
		POPUP_Y - 1,
		POPUP_X + POPUP_WIDTH - 2,
		POPUP_Y + popup_height - 2,
		BACK_COLOR
	);
	UG_SetForecolor(BACK_COLOR);
	UG_SetBackcolor(FORE_COLOR);
	title_width = strlen(title) * 5;
	title_x = POPUP_X + 4;
	if (title_width < (POPUP_WIDTH - 8)) {
		title_x = POPUP_X + ((POPUP_WIDTH - title_width) / 2);
	}
	UG_PutString(title_x, POPUP_Y + 4, (char*)title);

	while (*message && (text_y <= text_y_max)) {
		u8 line_len = 0;

		while (('\0' != message[line_len]) &&
			   ('\n' != message[line_len]) &&
			   (line_len < (POPUP_MESSAGE_LINE_MAX - 1))) {
			popup_line[line_len] = message[line_len];
			line_len++;
		}
		popup_line[line_len] = '\0';

		UG_PutString(POPUP_X + 4, text_y, popup_line);

		message += line_len;
		while (('\0' != *message) && ('\n' != *message)) {
			message++;
		}
		if ('\n' == *message) {
			message++;
		}
		text_y += FILELIST_ROW_HEIGHT;
	}

	if (show_buttons) {
		_draw_modal_buttons(button_y, cancel_selected);
	}

}

static void _draw_modal_buttons(UG_S16 y_offset, bool cancel_selected) {

	_draw_modal_button(
		POPUP_OK_X,
		y_offset,
		POPUP_OK_WIDTH,
		" OK ",
		!cancel_selected
	);
	_draw_modal_button(
		POPUP_CANCEL_X,
		y_offset,
		POPUP_CANCEL_WIDTH,
		" Cancel ",
		cancel_selected
	);

}

static void _draw_modal_button(
	UG_S16      x,
	UG_S16      y,
	UG_S16      width,
	const char *label,
	bool        selected
) {

	UG_COLOR forecolor = selected ? FORE_COLOR : BACK_COLOR;
	UG_COLOR backcolor = selected ? BACK_COLOR : FORE_COLOR;

	UG_SetForecolor(forecolor);
	UG_SetBackcolor(backcolor);
	UG_FillFrame(x, y, x + width - 1, y + FILELIST_ROW_HEIGHT - 1, backcolor);
	UG_PutString(x, y, (char*)label);

}

static void _draw_selectable_name(
	int              y_offset,
	const fb_item_t *item,
	bool             selected
) {

	char disp_name[DISP_NAME_MAX];
	UG_S16 text_x = FILELIST_X;
	UG_COLOR forecolor = selected ? BACK_COLOR : FORE_COLOR;
	UG_COLOR backcolor = selected ? FORE_COLOR : BACK_COLOR;

	_format_selectable_name(item, disp_name, DISP_NAME_MAX);

	UG_SetForecolor(forecolor);
	UG_SetBackcolor(backcolor);
	UG_FillFrame(
		FILELIST_X,
		y_offset,
		FILELIST_X + FILELIST_WIDTH - 1,
		y_offset + FILELIST_ROW_HEIGHT - 1,
		backcolor
	);

	if (FB_ITEM_DIR == item->type) {
		_draw_folder_icon(y_offset, forecolor);
		text_x += FILELIST_ICON_WIDTH;
	}

	UG_PutString(text_x, y_offset, disp_name);

}

static void _draw_folder_icon(int y_offset, UG_COLOR forecolor) {

	u16 page = y_offset >> 3;
	u16 index = FILELIST_X + (page << 7);

	for (u16 i = 0; i < FILELIST_ICON_WIDTH; i++) {
		s_framebuffer[index + i] =
			(FORE_COLOR == forecolor) ?
				s_folder_icon_8x8[i] :
				(u8)~s_folder_icon_8x8[i];
	}

}

static void _format_selectable_name(
	const fb_item_t *item,
	char            *disp_name,
	u16         disp_name_size
) {

	memset(disp_name, 0, disp_name_size);
	_append_text(disp_name, disp_name_size, item->name);

}

static void _append_text(char *dest, u16 dest_size, const char *src) {

	u16 len = strlen(dest);

	if (len >= (dest_size - 1)) {
		return;
	}

	strncat(dest, src, dest_size - len - 1);

}

static u32 _boot_effect_rand(void) {

	s_boot_effect_rng ^= s_boot_effect_rng << 13;
	s_boot_effect_rng ^= s_boot_effect_rng >> 17;
	s_boot_effect_rng ^= s_boot_effect_rng << 5;
	return s_boot_effect_rng;

}

static u8 _boot_effect_progress_column(u32 done, u32 total) {

	uint64_t scaled;

	if (0 == total) {
		return 0;
	}

	if (done >= total) {
		return s_device.x_dim;
	}

	scaled = (uint64_t)done * s_device.x_dim;
	return (u8)(scaled / total);

}

static void _draw_boot_effect(u32 done, u32 total) {

	u8 progress_column = _boot_effect_progress_column(done, total);

	_draw_boot_effect_bar(progress_column);
	_draw_boot_effect_scan(progress_column);

}

static void _draw_boot_effect_bar(u8 progress_column) {

	UG_FillFrame(
		0,
		BOOT_EFFECT_BAR_Y,
		s_device.x_dim - 1,
		BOOT_EFFECT_BAR_Y + BOOT_EFFECT_BAR_HEIGHT - 1,
		BACK_COLOR
	);

	for (u8 x = 0; x < s_device.x_dim; x++) {
		bool active = x < progress_column;

		for (u8 y = BOOT_EFFECT_BAR_Y;
			 y < BOOT_EFFECT_BAR_Y + BOOT_EFFECT_BAR_HEIGHT;
			 y++) {
			bool pixel_on = active ?
				(((x + y) & 0x03) != 0) :
				(0 == ((x + y) & 0x0f));
			_put_pixel(x, y, pixel_on ? FORE_COLOR : BACK_COLOR);
		}
	}

}

static void _draw_boot_effect_scan(u8 progress_column) {

	u8 lead = (progress_column < s_device.x_dim) ?
		progress_column :
		s_device.x_dim - 1;
	u8 phase = _boot_effect_rand() & 0x07;

	for (u8 i = 0; i < BOOT_EFFECT_SCAN_WIDTH; i++) {
		u8 x = (lead + i < s_device.x_dim) ?
			(lead + i) :
			(s_device.x_dim - 1);
		_xor_framebuffer_column(x, 0xAAu >> (i & 1));
	}

	for (u8 i = 0; i < 4; i++) {
		u8 x = (u8)(
			(_boot_effect_rand() + phase + (i * 23u)) & 0x7fu
		);
		_xor_framebuffer_column(x, (0x81u >> (i & 1)) | (0x18u << (i & 1)));
	}

}

static void _xor_framebuffer_column(u8 x, u8 pattern) {

	for (u16 page = 0; page < (s_device.y_dim >> 3); page++) {
		s_framebuffer[x + (page << 7)] ^= pattern;
	}

}

static bool _copy_fatal_line(char *line, const char **message) {

	const char *cursor = *message;
	const char *wrap_cursor = NULL;
	u8 length = 0;
	u8 wrap_length = 0;

	while ((' ' == *cursor) || ('\t' == *cursor) ||
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

static bool _fatal_message_has_content(const char *message) {

	while ((' ' == *message) || ('\t' == *message) ||
		   ('\r' == *message) || ('\n' == *message)) {
		message++;
	}

	return '\0' != *message;
}

static void _ellipsize_fatal_line(char *line) {

	u8 length = strlen(line);

	if (length > (FATAL_LINE_CHARS - 3)) {
		length = FATAL_LINE_CHARS - 3;
	}

	line[length++] = '.';
	line[length++] = '.';
	line[length++] = '.';
	line[length] = '\0';
}

static void _format_fatal_register(char *line, const char *name, u32 value) {

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
