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

#include "bootloader/ffconf.h"
#include <ff.h>
#include <string.h>
#include "dev_flash.h"
#include "dev_sdcard.h"
#include "svc_panel.h"
#include "file_browser.h"
#include "gui.h"
#include "macros.h"
#include "handoff.h"
#include "bootloader.h"
#include "boot_image.h"
#include "flash_bootloader.h"

/*----- Macros -------------------------------------------------------*/

#define BUTTON_ENTER 0x09
#define ENCODER_MAIN 0x00

#define FLASH_TOTAL_SIZE (16*1024*1024)

#define FIRMWARE_SIZE (2*1024*1024)
#define FIRMWARE_FLASH_ADDR 0x00020000u
#define FIRMWARE_DEST_ADDR 0xC0000000u
#define FB_CACHE_MAX_ITEMS 512
#define FB_SDCARD_MOUNT_RETRY_TICKS 256
#define FIRMWARE_READ_CHUNK_SIZE (64*1024u)

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".ddr_data")))
    static uint8_t s_flash_dump_buffer[FLASH_TOTAL_SIZE];

static fb_view_t s_view = {
    .cdir = "/"
};

static fb_item_t s_item_cache[FB_CACHE_MAX_ITEMS];
static uint16_t s_selected_item;
static uint16_t s_total_items;
static bool s_last_sdcard_present;
static bool s_sdcard_mounted;
static uint16_t s_sdcard_mount_retry_ticks;

/*----- Extern variable definitions ----------------------------------*/

extern FATFS g_fatfs;

/*----- Static function prototypes -----------------------------------*/

static bool _load_current_dir(void);
static fb_item_t *_append_cached_item(
    fb_item_type_t type,
    const char    *name,
    uint32_t       size
);
static void _move_last_item_before_files(void);
static void _append_special_items(void);
static void _show_flash_only_view(void);
static bool _mount_sdcard(void);
static void _unmount_sdcard(void);
static void _poll_sdcard(void);
static void _refresh_view(void);
static void _sync_scroll_to_selection(void);
static bool _is_root_dir(const char *path);
static bool _is_dot_entry(const char *name);
static bool _has_extension(const char *name, const char *extension);
static char _to_upper(char c);
static void _set_error(const char *message);
static void _clear_error(void);
static bool _set_current_dir(const char *path);
static void _build_child_path(char *dest, uint16_t dest_size, const char *name);
static void _open_child_dir(const char *name);
static void _open_parent_dir(void);
static bool _detect_boot_file(
    FIL               *file,
    const fb_item_t   *item,
    boot_image_info_t *boot_image
);
static bool _read_exact(FIL *file, void *dest, UINT size);
static bool _seek_file(FIL *file, FSIZE_t offset);
static bool _read_sd_payload(FIL *file, const TCHAR *path, uint32_t size);
static uint32_t _firmware_read_chunk_size(uint32_t offset, uint32_t total);
static bool _load_boot_file(
    const TCHAR         *path,
    const fb_item_t    *item,
    boot_image_info_t  *boot_image
);
static void _read_flash_app(void);
static bool _dump_flash_to_sdcard(void);
static void _action_next(void);
static void _action_prev(void);
static void _action_open(void);
static void _encoder_callback(uint8_t index, int8_t value);
static void _button_callback(uint8_t index, bool state);

/*----- Extern function implementations ------------------------------*/

void file_browser_init(void) {

    s_last_sdcard_present = dev_sdcard_present();
    s_sdcard_mount_retry_ticks = 0;

    _show_flash_only_view();

    if (s_last_sdcard_present) {
        _mount_sdcard();
    }

    svc_panel_register_callback(ENCODER_EVENT, _encoder_callback);
    svc_panel_register_callback(BUTTON_EVENT, _button_callback);

}

const fb_view_t *file_browser_get_view(void) {
    return &s_view;
}

void file_browser_tick(void) {
    svc_panel_process();
    _poll_sdcard();
}

/*----- Static function implementations ------------------------------*/

