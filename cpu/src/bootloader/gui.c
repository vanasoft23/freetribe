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

/*----- Includes -----------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ugui.h>
#include "dev_lcd.h"
#include "macros.h"
#include "file_browser.h"

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

#define BACK_COLOR 0x0
#define FORE_COLOR 0x1

#define BOOT_EFFECT_SEED 0x5A17C0DEu
#define BOOT_EFFECT_BAR_Y 52
#define BOOT_EFFECT_BAR_HEIGHT 12
#define BOOT_EFFECT_SCAN_WIDTH 3

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    GUI_SELECTABLE_FILE,
    GUI_SELECTABLE_DIR,
    GUI_SELECTABLE_PARENT_DIR,
    GUI_SELECTABLE_BOOT_FLASH
} gui_selectable_type_t;

typedef struct {
    gui_selectable_type_t type;
    const char           *name;
    bool                  selected;
} gui_selectable_name_t;

/*----- Static variable definitions ----------------------------------*/

static const uint8_t s_folder_icon_8x8[FILELIST_ICON_WIDTH] = {
    0x00, 0x60, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00
};

static UG_DEVICE s_device;
static UG_GUI    s_ugui;
static UG_WINDOW s_wnd;

static uint8_t s_framebuffer[0x400];
static uint32_t s_boot_effect_rng = BOOT_EFFECT_SEED;

/*----- Static function prototypes -----------------------------------*/

static void _draw_filelist(int y_offset);
static void _draw_popup(const char *title, const char *message);
static void _draw_sdcard_missing_notice(void);
static void _draw_selectable_name(int y_offset, const gui_selectable_name_t *item);
static void _draw_folder_icon(int y_offset, UG_COLOR forecolor);
static void _format_selectable_name(
    const gui_selectable_name_t *item,
    char                       *disp_name,
    uint16_t                    disp_name_size
);
static void _append_text(char *dest, uint16_t dest_size, const char *src);
static void _refresh_screen(void);
static uint32_t _boot_effect_rand(void);
static uint8_t _boot_effect_progress_column(uint32_t done, uint32_t total);
static void _draw_boot_effect(uint32_t done, uint32_t total);
static void _draw_boot_effect_bar(uint8_t progress_column);
static void _draw_boot_effect_scan(uint8_t progress_column);
static void _xor_framebuffer_column(uint8_t x, uint8_t pattern);
static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c);

/*----- Extern function implementations ------------------------------*/

void gui_init(void) {

    s_device.x_dim = 128;
    s_device.y_dim = 64;
    s_device.pset  = _put_pixel;

    UG_Init(&s_ugui, &s_device);

    // UG_DriverRegister(DRIVER_FILL_FRAME, svc_display_fill_frame);

    UG_FontSelect(FONT_5X8);
    UG_ConsoleSetArea(4, 32, 123, 40);
    UG_ConsoleSetBackcolor(BACK_COLOR);
    UG_ConsoleSetForecolor(FORE_COLOR);
    UG_FillScreen(BACK_COLOR);

}

void gui_tick(void) {
    
    const fb_view_t *view = file_browser_get_view();
    int y_offset = 0;
    _draw_filelist(y_offset);

    if (!view->sdcard_inserted) {
        _draw_sdcard_missing_notice();
    }

    if (view->error_active) {
        _draw_popup("ERROR :(", view->error_message);
    }

    UG_Update();
    _refresh_screen();
}

void gui_show_message_box(const char *title, const char *message) {

    _draw_popup(title, message);
    UG_Update();
    _refresh_screen();

}

void gui_boot_effect_begin(void) {

    s_boot_effect_rng = BOOT_EFFECT_SEED;
    gui_tick();

}

void gui_boot_effect_step(uint32_t done, uint32_t total) {

    _draw_boot_effect(done, total);
    _refresh_screen();

}

/*----- Static function implementations ------------------------------*/

