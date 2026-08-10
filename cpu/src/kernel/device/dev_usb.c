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
 * @file  dev_usb.c
 *
 * @brief USB device-layer facade.
 */

/*----- Includes -----------------------------------------------------*/

#include "dev_usb.h"

#include "ft.h"

#if CFG_TUD_MSC

#include "dev_sdcard.h"
#include "per_usb.h"
#include "tusb.h"

#include "class/msc/msc.h"

#define USB_MSC_BLOCK_SIZE 512u

static bool g_usb_ready = false;
static bool g_msc_sdcard_exported = false;
static volatile t_dev_usb_msc_export_status g_msc_export_status =
    DEV_USB_MSC_EXPORT_DISABLED;
static u32 g_msc_sdcard_block_count = 0;
static u32 g_msc_read_block[USB_MSC_BLOCK_SIZE / sizeof(u32)];

static bool _msc_valid_lun(u8 lun);
static s32 _msc_fail_not_ready(u8 lun);
static bool _msc_read10_to_buffer(u32 lba, u32 offset,
                                  void *buffer, u32 bufsize);

bool dev_usb_init_device(u8 int_channel) {

    if (g_usb_ready) {
        return true;
    }

    if (!per_usb_init_device(int_channel)) {
        return false;
    }

    if (!tusb_init()) {
        per_usb_terminate();
        return false;
    }

    g_usb_ready = true;

    return true;
}

void dev_usb_task(void) {

    if (g_usb_ready) {
        tud_task_ext(0, false);
    }
}

bool dev_usb_ready(void) { return g_usb_ready; }

bool dev_usb_mounted(void) {

    return g_usb_ready && tud_mounted();
}

bool dev_usb_cdc_connected(void) {

    return g_usb_ready && tud_cdc_connected();
}

u32 dev_usb_cdc_available(void) {

    if (!g_usb_ready) {
        return 0;
    }

    return tud_cdc_available();
}

u32 dev_usb_cdc_read(void *buffer, u32 length) {

    if ((buffer == NULL) || (length == 0) || !g_usb_ready) {
        return 0;
    }

    return tud_cdc_read(buffer, length);
}

u32 dev_usb_cdc_write(const void *buffer, u32 length) {

    u32 written = 0;

    if ((buffer == NULL) || (length == 0) || !g_usb_ready ||
        !tud_cdc_connected()) {
        return 0;
    }

    written = tud_cdc_write(buffer, length);
    tud_cdc_write_flush();

    return written;
}

u32 dev_usb_vendor_write(const void *buffer, u32 length) {

#if CFG_TUD_VENDOR
    if ((buffer == NULL) || (length == 0) || !g_usb_ready ||
        !tud_vendor_mounted()) {
        return 0;
    }

    return tud_vendor_write(buffer, length);
#else
    (void)buffer;
    (void)length;

    return 0;
#endif
}

bool dev_usb_msc_export_sdcard_readonly(void) {

    if (!dev_sdcard_present()) {
        g_msc_sdcard_exported = false;
        g_msc_sdcard_block_count = 0;
        g_msc_export_status = DEV_USB_MSC_EXPORT_NO_CARD;
        return false;
    }

    if (SDCARD_OK != dev_sdcard_init()) {
        g_msc_sdcard_exported = false;
        g_msc_sdcard_block_count = 0;
        g_msc_export_status = DEV_USB_MSC_EXPORT_INIT_FAILED;
        return false;
    }

    g_msc_sdcard_block_count = dev_sdcard_get_sector_count();
    g_msc_sdcard_exported = (g_msc_sdcard_block_count > 0);
    g_msc_export_status = g_msc_sdcard_exported
                              ? DEV_USB_MSC_EXPORT_OK
                              : DEV_USB_MSC_EXPORT_NO_BLOCKS;

    return g_msc_sdcard_exported;
}

void dev_usb_msc_disable(void) {

    g_msc_sdcard_exported = false;
    g_msc_sdcard_block_count = 0;
    g_msc_export_status = DEV_USB_MSC_EXPORT_DISABLED;
}

bool dev_usb_msc_exported(void) { return g_msc_sdcard_exported; }

t_dev_usb_msc_export_status dev_usb_msc_export_status(void) {

    return g_msc_export_status;
}

u8 tud_msc_get_maxlun_cb(void) { return 1; }

void tud_msc_inquiry_cb(u8 lun, u8 vendor_id[8],
                        u8 product_id[16], u8 product_rev[4]) {

    (void)lun;

    memcpy(vendor_id, "FREETRIB", 8);
    memcpy(product_id, "SD READONLY     ", 16);
    memcpy(product_rev, "0001", 4);
}

bool tud_msc_test_unit_ready_cb(u8 lun) {

    if (!_msc_valid_lun(lun) || !g_msc_sdcard_exported ||
        !dev_sdcard_present()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return false;
    }

    return true;
}

