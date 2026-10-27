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
#include "string/pathstr.h"

/*----- Macros -------------------------------------------------------*/

#define FB_CACHE_MAX_ITEMS          512
#define FB_SDCARD_MOUNT_RETRY_TICKS 256

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	fb_view_t view;
	fb_item_t items[FB_CACHE_MAX_ITEMS];
	char      current_dir[PATH_NAME_MAX];
	u16       item_count;
	u16       selected_index;
	u16       scroll_index;
	u16       mount_retry_ticks;
	bool      card_present;
	bool      mounted;
	bool      suspended;
	bool      msc_active;
	bool      refresh_pending;
} file_browser_state_t;

/*----- Static variable definitions ----------------------------------*/

static file_browser_state_t s_browser;

/*----- Extern variable definitions ----------------------------------*/

extern FATFS g_fatfs;

/*----- Static function prototypes -----------------------------------*/

static bool _load_current_dir(void);
static fb_item_t *_append_cached_item(
	fb_item_type_t type,
	const char    *name,
	u32            size
);
static void _reset_listing(void);
static void _append_browser_items(bool sdcard_available);
static void _show_flash_only_view(void);
static bool _mount_sdcard(void);
static void _unmount_sdcard(void);
static void _poll_sdcard(void);
static bool _refresh_sdcard_if_pending(void);
static void _refresh_view(void);
static void _sync_scroll_to_selection(void);
static bool _set_current_dir(const char *path);

/*----- Extern function implementations ------------------------------*/

void file_browser_init(void)
{
	memset(&s_browser, 0, sizeof(s_browser));
	strcpy(s_browser.current_dir, "/");
	s_browser.card_present = dev_sdcard_present();

	_show_flash_only_view();

	if (s_browser.card_present) {
		_mount_sdcard();
	}
}

void file_browser_tick(void)
{
	if (s_browser.suspended) {
		s_browser.card_present = dev_sdcard_present();
		s_browser.view.sdcard_inserted = s_browser.card_present;
		return;
	}

	_poll_sdcard();
}

void file_browser_next(void)
{
	_refresh_sdcard_if_pending();

	if (0 == s_browser.item_count) {
		return;
	}

	s_browser.selected_index =
		(s_browser.selected_index + 1u) % s_browser.item_count;
	_refresh_view();
}

void file_browser_prev(void)
{
	_refresh_sdcard_if_pending();

	if (0 == s_browser.item_count) {
		return;
	}

	s_browser.selected_index =
		(s_browser.selected_index + s_browser.item_count - 1u) %
		s_browser.item_count;
	_refresh_view();
}

bool file_browser_activate(fb_command_t *command)
{
	if (NULL == command) {
		return false;
	}

	*command = (fb_command_t) {0};

	/* A refresh resets selection. Do not activate a different item than the
	 * one the user saw before pressing Enter. */
	if (_refresh_sdcard_if_pending() || (0 == s_browser.item_count)) {
		return false;
	}

	const fb_item_t *item = &s_browser.items[s_browser.selected_index];

	if (s_browser.suspended &&
		(FB_ITEM_BOOT_FLASH != item->type) &&
		(FB_ITEM_INSTALL_BOOTLOADER != item->type)) {
		return false;
	}

	switch (item->type) {
	case FB_ITEM_BOOT_FLASH:
		command->type = FB_COMMAND_BOOT_FLASH;
		break;

	case FB_ITEM_INSTALL_BOOTLOADER:
		command->type = FB_COMMAND_INSTALL_BOOTLOADER;
		break;

	case FB_ITEM_DUMP_FLASH:
		if (path_build_child_path(
			command->path,
			PATH_NAME_MAX,
			"FLASHDMP.BIN",
			s_browser.current_dir
		)) {
			command->type = FB_COMMAND_DUMP_FLASH;
		}
		break;

	case FB_ITEM_ROOT_DIR:
		_set_current_dir("/");
		break;

	case FB_ITEM_PARENT_DIR: {
		char parent[PATH_NAME_MAX];
		strcpy(parent, s_browser.current_dir);
		path_goto_parent_dir(parent);
		_set_current_dir(parent);
		break;
	}

	case FB_ITEM_DIR: {
		char child[PATH_NAME_MAX];
		if (path_build_child_path(
			child,
			PATH_NAME_MAX,
			item->name,
			s_browser.current_dir
		)) {
			DLOG("Listing files %s", child);
			_set_current_dir(child);
		}
		break;
	}

	case FB_ITEM_FILE:
		if (path_build_child_path(
			command->path,
			PATH_NAME_MAX,
			item->name,
			s_browser.current_dir
		)) {
			command->type = FB_COMMAND_BOOT_FILE;
		}
		break;

	default:
		break;
	}

	return FB_COMMAND_NONE != command->type;
}