static bool _load_current_dir(void) {

    static DIR     dir;
    static FILINFO fno;
    FRESULT        res;

    res = f_opendir(&dir, s_view.cdir);  
    if (FR_OK != res) {
        DEBUG_LOG("f_opendir failed: %i", (int)res);
        _unmount_sdcard();
        return false;
    }

    s_selected_item = 0;
    s_view.scroll = 0;
    s_view.selected = 0;
    s_total_items = 0;
    _append_special_items();
    
    while (s_total_items < FB_CACHE_MAX_ITEMS) {
        res = f_readdir(&dir, &fno);
        if (FR_OK != res) {
            DEBUG_LOG("f_readdir failed: %d", res);
            f_closedir(&dir);
            _unmount_sdcard();
            return false;
        }
        
        // end of directory
        if (0 == fno.fname[0]) {
            break;
        }
        if (_is_dot_entry(fno.fname)) {
            continue;
        }
        if (fno.fattrib & AM_HID) {
            continue;
        }

        if (fno.fattrib & AM_DIR) {
            _append_cached_item(FB_ITEM_DIR, fno.fname, (uint32_t)fno.fsize);
            _move_last_item_before_files();
        } else {
            _append_cached_item(FB_ITEM_FILE, fno.fname, (uint32_t)fno.fsize);
        }

    }

    f_closedir(&dir);
    _refresh_view();
    return true;

}

static fb_item_t *_append_cached_item(
    fb_item_type_t type,
    const char    *name,
    uint32_t       size
) {

    if (s_total_items >= FB_CACHE_MAX_ITEMS) {
        return NULL;
    }

    fb_item_t *item = &s_item_cache[s_total_items];
    item->type = type;
    item->is_dir = (FB_ITEM_DIR == type)
        || (FB_ITEM_ROOT_DIR == type)
        || (FB_ITEM_PARENT_DIR == type);
    strncpy(item->name, name, FB_NAME_MAX - 1);
    item->name[FB_NAME_MAX - 1] = '\0';
    item->size = size;
    s_total_items++;

    return item;

}

static void _move_last_item_before_files(void) {

    fb_item_t item;
    uint16_t insert_index;

    if (s_total_items < 2) {
        return;
    }

    for (insert_index = 0; insert_index < (s_total_items - 1); insert_index++) {
        if (FB_ITEM_FILE == s_item_cache[insert_index].type) {
            break;
        }
    }

    if (insert_index >= (s_total_items - 1)) {
        return;
    }

    item = s_item_cache[s_total_items - 1];
    for (uint16_t i = s_total_items - 1; i > insert_index; i--) {
        s_item_cache[i] = s_item_cache[i - 1];
    }
    s_item_cache[insert_index] = item;

}

static void _append_special_items(void) {

    _append_cached_item(FB_ITEM_BOOT_FLASH, "Boot from flash", 0);
    _append_cached_item(FB_ITEM_INSTALL_BOOTLOADER, "Install bootloader", 0);
    _append_cached_item(FB_ITEM_DUMP_FLASH, "Dump flash to SD", 0);
    if (_is_root_dir(s_view.cdir)) {
        _append_cached_item(FB_ITEM_ROOT_DIR, ".", 0);
    } else {
        _append_cached_item(FB_ITEM_PARENT_DIR, "..", 0);
    }

}

static void _show_flash_only_view(void) {

    s_sdcard_mounted = false;
    s_view.sdcard_inserted = dev_sdcard_present();
    s_selected_item = 0;
    s_view.scroll = 0;
    s_view.selected = 0;
    s_total_items = 0;
    _clear_error();
    strncpy(s_view.cdir, "/", FB_NAME_MAX - 1);
    s_view.cdir[FB_NAME_MAX - 1] = '\0';

    _append_cached_item(FB_ITEM_BOOT_FLASH, "Boot from flash", 0);
    _append_cached_item(FB_ITEM_INSTALL_BOOTLOADER, "Install bootloader", 0);
    _refresh_view();

}

static bool _mount_sdcard(void) {

    FRESULT res;

    if (!dev_sdcard_present()) {
        _show_flash_only_view();
        return false;
    }

    res = f_mount(&g_fatfs, "", 1);
    if (FR_OK != res) {
        DEBUG_LOG("f_mount failed: %i", (int)res);
        _show_flash_only_view();
        return false;
    }

    s_view.sdcard_inserted = true;
    s_sdcard_mounted = true;
    s_sdcard_mount_retry_ticks = 0;
    return _set_current_dir("/");

}

static void _unmount_sdcard(void) {

    f_mount(0, "", 0);
    s_sdcard_mounted = false;
    s_sdcard_mount_retry_ticks = 0;
    _show_flash_only_view();

}

static void _poll_sdcard(void) {

    bool present = dev_sdcard_present();

    if (present != s_last_sdcard_present) {
        s_last_sdcard_present = present;
        s_sdcard_mount_retry_ticks = 0;

        if (present) {
            _mount_sdcard();
        } else {
            _unmount_sdcard();
        }
        return;
    }

    if (!present || s_sdcard_mounted) {
        return;
    }

    s_sdcard_mount_retry_ticks++;
    if (s_sdcard_mount_retry_ticks >= FB_SDCARD_MOUNT_RETRY_TICKS) {
        s_sdcard_mount_retry_ticks = 0;
        _mount_sdcard();
    }

}

