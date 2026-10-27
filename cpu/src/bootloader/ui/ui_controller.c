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
 * @file    ui_controller.c
 * 
 * @brief   Bootloader presenter, input controller, and UI state machine.
 * 
 * @author  vanasoft 23
 */

/*----- Includes -----------------------------------------------------*/

#include "bootloader/ffconf.h"
#include <ff.h>

#include "ft.h"

#include "per_gpio.h"
#include "dev_lcd.h"
#include "svc_panel.h"
#include "shutdown.h"

#include "cmd/install.h"
#include "cmd/program_loader.h"
#include "cmd/handoff.h"
#include "ui/modal.h"
#include "ui/ui_controller.h"
#include "ui/file_browser.h"
#include "ui/render.h"
#include "usb/boot_usb.h"
#include "flash_io.h"

/*----- Macros -------------------------------------------------------*/

#define BUTTON_ENTER 0x09
#define ENCODER_MAIN 0x00

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	view_t  view;
	modal_t modal;
	u32     progress_done;
	u32     progress_total;
} ui_state_t;

typedef struct {
	FIL            file;
	struct program descriptor;
} program_source_t;

/*----- Static variable definitions ----------------------------------*/

static ui_state_t       s_ui;
static program_source_t s_program_source;

/*----- Static function prototypes -----------------------------------*/

static void _render(void);
static void _show_browser(void);
static void _show_modal(
	const char     *title,
	const char     *message,
	modal_type_t    type
);
static void _progress_callback(u32 done, u32 total);
static void _encoder_callback(u8 index, s8 value);
static void _button_callback(u8 index, bool state);
static void _activate_modal(void);
static void _dispatch_fb_command(const fb_command_t *command);
static void _check_power_off_btn(void);
static void _run_flash_dump(const char *path);
static void _run_install_sbl(void);
static void _run_program_file(const fb_command_t *command);
static u32 _read_program_bytes(void *dst, u32 src, u32 nbytes);
static const char *_program_error_message(progldr_status_t status);

/*----- Extern function implementations ------------------------------*/

void ui_controller_init(void)
{
	file_browser_init();
	gui_init();

	svc_panel_register_callback(ENCODER_EVENT, _encoder_callback);
	svc_panel_register_callback(BUTTON_EVENT, _button_callback);

	_show_browser();
	_render();
}

void ui_controller_tick(void)
{
	_check_power_off_btn();
	svc_panel_process();
	file_browser_tick();
	_render();
}

/*----- Static function implementations ------------------------------*/

static void _render(void)
{
	gui_view_t view = {
		.mode           = s_ui.view,
		.browser        = file_browser_get_view(),
		.modal          = s_ui.modal,
		.progress_done  = s_ui.progress_done,
		.progress_total = s_ui.progress_total
	};

	gui_render(&view);
}

static void _show_browser(void)
{
	s_ui.view = VIEW_BROWSER;
	s_ui.modal = (modal_t) {
		.type = MODAL_DISMISS
	};
	s_ui.progress_done = 0;
	s_ui.progress_total = 0;
}


static void _show_modal(
	const char         *title,
	const char         *message,
	modal_type_t        type
) {
	s_ui.view = VIEW_MODAL;
	s_ui.modal = (modal_t) {
		.type    = type,
		.title   = title,
		.message = message
	};
}

static void _activate_modal(void)
{
	switch (s_ui.modal.type) {
	case MODAL_CONFIRM_INSTALL:
		if (s_ui.modal.cancel_selected) {
			_show_browser();
		} else {
			_run_install_sbl();
		}
		break;

	case MODAL_DISMISS:
		_show_browser();
		break;

	case MODAL_UNSKIPPABLE:
	default:
		break;
	}
}

static void _progress_callback(u32 done, u32 total)
{
	s_ui.view = VIEW_PROGRESS;
	s_ui.progress_done = done;
	s_ui.progress_total = total;
	_render();
}

static void _encoder_callback(u8 index, s8 value)
{
	if (ENCODER_MAIN != index) {
		return;
	}

	switch (s_ui.view) {

	case VIEW_MODAL:
		if ((MODAL_CONFIRM_INSTALL == s_ui.modal.type) && (0 != value)) {
			s_ui.modal.cancel_selected = !s_ui.modal.cancel_selected;
		}
		break;

	case VIEW_BROWSER:
		if (value > 0) {
			file_browser_next();
		} else if (value < 0) {
			file_browser_prev();
		}
		break;

	case VIEW_PROGRESS:
	default:
		break;

	}
}

static void _button_callback(u8 index, bool state)
{
	if ((BUTTON_ENTER != index) || !state) {
		return;
	}

	switch (s_ui.view) {

	case VIEW_BROWSER: {
		fb_command_t command;
		if (file_browser_activate(&command)) {
			_dispatch_fb_command(&command);
		}
	} break;

	case VIEW_MODAL:
		_activate_modal();
		break;

	case VIEW_PROGRESS:
	default:
		break;

	}
}



