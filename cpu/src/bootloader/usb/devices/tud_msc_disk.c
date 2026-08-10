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
 * @file    tud_msc_disk.c
 *
 * @brief   Exposes the SD card as a writable USB mass-storage disk.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <class/msc/msc.h>
#include <ff.h>
#include <diskio.h>
#include <tusb.h>

#include "dev_sdcard.h"
#include "ui/file_browser.h"
#include "usb/devices/tud_msc_disk.h"

#if CFG_TUD_MSC

/*----- Macros -------------------------------------------------------*/

#define BOOT_MSC_DRIVE                         0u
#define BOOT_MSC_BLOCK_SIZE                    512u
#define BOOT_MSC_SCSI_VERIFY_10                0x2fu
#define BOOT_MSC_SCSI_SYNCHRONIZE_CACHE_10     0x35u
#define BOOT_MSC_SCSI_READ_CAPACITY_16         0x9eu
#define BOOT_MSC_READ_CAPACITY_16_SERVICE      0x10u
#define BOOT_MSC_READ_CAPACITY_16_RESPONSE_LEN 32u

/*----- Static variable definitions ----------------------------------*/

static bool s_ejected;
static bool s_write_active;

/*----- Static function prototypes -----------------------------------*/

static bool _msc_valid_lun(u8 lun);
static bool _msc_ready(u8 lun);
static bool _msc_capacity(u8 lun, u32 *block_count);
static bool _msc_valid_transfer(u8 lun, u32 lba, u32 offset,
                                u32 bufsize, u32 *block_count);
static void _msc_begin_host_write(void);
static void _msc_end_host_write(bool refresh_browser);
static void _write_be32(u8 *dest, u32 value);
static void _write_be64(u8 *dest, u64 value);

/*----- Extern function implementations ------------------------------*/

void boot_msc_mount_cb(void) {

    s_ejected = false;
    _msc_end_host_write(false);
    file_browser_set_msc_active(true);

}

void boot_msc_umount_cb(void) {

    s_ejected = false;
    _msc_end_host_write(true);
    file_browser_set_msc_active(false);

}

u8 tud_msc_get_maxlun_cb(void) {

    return 1;

}

void tud_msc_inquiry_cb(u8 lun, u8 vendor_id[8],
                        u8 product_id[16], u8 product_rev[4]) {
    (void)lun;

    memcpy(vendor_id, "FREETRIB", 8);
    memcpy(product_id, "BOOT SD CARD    ", 16);
    memcpy(product_rev, "0001", 4);

}

bool tud_msc_test_unit_ready_cb(u8 lun) {

    return _msc_ready(lun);

}

void tud_msc_capacity_cb(u8 lun, u32 *block_count,
                         u16 *block_size) {

    if ((block_count == NULL) || (block_size == NULL)) {
        return;
    }

    *block_count = 0;
    *block_size = 0;

    if (!_msc_capacity(lun, block_count)) {
        return;
    }

    *block_size = BOOT_MSC_BLOCK_SIZE;

}

bool tud_msc_start_stop_cb(u8 lun, u8 power_condition, bool start,
                           bool load_eject) {
    (void)power_condition;

    if (!_msc_valid_lun(lun)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x25, 0x00);
        return false;
    }

    if (!load_eject) {
        return true;
    }

    s_ejected = !start;
    file_browser_set_msc_active(!s_ejected);

    if (s_ejected) {
        _msc_end_host_write(true);
    }

    return true;

}

bool tud_msc_prevent_allow_medium_removal_cb(u8 lun,
                                             u8 prohibit_removal,
                                             u8 control) {
    (void)prohibit_removal;
    (void)control;

    return _msc_valid_lun(lun);

}

bool tud_msc_is_writable_cb(u8 lun) {

    return _msc_ready(lun);

}

s32 tud_msc_read10_cb(u8 lun, u32 lba, u32 offset,
                      void *buffer, u32 bufsize) {

    u32 block_count;

    if ((buffer == NULL) ||
        !_msc_valid_transfer(lun, lba, offset, bufsize, &block_count)) {
        return TUD_MSC_RET_ERROR;
    }

    if (RES_OK != disk_read(BOOT_MSC_DRIVE, buffer, lba, block_count)) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x11, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    return (s32)bufsize;

}