static void _refresh_view(void) {

    s_view.count = 0;

    if ((s_total_items > 0) && (s_selected_item >= s_total_items)) {
        s_selected_item = s_total_items - 1;
    }

    _sync_scroll_to_selection();

    for (uint16_t i = s_view.scroll;
         (i < s_total_items) && (s_view.count < FB_MAX_ITEMS);
         i++) {
        s_view.items[s_view.count] = s_item_cache[i];
        s_view.count++;
    }

    s_view.selected = s_selected_item - s_view.scroll;

}

static void _sync_scroll_to_selection(void) {

    if (s_selected_item < s_view.scroll) {
        s_view.scroll = s_selected_item;
    } else if (s_selected_item >= (s_view.scroll + FB_MAX_ITEMS)) {
        s_view.scroll = s_selected_item - FB_MAX_ITEMS + 1;
    }

}

static bool _is_root_dir(const char *path) {
    return (0 == strcmp(path, "/"));
}

static bool _is_dot_entry(const char *name) {
    return (0 == strcmp(name, ".")) || (0 == strcmp(name, ".."));
}

static bool _has_extension(const char *name, const char *extension) {

    uint16_t name_len = strlen(name);
    uint16_t ext_len = strlen(extension);

    if (name_len < ext_len) {
        return false;
    }

    name += name_len - ext_len;
    while (*extension) {
        if (_to_upper(*name) != _to_upper(*extension)) {
            return false;
        }
        name++;
        extension++;
    }

    return true;

}

static char _to_upper(char c) {

    if ((c >= 'a') && (c <= 'z')) {
        return c - ('a' - 'A');
    }

    return c;

}

static void _set_error(const char *message) {

    strncpy(s_view.error_message, message, FB_ERROR_MAX - 1);
    s_view.error_message[FB_ERROR_MAX - 1] = '\0';
    s_view.error_active = true;

}

static void _clear_error(void) {

    s_view.error_active = false;
    s_view.error_message[0] = '\0';

}

static bool _set_current_dir(const char *path) {

    strncpy(s_view.cdir, path, FB_NAME_MAX - 1);
    s_view.cdir[FB_NAME_MAX - 1] = '\0';
    return _load_current_dir();

}

static void _build_child_path(char *dest, uint16_t dest_size, const char *name) {

    uint16_t len;

    memset(dest, 0, dest_size);

    if (_is_root_dir(s_view.cdir)) {
        strncpy(dest, "/", dest_size - 1);
    } else {
        strncpy(dest, s_view.cdir, dest_size - 1);
        dest[dest_size - 1] = '\0';

        len = strlen(dest);
        if (len < (dest_size - 1)) {
            strncat(dest, "/", dest_size - len - 1);
        }
    }

    len = strlen(dest);
    if (len < (dest_size - 1)) {
        strncat(dest, name, dest_size - len - 1);
    }

}

static void _open_child_dir(const char *name) {

    char path[FB_NAME_MAX];

    _build_child_path(path, FB_NAME_MAX, name);
    DEBUG_LOG("Listing files %s", path);
    _set_current_dir(path);

}

static void _open_parent_dir(void) {

    char path[FB_NAME_MAX];
    char *last_separator;

    strncpy(path, s_view.cdir, FB_NAME_MAX - 1);
    path[FB_NAME_MAX - 1] = '\0';

    last_separator = strrchr(path, '/');
    if ((NULL == last_separator) || (last_separator == path)) {
        _set_current_dir("/");
        return;
    }

    *last_separator = '\0';
    _set_current_dir(path);

}

static bool _detect_boot_file(
    FIL               *file,
    const fb_item_t   *item,
    boot_image_info_t *boot_image
) {

    uint8_t prefix[BOOT_IMAGE_VSB_HEADER_SIZE];
    uint8_t trailer[BOOT_IMAGE_FREETRIBE_TRAILER_SIZE];
    uint32_t prefix_size = item->size;
    uint32_t trailer_size = 0;
    const uint8_t *trailer_ptr = NULL;

    if (!_has_extension(item->name, ".bin") &&
        !_has_extension(item->name, ".VSB")) {
        return false;
    }

    if (prefix_size > BOOT_IMAGE_VSB_HEADER_SIZE) {
        prefix_size = BOOT_IMAGE_VSB_HEADER_SIZE;
    }

    if (!_seek_file(file, 0) ||
        ((prefix_size > 0) && !_read_exact(file, prefix, (UINT)prefix_size))) {
        return false;
    }

    if (item->size >= BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) {
        trailer_size = BOOT_IMAGE_FREETRIBE_TRAILER_SIZE;
        trailer_ptr = trailer;

        if (!_seek_file(file, item->size - BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) ||
            !_read_exact(file, trailer, BOOT_IMAGE_FREETRIBE_TRAILER_SIZE)) {
            return false;
        }
    }

    return boot_image_classify_parts(
        prefix,
        prefix_size,
        trailer_ptr,
        trailer_size,
        item->size,
        boot_image
    );

}