bool file_browser_refresh(void)
{
	if (s_browser.suspended || !s_browser.mounted) {
		return false;
	}

	s_browser.refresh_pending = false;
	return _load_current_dir();
}

void file_browser_set_msc_active(bool active)
{
	s_browser.msc_active = active;
}

void file_browser_suspend_sdcard(void)
{
	if (s_browser.suspended) {
		return;
	}

	s_browser.suspended = true;
	f_mount(NULL, "", 0);
	s_browser.mounted = false;
	s_browser.mount_retry_ticks = 0;
}

void file_browser_resume_sdcard(void)
{
	if (!s_browser.suspended) {
		return;
	}

	s_browser.suspended = false;
	s_browser.card_present = dev_sdcard_present();
	s_browser.mount_retry_ticks = 0;

	if (s_browser.card_present) {
		FRESULT res = f_mount(&g_fatfs, "", 0);

		if (FR_OK != res) {
			DLOG("f_mount registration failed: %i", (int)res);
			_show_flash_only_view();
			return;
		}

		s_browser.view.sdcard_inserted = true;
		s_browser.mounted = true;
		s_browser.refresh_pending = true;
	} else {
		_unmount_sdcard();
	}
}

const fb_view_t *file_browser_get_view(void)
{
	return &s_browser.view;
}

/*----- Static function implementations ------------------------------*/

static bool _load_current_dir(void)
{
	static DIR     dir;
	static FILINFO fno;
	FRESULT res;
	u16 first_file_index;

	s_browser.refresh_pending = false;

	res = f_opendir(&dir, s_browser.current_dir);
	if (FR_OK != res) {
		DLOG("f_opendir failed: %i", (int)res);
		_unmount_sdcard();
		return false;
	}

	_reset_listing();
	_append_browser_items(true);
	first_file_index = s_browser.item_count;

	while (s_browser.item_count < FB_CACHE_MAX_ITEMS) {
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
			fb_item_t *directory = _append_cached_item(
				FB_ITEM_DIR,
				fno.fname,
				(u32)fno.fsize
			);

			if (NULL != directory) {
				if (first_file_index < (s_browser.item_count - 1u)) {
					fb_item_t entry = *directory;
					memmove(
						&s_browser.items[first_file_index + 1u],
						&s_browser.items[first_file_index],
						(s_browser.item_count - first_file_index - 1u) *
							sizeof(fb_item_t)
					);
					s_browser.items[first_file_index] = entry;
				}
				first_file_index++;
			}
		} else {
			_append_cached_item(FB_ITEM_FILE, fno.fname, (u32)fno.fsize);
		}
	}

	res = f_closedir(&dir);
	if (FR_OK != res) {
		DLOG("f_closedir failed: %d", res);
		_unmount_sdcard();
		return false;
	}

	_refresh_view();
	return true;
}

static fb_item_t *_append_cached_item(
	fb_item_type_t type,
	const char    *name,
	u32            size
) {
	if (s_browser.item_count >= FB_CACHE_MAX_ITEMS) {
		return NULL;
	}

	fb_item_t *item = &s_browser.items[s_browser.item_count++];
	item->type = type;
	strncpy(item->name, name, PATH_NAME_MAX - 1);
	item->name[PATH_NAME_MAX - 1] = '\0';
	item->size = size;

	return item;
}