void tud_msc_capacity_cb(u8 lun, u32 *block_count,
                         u16 *block_size) {

    if ((block_count == NULL) || (block_size == NULL)) {
        return;
    }

    *block_count = 0;
    *block_size = 0;

    if (!_msc_valid_lun(lun)) {
        return;
    }

    *block_count = g_msc_sdcard_block_count;
    *block_size = USB_MSC_BLOCK_SIZE;
}

bool tud_msc_is_writable_cb(u8 lun) {

    (void)lun;

    return false;
}

s32 tud_msc_read10_cb(u8 lun, u32 lba, u32 offset,
                          void *buffer, u32 bufsize) {

    if (!_msc_valid_lun(lun) || !g_msc_sdcard_exported ||
        (buffer == NULL) || (bufsize == 0) ||
        (offset >= USB_MSC_BLOCK_SIZE)) {
        return _msc_fail_not_ready(lun);
    }

    u32 read_span = offset + bufsize;

    if (read_span < offset) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    u32 block_count =
        (read_span + USB_MSC_BLOCK_SIZE - 1u) / USB_MSC_BLOCK_SIZE;

    if ((block_count == 0) || (block_count > g_msc_sdcard_block_count) ||
        (lba > (g_msc_sdcard_block_count - block_count))) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    if (!_msc_read10_to_buffer(lba, offset, buffer, bufsize)) {
        return _msc_fail_not_ready(lun);
    }

    return (s32)bufsize;
}

s32 tud_msc_write10_cb(u8 lun, u32 lba, u32 offset,
                           u8 *buffer, u32 bufsize) {

    (void)lba;
    (void)offset;
    (void)buffer;
    (void)bufsize;

    tud_msc_set_sense(lun, SCSI_SENSE_DATA_PROTECT, 0x27, 0x00);
    return TUD_MSC_RET_ERROR;
}

s32 tud_msc_scsi_cb(u8 lun, u8 const scsi_cmd[16],
                        void *buffer, u16 bufsize) {

    (void)buffer;
    (void)bufsize;

    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    return TUD_MSC_RET_ERROR;
}

static bool _msc_valid_lun(u8 lun) { return lun == 0; }

static s32 _msc_fail_not_ready(u8 lun) {

    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);

    return TUD_MSC_RET_ERROR;
}

static bool _msc_read10_to_buffer(u32 lba, u32 offset,
                                  void *buffer, u32 bufsize) {

    if ((offset == 0) && ((bufsize % USB_MSC_BLOCK_SIZE) == 0) &&
        ((((uintptr_t)buffer) & (sizeof(u32) - 1u)) == 0u)) {
        return SDCARD_OK == dev_sdcard_read(
                                lba, bufsize / USB_MSC_BLOCK_SIZE,
                                (u32 *)buffer);
    }

    u8 *dest = (u8 *)buffer;
    u32 remaining = bufsize;
    u32 current_lba = lba;
    u32 current_offset = offset;

    while (remaining > 0) {
        if (SDCARD_OK != dev_sdcard_read(current_lba, 1, g_msc_read_block)) {
            return false;
        }

        u32 bytes_this_block =
            USB_MSC_BLOCK_SIZE - current_offset;

        if (bytes_this_block > remaining) {
            bytes_this_block = remaining;
        }

        memcpy(dest,
               ((const u8 *)g_msc_read_block) + current_offset,
               bytes_this_block);

        dest += bytes_this_block;
        remaining -= bytes_this_block;
        current_lba++;
        current_offset = 0;
    }

    return true;
}

#else

bool dev_usb_init_device(u8 int_channel) {

    (void)int_channel;

    return false;
}

void dev_usb_task(void) {}

bool dev_usb_ready(void) { return false; }

bool dev_usb_mounted(void) { return false; }

bool dev_usb_cdc_connected(void) { return false; }

u32 dev_usb_cdc_available(void) { return 0; }

u32 dev_usb_cdc_read(void *buffer, u32 length) {

    (void)buffer;
    (void)length;

    return 0;
}

u32 dev_usb_cdc_write(const void *buffer, u32 length) {

    (void)buffer;
    (void)length;

    return 0;
}

u32 dev_usb_vendor_write(const void *buffer, u32 length) {

    (void)buffer;
    (void)length;

    return 0;
}

bool dev_usb_msc_export_sdcard_readonly(void) { return false; }

void dev_usb_msc_disable(void) {}

bool dev_usb_msc_exported(void) { return false; }

t_dev_usb_msc_export_status dev_usb_msc_export_status(void) {

    return DEV_USB_MSC_EXPORT_DISABLED;
}

#endif // CFG_TUD_MSC

/*----- End of file --------------------------------------------------*/
