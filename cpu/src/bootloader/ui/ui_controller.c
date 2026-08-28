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
 * @file    boot_controller.c
 * 
 * @brief   Bootloader presenter, input controller, and UI state machine.
 * 
 * @author  vanasoft 23
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "per_gpio.h"
#include "svc_panel.h"

#include "service/boot_image.h"
#include "service/boot_section.h"
#include "ui/modal.h"
#include "ui/ui_controller.h"
#include "ui/file_browser.h"
#include "ui/render.h"
#include "flash_io.h"
#include "service/firmware.h"
#include "handoff.h"

/*----- Macros -------------------------------------------------------*/

#define BUTTON_ENTER 0x09
#define ENCODER_MAIN 0x00

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	view_t              state;
	modal_t             modal;
	u32                 progress_done;
	u32                 progress_total;
} boot_presenter_t;

/*----- Static variable definitions ----------------------------------*/

static boot_presenter_t s_presenter = {
	.state = VIEW_BROWSER
};

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
static void _activate_browser_item(void);
static void _activate_modal(void);
static void _dispatch_fb_command(const fb_command_t *command);
static void _check_power_off_btn(void);

/*----- Extern function implementations ------------------------------*/

void ui_controller_init(void) {
	file_browser_init();
	gui_init();

	svc_panel_register_callback(ENCODER_EVENT, _encoder_callback);
	svc_panel_register_callback(BUTTON_EVENT, _button_callback);

	_show_browser();
	_render();
}

void ui_controller_tick(void) {
	_check_power_off_btn();
	svc_panel_process();
	file_browser_tick();
	_render();
}

void ui_controller_run_install_sbl(void) {
	_show_modal("Installing...", "DO NOT TURN OFF\nPOWER!!!", MODAL_UNSKIPPABLE);
	_render();

	bootsect_res_t res = install_sbl_to_flash();
	switch (res) {

	case BOOTSECT_INSTALL_SUCCESS:
		 _show_modal("Success!", "Restart your device.", MODAL_DISMISS);
		 break;

	case BOOTSECT_INSTALL_FAIL_REPAIRED:
		 _show_modal("ERROR!", "Read failed.\nYou can safely reboot.", MODAL_DISMISS);
		 break;

	case BOOTSECT_INSTALL_FAIL_CORRUPTED:
		_show_modal("ERROR!", "Not installed.\nYou can safely reboot.", MODAL_DISMISS);
		break;

	default:
		_show_modal("Unknown error!", "Please try again.", MODAL_DISMISS);
		break;

	}

}

/*----- Static function implementations ------------------------------*/

static void _render(void) {
	
	gui_view_t view = {
		.mode           = s_presenter.state,
		.browser        = file_browser_get_view(),
		.modal          = s_presenter.modal,
		.progress_done  = s_presenter.progress_done,
		.progress_total = s_presenter.progress_total
	};

	gui_render(&view);

}

static void _show_browser(void) {

	s_presenter.state = VIEW_BROWSER;
	s_presenter.modal = (modal_t) {
		.type = MODAL_DISMISS
	};
	s_presenter.progress_done = 0;
	s_presenter.progress_total = 0;

}


static void _show_modal(
	const char         *title,
	const char         *message,
	modal_type_t        type
) {

	s_presenter.state = VIEW_MODAL;
	s_presenter.modal = (modal_t) {
		.type    = type,
		.title   = title,
		.message = message
	};

}


static void _activate_modal(void) {

	if ((MODAL_CONFIRM_INSTALL == s_presenter.modal.type)
	 && !s_presenter.modal.cancel_selected) {
		ui_controller_run_install_sbl();
		return;
	}

	_show_browser();

}


static void _progress_callback(u32 done, u32 total) {

	s_presenter.state          = VIEW_PROGRESS;
	s_presenter.progress_done  = done;
	s_presenter.progress_total = total;
	_render();

}

