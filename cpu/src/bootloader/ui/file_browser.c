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
 * @file    file_browser.c
 * @brief   File-browser model and SD-card-backed directory data.
 */

/*----- Includes -----------------------------------------------------*/

#include "bootloader/ffconf.h"
#include <ff.h>

#include "ft.h"

#include "dev_sdcard.h"

#include "ui/file_browser.h"
#include "util/pathstr.h"

/*----- Macros -------------------------------------------------------*/

#define FB_CACHE_MAX_ITEMS          512
#define FB_SDCARD_MOUNT_RETRY_TICKS 256

/*----- Static variable definitions ----------------------------------*/

static fb_view_t s_view;
static fb_item_t s_item_cache[FB_CACHE_MAX_ITEMS];
static char      s_current_dir[PATH_NAME_MAX] = "/";
static u16       s_selected_item;
static u16       s_total_items;
static u16       s_scroll;
static bool      s_last_sdcard_present;
static bool      s_sdcard_mounted;
static bool      s_sdcard_suspended;
static bool      s_msc_active;
static bool      s_sdcard_refresh_pending;
static u16       s_sdcard_mount_retry_ticks;

/*----- Extern variable definitions ----------------------------------*/

extern FATFS g_fatfs;

/*----- Static function prototypes -----------------------------------*/

static bool _load_current_dir(void);
static fb_item_t *_append_cached_item(
	fb_item_type_t type,
	const char    *name,
	u32            size
);
static void _move_last_item_before_files(void);
static void _append_special_items(void);
static void _show_flash_only_view(void);
static bool _mount_sdcard(void);
static void _unmount_sdcard(void);
static void _poll_sdcard(void);
static void _refresh_sdcard_if_pending(void);
static void _refresh_view(void);
static void _sync_scroll_to_selection(void);
static bool _set_current_dir(const char *path);
static void _open_child_dir(const char *name);
static void _set_command(
	fb_command_t      *command,
	fb_command_type_t  type,
	const char        *path,
	u32                file_size
);

/*----- Extern function implementations ------------------------------*/

void file_browser_init(void) {

	s_last_sdcard_present = dev_sdcard_present();
	s_sdcard_suspended = false;
	s_msc_active = false;
	s_sdcard_refresh_pending = false;
	s_sdcard_mount_retry_ticks = 0;

	_show_flash_only_view();

	if (s_last_sdcard_present) {
		_mount_sdcard();
	}

}

void file_browser_tick(void) {

	if (s_sdcard_suspended) {
		s_last_sdcard_present = dev_sdcard_present();
		s_view.sdcard_inserted = s_last_sdcard_present;
		return;
	}

	_poll_sdcard();

}

void file_browser_next(void) {

	_refresh_sdcard_if_pending();

	if (0 == s_total_items) {
		return;
	}

	s_selected_item = (s_selected_item + 1) % s_total_items;
	_refresh_view();

}

void file_browser_prev(void) {

	_refresh_sdcard_if_pending();

	if (0 == s_total_items) {
		return;
	}

	s_selected_item = (s_selected_item - 1 + s_total_items) % s_total_items;
	_refresh_view();

}