static void _dispatch_fb_command(const fb_command_t *command)
{
	switch (command->type) {
	case FB_COMMAND_BOOT_FLASH: {
		flashio_status_t status = flashio_read_safe(
			0x20000, (u8*)0xC0000000, 0x200000, 3, _progress_callback
		);
		if (FLASHIO_SUCCESS != status) {
			_show_modal("Boot failed", "Flash read failed", MODAL_DISMISS);
			break;
		}
		handoff_program(0xC0000000);
	} break;

	case FB_COMMAND_INSTALL_BOOTLOADER:
		_show_modal(
			"Install bootloader",
			"Continue?\nKEEP POWER ON.",
			MODAL_CONFIRM_INSTALL
		);
		break;

	case FB_COMMAND_DUMP_FLASH:
		_run_flash_dump(command->path);
		break;

	case FB_COMMAND_BOOT_FILE:
		_run_program_file(command);
		break;

	case FB_COMMAND_NONE:
	default:
		break;
	}
}

static void _run_flash_dump(const char *path)
{
	flashio_status_t status = flashio_dump_sdcard(path, _progress_callback);

	if (FLASHIO_SUCCESS == status) {
		file_browser_refresh();
		_show_browser();
		return;
	}

	if (FLASHIO_READ_FAILED == status) {
		_show_modal("Dump failed", "Hardware failure! Flash corrupted", MODAL_DISMISS);
	} else {
		_show_modal("Dump failed", "Could not write SD file", MODAL_DISMISS);
	}
}

static void _run_program_file(const fb_command_t *command)
{
	FRESULT file_result = f_open(&s_program_source.file, command->path, FA_READ);
	if (FR_OK != file_result) {
		_show_modal("Boot failed", "Could not open file", MODAL_DISMISS);
		return;
	}

	struct progldr loader = {
		.read_bytes_cb = _read_program_bytes,
		.source_size   = (u32)f_size(&s_program_source.file)
	};

	progldr_status_t status = program_identify(
		&loader,
		&s_program_source.descriptor
	);
	if (PROGRAM_VALID_ELF != status && PROGRAM_VALID_VSB != status) {
		f_close(&s_program_source.file);
		_show_modal("Boot failed", _program_error_message(status), MODAL_DISMISS);
		return;
	}

	boot_dfu_discard_staging();

	_show_modal("Loading...", "Loading program", MODAL_UNSKIPPABLE);
	_render();

	status = program_load(&loader, &s_program_source.descriptor);
	file_result = f_close(&s_program_source.file);

	if (PROGRAM_OK != status) {
		_show_modal("Boot failed", _program_error_message(status), MODAL_DISMISS);
		return;
	}

	if (FR_OK != file_result) {
		_show_modal("Boot failed", "Could not close file", MODAL_DISMISS);
		return;
	}

	program_boot(&s_program_source.descriptor);
}

static u32 _read_program_bytes(void *dst, u32 src, u32 nbytes)
{
	UINT bytes_read = 0;

	if (FR_OK != f_lseek(&s_program_source.file, src)) {
		return 0;
	}

	if (FR_OK != f_read(
		&s_program_source.file,
		dst,
		(UINT)nbytes,
		&bytes_read
	)) {
		return 0;
	}

	return (u32)bytes_read;
}

static const char *_program_error_message(progldr_status_t status)
{
	switch (status) {
	case PROGRAM_ERR_READ:
		return "Read failed";
	case PROGRAM_ERR_INVALID:
		return "Invalid program";
	default:
		return "Program load failed";
	}
}

static void _run_install_sbl(void)
{
	_show_modal("Installing...", "DO NOT TURN OFF\nPOWER!!!", MODAL_UNSKIPPABLE);
	_render();

	bootsect_res_t res = install_sbl_to_flash();
	switch (res) {
	case BOOTSECT_INSTALL_SUCCESS:
		_show_modal("Installed", "Restart your device", MODAL_DISMISS);
		break;

	case BOOTSECT_INSTALL_FAIL_BACKUP_READ:
		_show_modal(
			"Install aborted",
			"Flash read failed\nNothing was changed",
			MODAL_DISMISS
		);
		break;

	case BOOTSECT_INSTALL_FAIL_REPAIRED:
		_show_modal(
			"Install failed",
			"Original restored\nSafe to retry/reboot",
			MODAL_DISMISS
		);
		break;

	case BOOTSECT_INSTALL_FAIL_CORRUPTED:
		_show_modal(
			"FLASH CORRUPTED",
			"Rollback failed!\nDO NOT REBOOT",
			MODAL_DISMISS
		);
		break;

	case BOOTSECT_INSTALL_FAIL_IMAGE:
		_show_modal(
			"Install aborted",
			"Invalid boot image\nNothing was changed",
			MODAL_DISMISS
		);
		break;

	case BOOTSECT_UNKNOWN_ERROR:
	default:
		_show_modal(
			"Install aborted",
			"Internal error\nNothing was changed",
			MODAL_DISMISS
		);
		break;
	}
}

/*----- Power button handling ----------------------------------------*/


static void _check_power_off_btn(void)
{
	static int shutdown_timer;
	static bool held;
	bool pressed = !per_gpio_get_indexed(PIN_POWER_BUTTON);

	if (pressed) {
		if (!held) {
			shutdown_timer = 300;
			held = true;
		} else {
			shutdown_timer--;
			if (shutdown_timer == 0) {
				shutdown();
			}
		}
	} else {
		held = false;
	}
}
