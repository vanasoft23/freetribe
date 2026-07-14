/*----------------------------------------------------------------------

                     This file is part of Freetribe

----------------------------------------------------------------------*/

#include "tusb.h"

enum {
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_MSC,
    ITF_NUM_TOTAL,
};

enum {
    EPNUM_CDC_NOTIF = 0x81,
    EPNUM_CDC_OUT = 0x02,
    EPNUM_CDC_IN = 0x82,
    EPNUM_MSC_OUT = 0x03,
    EPNUM_MSC_IN = 0x83,
};

#define CONFIG_TOTAL_LEN                                                     \
    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0xCafe,
    .idProduct = 0x1803,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 0x01,
};

static const uint8_t desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT,
                       EPNUM_CDC_IN, 64),
    TUD_MSC_DESCRIPTOR(ITF_NUM_MSC, 5, EPNUM_MSC_OUT, EPNUM_MSC_IN, 64),
};

static const char *const string_desc_arr[] = {
    (const char[]){0x09, 0x04},
    "Freetribe",
    "Freetribe Kernel USB",
    "000001",
    "Kernel CDC",
    "Kernel MSC",
};

static uint16_t desc_string[32];

uint8_t const *tud_descriptor_device_cb(void) {

    return (uint8_t const *)&desc_device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {

    (void)index;

    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {

    (void)langid;

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
