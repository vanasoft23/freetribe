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
 * @file    usb_descriptors.c
 *
 * @brief   Defines the TinyUSB device, configuration, CDC, MSC, and
 *          string descriptors exposed by the Freetribe bootloader.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <class/cdc/cdc_device.h>
#include <tusb.h>

#include "boot_usb.h"

/*--------------------------------------------------------------------*/

enum {
	ITF_NUM_CDC = 0,
	ITF_NUM_CDC_DATA,
	ITF_NUM_DFU,
#if CFG_TUD_MSC
	ITF_NUM_MSC,
#endif
	ITF_NUM_TOTAL,
};

enum {
#if CFG_TUD_MSC
	EPNUM_MSC_OUT   = 0x01,
	EPNUM_MSC_IN    = 0x81,
#endif
	EPNUM_CDC_NOTIF = 0x82,
	EPNUM_CDC_OUT   = 0x03,
	EPNUM_CDC_IN    = 0x83
};

#define DFU_FUNC_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

#if CFG_TUD_MSC
#  define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_DFU_DESC_LEN(DFU_ALT_COUNT) + TUD_MSC_DESC_LEN)
#else
#  define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_DFU_DESC_LEN(DFU_ALT_COUNT))
#endif

static const tusb_desc_device_t desc_device = {
	.bLength            = sizeof(tusb_desc_device_t),
	.bDescriptorType    = TUSB_DESC_DEVICE,
	.bcdUSB             = 0x0200,
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,
	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
	.idVendor           = 0xE2FB, // Electribe 2 Freetribe Bootloader
	.idProduct          = 0x1802,
	.bcdDevice          = 0x0101,
	.iManufacturer      = 0x01,
	.iProduct           = 0x02,
	.iSerialNumber      = 0x03,
	.bNumConfigurations = 0x01,
};

static const tusb_desc_device_qualifier_t desc_device_qualifier = {
	.bLength            = sizeof(tusb_desc_device_qualifier_t),
	.bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
	.bcdUSB             = 0x0200,
	.bDeviceClass       = TUSB_CLASS_MISC,
	.bDeviceSubClass    = MISC_SUBCLASS_COMMON,
	.bDeviceProtocol    = MISC_PROTOCOL_IAD,
	.bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
	.bNumConfigurations = 0x01,
	.bReserved          = 0x00,
};

static const u8 desc_fs_configuration[] = {

	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),

	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
	TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, 5, DFU_FUNC_ATTRS, 0, BOOT_DFU_FS_XFER_BUFSIZE),
#if CFG_TUD_MSC
	TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 10, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
#endif

};

static const u8 desc_hs_configuration[] = {

	TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),

	TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 16, EPNUM_CDC_OUT, EPNUM_CDC_IN, 512),
	TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, 5, DFU_FUNC_ATTRS, 0, BOOT_DFU_HS_XFER_BUFSIZE),
#if CFG_TUD_MSC
	TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 10, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
#endif

};

static u8 s_desc_other_speed_config[CONFIG_TOTAL_LEN];



static const char *const string_desc_arr[] = {
	(const char[]){0x09, 0x04},   // 0: Supported language English
	"Freetribe",                  // 1: Manufacturer
	"Freetribe Bootloader USB",   // 2: Product
	"000101",                     // 3: Serial ID
	"Boot CDC",                   // 4: CDC
	"DFU Flash Bootloader",       // 5: DFU Partition 1
	"DFU Flash Firmware",         // 6: DFU Partition 2
	"DFU Debug Bootloader",       // 7: DFU Partition 3
	"DFU Debug Firmware",         // 8: DFU Partition 4
	"DFU Reflash",                // 9: DFU Partition 5
#if CFG_TUD_MSC
	"Boot MSC",                   // 10: MSC
#endif
};

static u16 desc_string[32];

u8 const *tud_descriptor_device_cb(void) {
	return (u8 const *)&desc_device;
}

u8 const *tud_descriptor_device_qualifier_cb(void) {
	return (u8 const *)&desc_device_qualifier;
}

u8 const *tud_descriptor_configuration_cb(u8 index) {
	(void)index;

	return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration
												: desc_fs_configuration;
}

u8 const *tud_descriptor_other_speed_configuration_cb(u8 index) {
	(void)index;

	const u8 *configuration =
		(tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration
											 : desc_hs_configuration;

	memcpy(s_desc_other_speed_config, configuration, CONFIG_TOTAL_LEN);
	s_desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

	return s_desc_other_speed_config;
}

u16 const *tud_descriptor_string_cb(u8 index, u16 langid) {

	u8 chr_count = 0;

	if (index == 0) {
		desc_string[1] = 0x0409;
		chr_count = 1;

	} else {
		if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
			return NULL;
		}

		const char *str = string_desc_arr[index];

		while ((chr_count < 31) && (str[chr_count] != '\0')) {
			desc_string[1 + chr_count] = str[chr_count];
			chr_count++;
		}
	}

	desc_string[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);

	return desc_string;
}

/*----- End of file --------------------------------------------------*/