s32 tud_msc_write10_cb(u8 lun, u32 lba, u32 offset,
                       u8 *buffer, u32 bufsize) {

    u32 block_count;

    if ((buffer == NULL) ||
        !_msc_valid_transfer(lun, lba, offset, bufsize, &block_count)) {
        return TUD_MSC_RET_ERROR;
    }

    _msc_begin_host_write();

    if (RES_OK != disk_write(BOOT_MSC_DRIVE, buffer, lba, block_count)) {
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    return (s32)bufsize;

}

void tud_msc_write10_complete_cb(u8 lun) {
    (void)lun;

    _msc_end_host_write(false);

}

s32 tud_msc_scsi_cb(u8 lun, u8 const scsi_cmd[16],
                    void *buffer, u16 bufsize) {

    if (!_msc_valid_lun(lun)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x25, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    switch (scsi_cmd[0]) {

    case SCSI_CMD_MODE_SELECT_6:
    case BOOT_MSC_SCSI_VERIFY_10:
        return 0;

    case BOOT_MSC_SCSI_SYNCHRONIZE_CACHE_10:
        if (!_msc_ready(lun)) {
            return TUD_MSC_RET_ERROR;
        }
        if (RES_OK != disk_ioctl(BOOT_MSC_DRIVE, CTRL_SYNC, NULL)) {
            tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0c, 0x00);
            return TUD_MSC_RET_ERROR;
        }
        return 0;

    case BOOT_MSC_SCSI_READ_CAPACITY_16: {

        u32 block_count;

        if (((scsi_cmd[1] & 0x1fu) !=
             BOOT_MSC_READ_CAPACITY_16_SERVICE) ||
            (buffer == NULL) ||
            (bufsize < BOOT_MSC_READ_CAPACITY_16_RESPONSE_LEN) ||
            !_msc_capacity(lun, &block_count)) {
            break;
        }

        u8 *response = buffer;

        memset(response, 0, BOOT_MSC_READ_CAPACITY_16_RESPONSE_LEN);
        _write_be64(response, (u64)block_count - 1u);
        _write_be32(&response[8], BOOT_MSC_BLOCK_SIZE);

        return BOOT_MSC_READ_CAPACITY_16_RESPONSE_LEN;
    }

    default:
        break;

    }

    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    return TUD_MSC_RET_ERROR;

}

/*----- Static function implementations ------------------------------*/

static bool _msc_valid_lun(u8 lun) {

    return lun == 0;

}

static bool _msc_ready(u8 lun) {

    if (!_msc_valid_lun(lun)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x25, 0x00);
        return false;
    }

    if (s_ejected || !dev_sdcard_present()) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        return false;
    }

    DSTATUS status = disk_status(BOOT_MSC_DRIVE);

    if ((status & STA_NOINIT) &&
        (0 != disk_initialize(BOOT_MSC_DRIVE))) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x04, 0x01);
        return false;
    }

    return true;

}

static bool _msc_capacity(u8 lun, u32 *block_count) {

    LBA_t sector_count;

    if ((block_count == NULL) || !_msc_ready(lun) ||
        (RES_OK != disk_ioctl(BOOT_MSC_DRIVE, GET_SECTOR_COUNT,
                              &sector_count)) ||
        (sector_count == 0)) {
        if (_msc_valid_lun(lun)) {
            tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3a, 0x00);
        }
        return false;
    }

    *block_count = (u32)sector_count;

    return true;

}

static bool _msc_valid_transfer(u8 lun, u32 lba, u32 offset,
                                u32 bufsize, u32 *block_count) {

    u32 capacity;

    if ((block_count == NULL) || !_msc_ready(lun)) {
        return false;
    }

    if ((offset != 0) || (bufsize == 0) ||
        ((bufsize % BOOT_MSC_BLOCK_SIZE) != 0)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x24, 0x00);
        return false;
    }

    *block_count = bufsize / BOOT_MSC_BLOCK_SIZE;

    if (!_msc_capacity(lun, &capacity)) {
        return false;
    }

    if ((lba >= capacity) || (*block_count > (capacity - lba))) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return false;
    }

    return true;

}

static void _msc_begin_host_write(void) {

    if (s_write_active) {
        return;
    }

    s_write_active = true;
    file_browser_suspend_sdcard();

}

static void _msc_end_host_write(bool refresh_browser) {

    if (!s_write_active) {
        return;
    }

    s_write_active = false;
    file_browser_resume_sdcard();

    if (refresh_browser) {
        file_browser_refresh();
    }

}

static void _write_be32(u8 *dest, u32 value) {

    // @TODO: maybe swap endianness first?
    dest[0] = (u8)(value >> 24);
    dest[1] = (u8)(value >> 16);
    dest[2] = (u8)(value >> 8);
    dest[3] = (u8)value;

}

static void _write_be64(u8 *dest, u64 value) {

    for (u32 i = 0; i < 8; i++) {
        dest[7u - i] = (u8)(value >> (i * 8u));
    }

}

#else

void boot_msc_mount_cb(void) {}
void boot_msc_umount_cb(void) {}

#endif

/*----- End of file --------------------------------------------------*/