static bool _read_exact(FIL *file, void *dest, UINT size) {

    FRESULT res;
    UINT bytes_read;

    res = f_read(file, dest, size, &bytes_read);
    if (FR_OK != res) {
        error("f_read (%d)", res);
    }

    return bytes_read == size;

}

static bool _seek_file(FIL *file, FSIZE_t offset) {

    FRESULT res = f_lseek(file, offset);

    if (FR_OK != res) {
        error("f_lseek (%d)", res);
    }

    return FR_OK == res;

}

static bool _read_sd_payload(FIL *file, const TCHAR *path, uint32_t size) {

    uint8_t *dest = (uint8_t*)FIRMWARE_DEST_ADDR;
    uint32_t offset = 0;

    gui_boot_effect_begin();

    while (offset < size) {
        FRESULT res;
        UINT bytes_read = 0;
        uint32_t chunk_size = _firmware_read_chunk_size(offset, size);

        res = f_read(file, dest + offset, (UINT)chunk_size, &bytes_read);
        if ((FR_OK != res) || (bytes_read != chunk_size)) {
            DEBUG_LOG(
                "firmware read failed: %s res=%d bytes=%u/%u",
                path,
                (int)res,
                (unsigned)bytes_read,
                (unsigned)chunk_size
            );
            _set_error("Read failed");
            return false;
        }

        offset += chunk_size;
        gui_boot_effect_step(offset, size);
    }

    return true;

}

static uint32_t _firmware_read_chunk_size(uint32_t offset, uint32_t total) {

    uint32_t remaining = total - offset;

    if (remaining > FIRMWARE_READ_CHUNK_SIZE) {
        return FIRMWARE_READ_CHUNK_SIZE;
    }

    return remaining;

}

static bool _load_boot_file(
    const TCHAR         *path,
    const fb_item_t    *item,
    boot_image_info_t  *boot_image
) {

    FRESULT res;
    FIL file;

    res = f_open(&file, path, FA_READ);
    if (FR_OK != res) {
        error("f_open (%d)", res);
    }

    if (!_detect_boot_file(&file, item, boot_image)) {
        DEBUG_LOG("not bootable: %s", path);
        _set_error("Invalid firmware");
        f_close(&file);
        return false;
    }

    DEBUG_LOG(
        "Boot file type: %s offset=%u payload=%u",
        boot_image_type_name(boot_image->type),
        (unsigned)boot_image->payload_offset,
        (unsigned)boot_image->payload_size
    );

    if (!_seek_file(&file, boot_image->payload_offset) ||
        !_read_sd_payload(&file, path, boot_image->payload_size)) {
        f_close(&file);
        return false;
    }

    res = f_close(&file);
    if (FR_OK != res) {
        error("f_close (%d)", res);
    }

    DEBUG_LOG("Boot file bytes read: %u", (unsigned)boot_image->payload_size);
    return true;

}

static void _read_flash_app(void) {

    uint8_t *dest = (uint8_t*)FIRMWARE_DEST_ADDR;
    uint32_t offset = 0;

    gui_boot_effect_begin();

    while (offset < FIRMWARE_SIZE) {
        uint32_t chunk_size = _firmware_read_chunk_size(offset, FIRMWARE_SIZE);

        dev_flash_read(
            FIRMWARE_FLASH_ADDR + offset,
            dest + offset,
            chunk_size
        );

        offset += chunk_size;
        gui_boot_effect_step(offset, FIRMWARE_SIZE);
    }

}