static void _put_pixel(UG_S16 pos_x, UG_S16 pos_y, UG_COLOR c) {

    uint16_t byte_index = pos_x + ((pos_y >> 3) << 7);
    uint16_t bit_index = pos_y & 7;

    // Get current byte from frame buffer.
    uint8_t byte = s_framebuffer[byte_index];

    // Set pixel bit and write to frame buffer.
    s_framebuffer[byte_index] =
        (byte & ~(1UL << bit_index)) | (((bool)c) << bit_index);

}

static void _refresh_screen(void) {

    dev_lcd_set_frame(s_framebuffer);

}

static uint32_t _boot_effect_rand(void) {

    s_boot_effect_rng ^= s_boot_effect_rng << 13;
    s_boot_effect_rng ^= s_boot_effect_rng >> 17;
    s_boot_effect_rng ^= s_boot_effect_rng << 5;
    return s_boot_effect_rng;

}

static uint8_t _boot_effect_progress_column(uint32_t done, uint32_t total) {

    uint64_t scaled;

    if (0 == total) {
        return 0;
    }

    if (done >= total) {
        return s_device.x_dim;
    }

    scaled = (uint64_t)done * s_device.x_dim;
    return (uint8_t)(scaled / total);

}

static void _draw_boot_effect(uint32_t done, uint32_t total) {

    uint8_t progress_column = _boot_effect_progress_column(done, total);

    _draw_boot_effect_bar(progress_column);
    _draw_boot_effect_scan(progress_column);

}

static void _draw_boot_effect_bar(uint8_t progress_column) {

    UG_FillFrame(
        0,
        BOOT_EFFECT_BAR_Y,
        s_device.x_dim - 1,
        BOOT_EFFECT_BAR_Y + BOOT_EFFECT_BAR_HEIGHT - 1,
        BACK_COLOR
    );

    for (uint8_t x = 0; x < s_device.x_dim; x++) {
        bool active = x < progress_column;

        for (uint8_t y = BOOT_EFFECT_BAR_Y;
             y < BOOT_EFFECT_BAR_Y + BOOT_EFFECT_BAR_HEIGHT;
             y++) {
            bool pixel_on = active ? (((x + y) & 0x03) != 0) : (0 == ((x + y) & 0x0f));
            _put_pixel(x, y, pixel_on ? FORE_COLOR : BACK_COLOR);
        }
    }

}

static void _draw_boot_effect_scan(uint8_t progress_column) {

    uint8_t lead = (progress_column < s_device.x_dim) ? progress_column : s_device.x_dim - 1;
    uint8_t phase = _boot_effect_rand() & 0x07;

    for (uint8_t i = 0; i < BOOT_EFFECT_SCAN_WIDTH; i++) {
        uint8_t x = (lead + i < s_device.x_dim) ? (lead + i) : (s_device.x_dim - 1);
        _xor_framebuffer_column(x, 0xAAu >> (i & 1));
    }

    for (uint8_t i = 0; i < 4; i++) {
        uint8_t x = (uint8_t)((_boot_effect_rand() + phase + (i * 23u)) & 0x7fu);
        _xor_framebuffer_column(x, (0x81u >> (i & 1)) | (0x18u << (i & 1)));
    }

}

static void _xor_framebuffer_column(uint8_t x, uint8_t pattern) {

    for (uint16_t page = 0; page < (s_device.y_dim >> 3); page++) {
        s_framebuffer[x + (page << 7)] ^= pattern;
    }

}