bool file_browser_activate(fb_command_t *command) {

	const fb_item_t *item;

	if (NULL == command) {
		return false;
	}

	_set_command(command, FB_COMMAND_NONE, NULL, 0);
	_refresh_sdcard_if_pending();

	if (0 == s_view.count) {
		return false;
	}

	item = &s_view.items[s_view.selected];

	if (s_sdcard_suspended &&
		(FB_ITEM_BOOT_FLASH != item->type) &&
		(FB_ITEM_INSTALL_BOOTLOADER != item->type)) {
		return false;
	}

	if (s_msc_active && (FB_ITEM_DUMP_FLASH == item->type)) {
		return false;
	}

	switch (item->type) {

	case FB_ITEM_BOOT_FLASH:
		_set_command(command, FB_COMMAND_BOOT_FLASH, NULL, 0);
		break;

	case FB_ITEM_INSTALL_BOOTLOADER:
		_set_command(command, FB_COMMAND_INSTALL_BOOTLOADER, NULL, 0);
		break;

	case FB_ITEM_DUMP_FLASH:
		path_build_child_path(command->path, PATH_NAME_MAX, "FLASHDMP.BIN", s_current_dir);
		command->type = FB_COMMAND_DUMP_FLASH;
		break;

	case FB_ITEM_ROOT_DIR:
		_set_current_dir("/");
		break;

	case FB_ITEM_PARENT_DIR:
		path_goto_parent_dir(s_current_dir);
		_load_current_dir();
		break;

	case FB_ITEM_DIR:
		_open_child_dir(item->name);
		break;

	case FB_ITEM_FILE:
	default:
		path_build_child_path(command->path, PATH_NAME_MAX, item->name, s_current_dir);
		command->type = FB_COMMAND_BOOT_FILE;
		command->file_size = item->size;
		break;

	}

	return FB_COMMAND_NONE != command->type;

}

bool file_browser_refresh(void) {

	if (s_sdcard_suspended || !s_sdcard_mounted) {
		return false;
	}

	s_sdcard_refresh_pending = false;
	return _load_current_dir();

}

void file_browser_set_msc_active(bool active) {

	s_msc_active = active;

}

void file_browser_suspend_sdcard(void) {

	if (s_sdcard_suspended) {
		return;
	}

	s_sdcard_suspended = true;
	f_mount(NULL, "", 0);
	s_sdcard_mounted = false;
	s_sdcard_mount_retry_ticks = 0;

}

void file_browser_resume_sdcard(void) {

	if (!s_sdcard_suspended) {
		return;
	}

	s_sdcard_suspended = false;
	s_last_sdcard_present = dev_sdcard_present();
	s_sdcard_mount_retry_ticks = 0;

	if (s_last_sdcard_present) {
		FRESULT res = f_mount(&g_fatfs, "", 0);

		if (FR_OK != res) {
			DLOG("f_mount registration failed: %i", (int)res);
			_show_flash_only_view();
			return;
		}

		s_view.sdcard_inserted = true;
		s_sdcard_mounted = true;
		s_sdcard_refresh_pending = true;
	} else {
		_unmount_sdcard();
	}

}

const fb_view_t *file_browser_get_view(void) {
	return &s_view;
}

/*----- Static function implementations ------------------------------*/

static bool _load_current_dir(void) {

	static DIR     dir;
	static FILINFO fno;
	FRESULT res;

	s_sdcard_refresh_pending = false;

	res = f_opendir(&dir, s_current_dir);
	if (FR_OK != res) {
		DLOG("f_opendir failed: %i", (int)res);
		_unmount_sdcard();
		return false;
	}

	s_selected_item = 0;
	s_scroll        = 0;
	s_view.selected = 0;
	s_total_items   = 0;
	_append_special_items();

	while (s_total_items < FB_CACHE_MAX_ITEMS) {
		res = f_readdir(&dir, &fno);
		if (FR_OK != res) {
			DLOG("f_readdir failed: %d", res);
			f_closedir(&dir);
			_unmount_sdcard();
			return false;
		}

		if (0 == fno.fname[0]) {
			break;
		}
		if (path_is_dot_entry(fno.fname)) {
			continue;
		}
		if (fno.fattrib & AM_HID) {
			continue;
		}

		if (fno.fattrib & AM_DIR) {
			_append_cached_item(FB_ITEM_DIR, fno.fname, (u32)fno.fsize);
			_move_last_item_before_files();
		} else {
			_append_cached_item(FB_ITEM_FILE, fno.fname, (u32)fno.fsize);
		}
	}

	f_closedir(&dir);
	_refresh_view();
	return true;

}

static fb_item_t *_append_cached_item(
	fb_item_type_t type,
	const char    *name,
	u32            size
) {

	fb_item_t *item;

	if (s_total_items >= FB_CACHE_MAX_ITEMS) {
		return NULL;
	}

	item = &s_item_cache[s_total_items];
	item->type = type;
	strncpy(item->name, name, PATH_NAME_MAX - 1);
	item->name[PATH_NAME_MAX - 1] = '\0';
	item->size = size;
	s_total_items++;

	return item;

}

