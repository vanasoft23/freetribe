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
 * @file   file_browser.h
 *
 * @author vanasoft23
 */

#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include "ft.h"

#include "util/pathstr.h"

#define FB_MAX_ITEMS  8

typedef enum {
	FB_ITEM_FILE,
	FB_ITEM_DIR,
	FB_ITEM_ROOT_DIR,
	FB_ITEM_PARENT_DIR,
	FB_ITEM_BOOT_FLASH,
	FB_ITEM_INSTALL_BOOTLOADER,
	FB_ITEM_DUMP_FLASH
} fb_item_type_t;

typedef struct {
	char           name[PATH_NAME_MAX];
	fb_item_type_t type;
	u32            size;
} fb_item_t;

typedef struct {
	fb_item_t items[FB_MAX_ITEMS];
	u16       count;
	u16       selected;
	bool      sdcard_inserted;
} fb_view_t;

typedef enum {
	FB_COMMAND_NONE,
	FB_COMMAND_BOOT_FLASH,
	FB_COMMAND_INSTALL_BOOTLOADER,
	FB_COMMAND_DUMP_FLASH,
	FB_COMMAND_BOOT_FILE
} fb_command_type_t;

typedef struct {
	fb_command_type_t type;
	char              path[PATH_NAME_MAX];
	u32               file_size;
} fb_command_t;

void file_browser_init(void);
void file_browser_tick(void);
void file_browser_next(void);
void file_browser_prev(void);
bool file_browser_activate(fb_command_t *command);
bool file_browser_refresh(void);
void file_browser_set_msc_active(bool active);
void file_browser_suspend_sdcard(void);
void file_browser_resume_sdcard(void);
const fb_view_t *file_browser_get_view(void);

#endif
