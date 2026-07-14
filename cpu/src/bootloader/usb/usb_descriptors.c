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

#include <string.h>

#include "tusb.h"

enum {
#if CFG_TUD_MSC
    ITF_NUM_MSC = 0,
#endif
    ITF_NUM_DFU,
    ITF_NUM_TOTAL,
};

#if CFG_TUD_MSC
enum {
    EPNUM_MSC_OUT = 0x01,
    EPNUM_MSC_IN = 0x81,
};
#endif

#define DFU_ALT_COUNT 1
#define DFU_FUNC_ATTRS (DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

#if CFG_TUD_MSC
#define CONFIG_TOTAL_LEN                                                     \
    (TUD_CONFIG_DESC_LEN + TUD_MSC_DESC_LEN + TUD_DFU_DESC_LEN(DFU_ALT_COUNT))
#else
#define CONFIG_TOTAL_LEN                                                     \
    (TUD_CONFIG_DESC_LEN + TUD_DFU_DESC_LEN(DFU_ALT_COUNT))
#endif

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xE2FB, // Electribe 2 Freetribe Bootloader
    .idProduct = 0x1802,
    .bcdDevice = 0x0101,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const tusb_desc_device_qualifier_t desc_device_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0x00,
    .bDeviceSubClass = 0x00,
    .bDeviceProtocol = 0x00,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 0x01,
    .bReserved = 0x00,
};

static const uint8_t desc_fs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
#if CFG_TUD_MSC
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 4, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
#endif
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, 5, DFU_FUNC_ATTRS, 0,
                       BOOT_DFU_FS_XFER_BUFSIZE),
};

static const uint8_t desc_hs_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
#if CFG_TUD_MSC
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 4, EPNUM_MSC_OUT, EPNUM_MSC_IN, 512),
#endif
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU, DFU_ALT_COUNT, 5, DFU_FUNC_ATTRS, 0,
                       BOOT_DFU_HS_XFER_BUFSIZE),
};

static uint8_t desc_other_speed_configuration[CONFIG_TOTAL_LEN];

static const char *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Freetribe",
    "Freetribe Bootloader USB",
    "000101",
#if CFG_TUD_MSC
    "Boot MSC",
#endif
    "Boot DDR",
};

static uint16_t desc_string[32];

uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_device_qualifier_cb(void) {
    return (uint8_t const *)&desc_device_qualifier;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;

    return (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_hs_configuration
                                                : desc_fs_configuration;
}

uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index) {
    (void)index;

    const uint8_t *configuration =
        (tud_speed_get() == TUSB_SPEED_HIGH) ? desc_fs_configuration
                                             : desc_hs_configuration;

    memcpy(desc_other_speed_configuration, configuration, CONFIG_TOTAL_LEN);
    desc_other_speed_configuration[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

    return desc_other_speed_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {

    uint8_t chr_count = 0;

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