static void _move_last_item_before_files(void) {

	fb_item_t item;
	u16 insert_index;

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
	for (u16 i = s_total_items - 1; i > insert_index; i--) {
		s_item_cache[i] = s_item_cache[i - 1];
	}
	s_item_cache[insert_index] = item;

}

static void _append_special_items(void) {

	_append_cached_item(FB_ITEM_BOOT_FLASH, "Boot from flash", 0);
	_append_cached_item(FB_ITEM_INSTALL_BOOTLOADER, "Install bootloader", 0);
	_append_cached_item(FB_ITEM_DUMP_FLASH, "Dump flash to SD", 0);

	if (path_is_root_dir(s_current_dir)) {
		_append_cached_item(FB_ITEM_ROOT_DIR, ".", 0);
	} else {
		_append_cached_item(FB_ITEM_PARENT_DIR, "..", 0);
	}

}

static void _show_flash_only_view(void) {

	s_sdcard_mounted = false;
	s_sdcard_refresh_pending = false;
	s_view.sdcard_inserted = dev_sdcard_present();
	s_selected_item  = 0;
	s_scroll         = 0;
	s_view.selected  = 0;
	s_total_items    = 0;
	strncpy(s_current_dir, "/", PATH_NAME_MAX - 1);
	s_current_dir[PATH_NAME_MAX - 1] = '\0';

	_append_cached_item(FB_ITEM_BOOT_FLASH, "Boot from flash", 0);
	_append_cached_item(FB_ITEM_INSTALL_BOOTLOADER, "Install bootloader", 0);
	_refresh_view();

}

static bool _mount_sdcard(void) {

	if (s_sdcard_suspended) {
		return false;
	}

	if (!dev_sdcard_present()) {
		_show_flash_only_view();
		return false;
	}

	FRESULT res = f_mount(&g_fatfs, "", 1);
	if (FR_OK != res) {
		DLOG("f_mount failed: %i", (int)res);
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

static void _refresh_sdcard_if_pending(void) {

	if (s_sdcard_refresh_pending && !s_sdcard_suspended) {
		_load_current_dir();
	}

}

static void _refresh_view(void) {

	s_view.count = 0;

	if ((s_total_items > 0) && (s_selected_item >= s_total_items)) {
		s_selected_item = s_total_items - 1;
	}

	_sync_scroll_to_selection();

	for (u16 i = s_scroll; ((i < s_total_items) && (s_view.count < FB_MAX_ITEMS)); i++) {
		s_view.items[s_view.count] = s_item_cache[i];
		s_view.count++;
	}

	s_view.selected = s_selected_item - s_scroll;

}

static void _sync_scroll_to_selection(void) {

	if (s_selected_item < s_scroll) {
		s_scroll = s_selected_item;
	} else if (s_selected_item >= (s_scroll + FB_MAX_ITEMS)) {
		s_scroll = s_selected_item - FB_MAX_ITEMS + 1;
	}

}


static bool _set_current_dir(const char *path) {

	strncpy(s_current_dir, path, PATH_NAME_MAX - 1);
	s_current_dir[PATH_NAME_MAX - 1] = '\0';
	return _load_current_dir();

}

static void _open_child_dir(const char *name) {

	char path[PATH_NAME_MAX];

	path_build_child_path(path, PATH_NAME_MAX, name, s_current_dir);
	DLOG("Listing files %s", path);
	_set_current_dir(path);

}



static void _set_command(
	fb_command_t      *command,
	fb_command_type_t  type,
	const char        *path,
	u32                file_size
) {

	command->type = type;
	command->file_size = file_size;
	command->path[0] = '\0';

	if (NULL != path) {
		strncpy(command->path, path, PATH_NAME_MAX - 1);
		command->path[PATH_NAME_MAX - 1] = '\0';
	}

}