static bool _dump_flash_to_sdcard(void) {

    FIL file;
    FRESULT res;
    char path[FB_NAME_MAX];
    uint32_t offset = 0;

    if (!s_sdcard_mounted && !_mount_sdcard()) {
        _set_error("No SD card");
        return false;
    }

    _build_child_path(path, FB_NAME_MAX, "FLASHDMP.BIN");

    res = f_open(&file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (FR_OK != res) {
        DEBUG_LOG("flash dump open failed: %s res=%d", path, (int)res);
        _set_error("Open failed");
        return false;
    }

    gui_boot_effect_begin();

    while (offset < FLASH_TOTAL_SIZE) {
        UINT bytes_written = 0;
        uint32_t chunk_size = _firmware_read_chunk_size(offset, FLASH_TOTAL_SIZE);

        dev_flash_read(offset, s_flash_dump_buffer, chunk_size);

        res = f_write(&file, s_flash_dump_buffer, (UINT)chunk_size, &bytes_written);
        if ((FR_OK != res) || (bytes_written != chunk_size)) {
            DEBUG_LOG(
                "flash dump write failed: offset=%u res=%d bytes=%u/%u",
                (unsigned)offset,
                (int)res,
                (unsigned)bytes_written,
                (unsigned)chunk_size
            );
            _set_error("Write failed");
            f_close(&file);
            return false;
        }

        offset += chunk_size;
        gui_boot_effect_step(offset, FLASH_TOTAL_SIZE);
    }

    res = f_close(&file);
    if (FR_OK != res) {
        DEBUG_LOG("flash dump close failed: res=%d", (int)res);
        _set_error("Close failed");
        return false;
    }

    DEBUG_LOG("Flash dumped to %s: %u bytes", path, (unsigned)FLASH_TOTAL_SIZE);
    _load_current_dir();
    return true;

}



/**
 * @brief   Callback triggered by panel encoder events.
 *
 * @param[in]   index   Index of encoder.
 * @param[in]   value   Value of encoder.
 */
static void _encoder_callback(uint8_t index, int8_t value) {

    switch (index) {

    case ENCODER_MAIN: {

        if (value == 0x01) {
            _action_next();
        } else {
            _action_prev();
        }

    } break;
    
    default: break;

    }
}

static void _button_callback(uint8_t index, bool state) {

    if (BUTTON_ENTER == index && state) {
        _action_open();
    }

}

static void _action_next(void) {

    _clear_error();

    if (0 == s_total_items) {
        return;
    }

    s_selected_item = (s_selected_item + 1) % s_total_items;
    _refresh_view();

}

static void _action_prev(void) {

    _clear_error();

    if (0 == s_total_items) {
        return;
    }

    s_selected_item = (s_selected_item - 1 + s_total_items) % s_total_items;
    _refresh_view();

}

static void _action_open(void) {

    fb_item_t *item;
    char path[FB_NAME_MAX];
    boot_image_info_t boot_image;

    if (s_view.error_active) {
        _clear_error();
        return;
    }

    if (0 == s_view.count) {
        return;
    }

    item = &s_view.items[s_view.selected];

    switch (item->type) {

    case FB_ITEM_BOOT_FLASH:
        DEBUG_LOG("booting flash...");
        _read_flash_app();
        handoff_factory_firmware();
        break;

    case FB_ITEM_INSTALL_BOOTLOADER: {
        DEBUG_LOG("installing bootloader...");

        /// @TODO: Must refuse to install when battery is too low, or atleast show choice popup.
    
        gui_show_message_box("Installing...", "DO NOT TURN OFF POWER!!!");

        switch (install_bootloader_to_flash()) {
        case FLASH_SUCCESS:
            gui_show_message_box("Success!", "Restart your device.");
            delay_block_ms(3000); // UGLY I KNOW
            break;
        case FLASH_READ_FAILED:
            gui_show_message_box("ERROR!", "Read failed.\nYou can safely reboot your korg.");
            delay_block_ms(3000); // UGLY I KNOW
            break;
        case FLASH_WRITE_REPAIRED:
            gui_show_message_box("ERROR!", "Bootloader not installed.\nYou can safely reboot your korg.");
            delay_block_ms(3000); // UGLY I KNOW
            break;
        case FLASH_WRITE_CORRUPTED:
            gui_show_message_box("FATAL ERROR!", "CORRUPTED FLASH\nDO NOT RESTART");
            delay_block_ms(3000); // UGLY I KNOW
            break;
        }
        
    } break;

    case FB_ITEM_DUMP_FLASH:
        DEBUG_LOG("dumping flash to SD...");
        _dump_flash_to_sdcard();
        break;

    case FB_ITEM_ROOT_DIR:
        _set_current_dir("/");
        break;

    case FB_ITEM_PARENT_DIR:
        _open_parent_dir();
        break;

    case FB_ITEM_DIR:
        _open_child_dir(item->name);
        break;

    case FB_ITEM_FILE:
    default:
        _build_child_path(path, FB_NAME_MAX, item->name);
        DEBUG_LOG("booting %s...", path);

        if (_load_boot_file(path, item, &boot_image)) {
            boot_image_handoff(&boot_image);
        }
        break;

    }

}