static void _encoder_callback(u8 index, s8 value) {

	if (ENCODER_MAIN != index) {
		return;
	}

	switch (s_presenter.state) {

	case VIEW_MODAL:
		s_presenter.modal.cancel_selected =
			!s_presenter.modal.cancel_selected;
		break;

	case VIEW_BROWSER:
		if (0x01 == value) {
			file_browser_next();
		} else {
			file_browser_prev();
		}
		break;

	case VIEW_PROGRESS:
	default:
		break;

	}

}

static void _button_callback(u8 index, bool state) {

	if ((BUTTON_ENTER != index) || !state) {
		return;
	}

	switch (s_presenter.state) {

	case VIEW_BROWSER:
		_activate_browser_item();
		break;

	case VIEW_MODAL:
		_activate_modal();
		break;

	case VIEW_PROGRESS:
	default:
		break;

	}

}

static void _activate_browser_item(void) {

	fb_command_t command;

	if (file_browser_activate(&command)) {
		_dispatch_fb_command(&command);
	}

}


static void _dispatch_fb_command(const fb_command_t *command) {

	flashio_status_t     flash_res;
	firmware_result_t    fw_result;
	boot_image_info_t    boot_image;

	switch (command->type) {

	case FB_COMMAND_BOOT_FLASH:

		 fw_load_flash(_progress_callback);
		 handoff_factory_firmware();
		 _show_modal("ERROR :(", "Boot returned", MODAL_DISMISS);
		 break;

	case FB_COMMAND_INSTALL_BOOTLOADER:

		 _show_modal(
			"Install bootloader",
			"Continue?\nKeep power connected.",
			MODAL_CONFIRM_INSTALL
		);
		 break;

	case FB_COMMAND_DUMP_FLASH:

		 flash_res = flashio_dump_sdcard(command->path, _progress_callback);
		 if (FLASHIO_SUCCESS == flash_res) {
			 file_browser_refresh();
			 _show_browser();
		 } else {
			 _show_modal("ERROR :(", "Dump to SDcard error", MODAL_DISMISS);
		 }
		 break;

	case FB_COMMAND_BOOT_FILE:

		 fw_result = fw_load_sdcard(
			 command->path,
			 command->file_size,
			 _progress_callback,
			 &boot_image
		 );
		 if (FW_SUCCESS == fw_result) {
			 boot_image_handoff(&boot_image);
			 _show_modal("ERROR :(", "Boot returned", MODAL_DISMISS);
		 } else {
			 _show_modal("ERROR :(", fw_result_str(fw_result), MODAL_DISMISS);
		 }
		 break;

	case FB_COMMAND_NONE:
	default:
		 break;

	}

}

/*----- Power button handling ----------------------------------------*/

static void _shutdown(void) {

	// @TODO: finish/terminate any outstanding writes/reads

	// // Allows handing off to factory bootloader.
	// // Factory SBL will hang if DDR already initialised.
	// per_ddr_terminate();

	// // Flush TRS MIDI before terminating MCU.
	// per_uart_terminate(1);
	// per_uart_terminate(0);

	// _hardware_terminate();
	
	// @TODO: GRACEFUL SHUTDOWN!!!!!!!!!!!!!!!!

	per_gpio_set_indexed(PIN_SHUTDOWN, false);
	for(;;) {
		extern void dev_lcd_set_backlight(bool,bool,bool);
		dev_lcd_set_backlight(false, true, false);
		// wait for power button to release
	}
}


static void _check_power_off_btn(void)
{
	static int shutdown_timer;
	static bool was_pressed;
	bool pressed = !per_gpio_get_indexed(PIN_POWER_BUTTON);

	if (pressed) {
		if (!was_pressed) {
			shutdown_timer = 300;
			was_pressed    = true;
		} else {
			shutdown_timer--;
			if (shutdown_timer == 0) {
				DLOG("SHUTDOWN!\n");
				_shutdown();
			}
		}
	} else {
		was_pressed    = false;
	}

	// DLOG("pressed: %i was_pressed: %i timer: %i",
	//     (int)pressed, (int)was_pressed, (int)shutdown_timer);

}