static void _reset_listing(void)
{
	s_browser.item_count = 0;
	s_browser.selected_index = 0;
	s_browser.scroll_index = 0;
	s_browser.view.count = 0;
	s_browser.view.selected = 0;
}

static void _append_browser_items(bool sdcard_available)
{
	_append_cached_item(FB_ITEM_BOOT_FLASH, "Boot from flash", 0);
	_append_cached_item(FB_ITEM_INSTALL_BOOTLOADER, "Install bootloader", 0);

	if (!sdcard_available) {
		return;
	}

	_append_cached_item(FB_ITEM_DUMP_FLASH, "Dump flash to SD", 0);

	if (path_is_root_dir(s_browser.current_dir)) {
		_append_cached_item(FB_ITEM_ROOT_DIR, ".", 0);
	} else {
		_append_cached_item(FB_ITEM_PARENT_DIR, "..", 0);
	}
}

static void _show_flash_only_view(void)
{
	s_browser.mounted = false;
	s_browser.refresh_pending = false;
	s_browser.view.sdcard_inserted = s_browser.card_present;
	strcpy(s_browser.current_dir, "/");

	_reset_listing();
	_append_browser_items(false);
	_refresh_view();
}

static bool _mount_sdcard(void)
{
	if (s_browser.suspended) {
		return false;
	}

	s_browser.card_present = dev_sdcard_present();
	if (!s_browser.card_present) {
		_show_flash_only_view();
		return false;
	}

	FRESULT res = f_mount(&g_fatfs, "", 1);
	if (FR_OK != res) {
		DLOG("f_mount failed: %i", (int)res);
		_show_flash_only_view();
		return false;
	}

	s_browser.view.sdcard_inserted = true;
	s_browser.mounted = true;
	s_browser.mount_retry_ticks = 0;
	return _set_current_dir("/");
}

static void _unmount_sdcard(void)
{
	f_mount(NULL, "", 0);
	s_browser.mounted = false;
	s_browser.mount_retry_ticks = 0;
	_show_flash_only_view();
}

static void _poll_sdcard(void)
{
	bool present = dev_sdcard_present();

	if (present != s_browser.card_present) {
		s_browser.card_present = present;
		s_browser.mount_retry_ticks = 0;

		if (present) {
			_mount_sdcard();
		} else {
			_unmount_sdcard();
		}
		return;
	}

	if (!present || s_browser.mounted) {
		return;
	}

	s_browser.mount_retry_ticks++;
	if (s_browser.mount_retry_ticks >= FB_SDCARD_MOUNT_RETRY_TICKS) {
		s_browser.mount_retry_ticks = 0;
		_mount_sdcard();
	}
}

static bool _refresh_sdcard_if_pending(void)
{
	if (s_browser.refresh_pending && !s_browser.suspended) {
		_load_current_dir();
		return true;
	}

	return false;
}

static void _refresh_view(void)
{
	s_browser.view.count = 0;

	if ((s_browser.item_count > 0) &&
		(s_browser.selected_index >= s_browser.item_count)) {
		s_browser.selected_index = s_browser.item_count - 1u;
	}

	_sync_scroll_to_selection();

	for (u16 i = s_browser.scroll_index;
		(i < s_browser.item_count) && (s_browser.view.count < FB_MAX_ITEMS);
		i++) {
		s_browser.view.items[s_browser.view.count++] = s_browser.items[i];
	}

	s_browser.view.selected =
		s_browser.selected_index - s_browser.scroll_index;
}

static void _sync_scroll_to_selection(void)
{
	if (s_browser.selected_index < s_browser.scroll_index) {
		s_browser.scroll_index = s_browser.selected_index;
	} else if (s_browser.selected_index >=
		(s_browser.scroll_index + FB_MAX_ITEMS)) {
		s_browser.scroll_index =
			s_browser.selected_index - FB_MAX_ITEMS + 1u;
	}
}

static bool _set_current_dir(const char *path)
{
	strncpy(s_browser.current_dir, path, PATH_NAME_MAX - 1);
	s_browser.current_dir[PATH_NAME_MAX - 1] = '\0';
	return _load_current_dir();
}