static void _draw_filelist(int y_offset) {

    const fb_view_t *view = file_browser_get_view();

    for (uint16_t i = 0; i < view->count; i++) {
        const fb_item_t *item = &view->items[i];
        gui_selectable_name_t row = {
            .type     = GUI_SELECTABLE_FILE,
            .name     = item->name,
            .selected = (i == view->selected)
        };

        switch (item->type) {

        case FB_ITEM_DIR:
            row.type = GUI_SELECTABLE_DIR;
            break;

        case FB_ITEM_ROOT_DIR:
            row.type = GUI_SELECTABLE_FILE;
            break;

        case FB_ITEM_PARENT_DIR:
            row.type = GUI_SELECTABLE_PARENT_DIR;
            break;

        case FB_ITEM_BOOT_FLASH:
            row.type = GUI_SELECTABLE_BOOT_FLASH;
            break;

        case FB_ITEM_FILE:
        default:
            row.type = GUI_SELECTABLE_FILE;
            break;

        }

        _draw_selectable_name(y_offset, &row);
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

static void _draw_popup(const char *title, const char *message) {

    char popup_line[POPUP_MESSAGE_LINE_MAX];
    uint8_t message_lines = 1;
    UG_S16 popup_height = POPUP_HEIGHT;
    UG_S16 text_y = POPUP_Y + POPUP_MESSAGE_Y_OFFSET;
    UG_S16 text_y_max;

    for (const char *cursor = message; *cursor; cursor++) {
        if ('\n' == *cursor) {
            message_lines++;
        }
    }

    popup_height += (message_lines - 1) * FILELIST_ROW_HEIGHT;
    if ((POPUP_Y + popup_height) > s_device.y_dim) {
        popup_height = s_device.y_dim - POPUP_Y;
    }
    text_y_max = POPUP_Y + popup_height - FILELIST_ROW_HEIGHT;

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
    UG_PutString(POPUP_X + 29, POPUP_Y + 4, (char*)title);

    while (*message && (text_y <= text_y_max)) {
        uint8_t line_len = 0;

        while (('\0' != message[line_len])
            && ('\n' != message[line_len])
            && (line_len < (POPUP_MESSAGE_LINE_MAX - 1))) {
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

}

static void _draw_selectable_name(int y_offset, const gui_selectable_name_t *item) {

    char disp_name[DISP_NAME_MAX];
    UG_S16 text_x = FILELIST_X;
    UG_COLOR forecolor = item->selected ? BACK_COLOR : FORE_COLOR;
    UG_COLOR backcolor = item->selected ? FORE_COLOR : BACK_COLOR;

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
    if (GUI_SELECTABLE_DIR == item->type) {
        _draw_folder_icon(y_offset, forecolor);
        text_x += FILELIST_ICON_WIDTH;
    }
    UG_PutString(text_x, y_offset, disp_name);

}

static void _draw_folder_icon(int y_offset, UG_COLOR forecolor) {

    uint16_t page = y_offset >> 3;
    uint16_t index = FILELIST_X + (page << 7);

    for (uint16_t i = 0; i < FILELIST_ICON_WIDTH; i++) {
        s_framebuffer[index + i] =
            (FORE_COLOR == forecolor) ? s_folder_icon_8x8[i] : ~s_folder_icon_8x8[i];
    }

}

static void _format_selectable_name(
    const gui_selectable_name_t *item,
    char                       *disp_name,
    uint16_t                    disp_name_size
) {

    memset(disp_name, 0, disp_name_size);

    switch (item->type) {

    case GUI_SELECTABLE_DIR:
        _append_text(disp_name, disp_name_size, item->name);
        break;

    case GUI_SELECTABLE_PARENT_DIR:
        _append_text(disp_name, disp_name_size, "..");
        break;

    case GUI_SELECTABLE_BOOT_FLASH:
        _append_text(disp_name, disp_name_size, "Boot from flash");
        break;

    case GUI_SELECTABLE_FILE:
    default:
        _append_text(disp_name, disp_name_size, item->name);
        break;

    }

}

static void _append_text(char *dest, uint16_t dest_size, const char *src) {

    uint16_t len = strlen(dest);

    if (len >= (dest_size - 1)) {
        return;
    }

    strncat(dest, src, dest_size - len - 1);

}
