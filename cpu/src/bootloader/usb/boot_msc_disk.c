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

   You should have received a copy of the GNU Affero General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.

                       Copyright bangcorrupt 2023

----------------------------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "class/msc/msc.h"
#include "macros.h"
#include "tusb.h"

#if CFG_TUD_MSC

#define BOOT_MSC_BLOCK_SIZE 512u
#define BOOT_MSC_BLOCK_COUNT 16u
#define BOOT_MSC_ROOT_ENTRIES 16u
#define BOOT_MSC_ROOT_LBA 2u
#define BOOT_MSC_DATA_LBA 3u
#define BOOT_MSC_FIRST_CLUSTER 2u
#define BOOT_MSC_DATA_CLUSTERS                                                   \
    (BOOT_MSC_BLOCK_COUNT - BOOT_MSC_DATA_LBA)
#define BOOT_MSC_LOG_BYTE_LIMIT 256u
#define BOOT_MSC_README_TEXT                                                     \
    "Freetribe boot MSC RAM disk.\r\n"                                           \
    "Copy a small file here and the bootloader will DEBUG_LOG its bytes.\r\n"
#define BOOT_MSC_SCSI_VERIFY_10 0x2Fu
#define BOOT_MSC_SCSI_SYNCHRONIZE_CACHE_10 0x35u
#define BOOT_MSC_SCSI_READ_CAPACITY_16 0x9Eu
#define BOOT_MSC_SCSI_READ_CAPACITY_16_SERVICE_ACTION 0x10u

static bool s_ejected = false;
static bool s_disk_initialized = false;
static uint32_t s_entry_signature[BOOT_MSC_ROOT_ENTRIES];
static uint8_t s_msc_disk[BOOT_MSC_BLOCK_COUNT][BOOT_MSC_BLOCK_SIZE];

static const uint8_t s_boot_sector_prefix[] = {
    0xEB, 0x3C, 0x90, 'M',  'S',  'D',  'O',  'S',  '5',  '.',  '0',
    0x00, 0x02, 0x01, 0x01, 0x00, 0x01, 0x10, 0x00, 0x10, 0x00, 0xF8,
    0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x29, 0x02, 0x18, 0x23, 0x00,
    'F',  'T',  'R',  'I',  'B',  'E',  ' ',  'M',  'S',  'C',  ' ',
    'F',  'A',  'T',  '1',  '2',  ' ',  ' ',  ' ',
};

TU_VERIFY_STATIC(sizeof(s_boot_sector_prefix) == 62u,
                 "Unexpected FAT12 boot sector prefix size");

static const uint8_t s_volume_label_entry[] = {
    'F', 'T', 'R', 'I', 'B', 'E', ' ', 'M', 'S', 'C', ' ', 0x08,
};

static const uint8_t s_readme_entry[] = {
    'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ', 'T', 'X', 'T', 0x20, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, (uint8_t)((sizeof(BOOT_MSC_README_TEXT) - 1u) & 0xffu),
    (uint8_t)(((sizeof(BOOT_MSC_README_TEXT) - 1u) >> 8) & 0xffu),
    (uint8_t)(((sizeof(BOOT_MSC_README_TEXT) - 1u) >> 16) & 0xffu),
    (uint8_t)(((sizeof(BOOT_MSC_README_TEXT) - 1u) >> 24) & 0xffu),
};

static void _disk_init(void);
static bool _msc_valid_lun(uint8_t lun);
static bool _msc_valid_span(uint32_t lba, uint32_t offset, uint32_t bufsize);
static uint16_t _fat12_next_cluster(uint16_t cluster);
static bool _cluster_to_lba(uint16_t cluster, uint32_t *lba);
static bool _file_chain_covers(uint16_t first_cluster, uint32_t file_size);
static uint32_t _le32(const uint8_t *data);
static uint16_t _le16(const uint8_t *data);
static uint32_t _file_checksum(uint16_t first_cluster, uint32_t file_size);
static bool _entry_is_readme(const uint8_t *entry);
static void _format_83_name(const uint8_t *entry, char name[13]);
static char _hex_digit(uint8_t value);
static void _log_hex_line(uint32_t offset, const uint8_t *data,
                          uint32_t count);
static void _log_file_bytes(const char *name, uint16_t first_cluster,
                            uint32_t file_size, uint32_t checksum);
static void _log_files_if_changed(void);

uint8_t tud_msc_get_maxlun_cb(void) {

    return 1;
}

void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {

    (void)lun;

    memcpy(vendor_id, "FREETRIB", 8);
    memcpy(product_id, "BOOT RAM DISK   ", 16);
    memcpy(product_rev, "0001", 4);
}

uint32_t tud_msc_inquiry2_cb(uint8_t lun, scsi_inquiry_resp_t *inquiry_resp,
                             uint32_t bufsize) {

    (void)bufsize;

    (void)lun;

    memcpy(inquiry_resp->vendor_id, "FREETRIB", 8);
    memcpy(inquiry_resp->product_id, "BOOT RAM DISK   ", 16);
    memcpy(inquiry_resp->product_rev, "0001", 4);

    return sizeof(scsi_inquiry_resp_t);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {

    if (!_msc_valid_lun(lun) || s_ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00);
        return false;
    }

    _disk_init();

    return true;
}

void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {

    (void)lun;

    if ((block_count == NULL) || (block_size == NULL)) {
        return;
    }

    _disk_init();

    *block_count = BOOT_MSC_BLOCK_COUNT;
    *block_size = BOOT_MSC_BLOCK_SIZE;
}

bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {

    (void)lun;
    (void)power_condition;

    if (load_eject) {
        s_ejected = !start;
    }

    return true;
}

bool tud_msc_prevent_allow_medium_removal_cb(uint8_t lun,
                                             uint8_t prohibit_removal,
                                             uint8_t control) {

    (void)lun;
    (void)prohibit_removal;
    (void)control;

    return true;
}

int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {

    if (!_msc_valid_lun(lun) || (buffer == NULL) ||
        !_msc_valid_span(lba, offset, bufsize)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    _disk_init();
    memcpy(buffer, &s_msc_disk[lba][offset], bufsize);

    return (int32_t)bufsize;
}

bool tud_msc_is_writable_cb(uint8_t lun) {

    (void)lun;

    return true;
}

int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {

    if (!_msc_valid_lun(lun) || (buffer == NULL) ||
        !_msc_valid_span(lba, offset, bufsize)) {
        tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x21, 0x00);
        return TUD_MSC_RET_ERROR;
    }

    _disk_init();
    memcpy(&s_msc_disk[lba][offset], buffer, bufsize);

    return (int32_t)bufsize;
}

int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {

    switch (scsi_cmd[0]) {
    case SCSI_CMD_MODE_SELECT_6:
    case BOOT_MSC_SCSI_VERIFY_10:
    case BOOT_MSC_SCSI_SYNCHRONIZE_CACHE_10:
        return 0;

    case BOOT_MSC_SCSI_READ_CAPACITY_16:
        if (((scsi_cmd[1] & 0x1Fu) ==
             BOOT_MSC_SCSI_READ_CAPACITY_16_SERVICE_ACTION) &&
            (buffer != NULL) && (bufsize >= 32u)) {
            uint8_t *rsp = (uint8_t *)buffer;

            memset(rsp, 0, 32u);
            rsp[7] = (uint8_t)(BOOT_MSC_BLOCK_COUNT - 1u);
            rsp[10] = (uint8_t)(BOOT_MSC_BLOCK_SIZE >> 8);
            rsp[11] = (uint8_t)(BOOT_MSC_BLOCK_SIZE & 0xffu);

            return 32;
        }
        break;

    default:
        break;
    }

    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);

    return TUD_MSC_RET_ERROR;
}

int32_t tud_msc_request_sense_cb(uint8_t lun, void *buffer, uint16_t bufsize) {

    (void)buffer;
    (void)bufsize;

    (void)lun;

    return sizeof(scsi_sense_fixed_resp_t);
}

void tud_msc_read10_complete_cb(uint8_t lun) {

    (void)lun;
}

void tud_msc_write10_complete_cb(uint8_t lun) {

    (void)lun;

    _log_files_if_changed();
}

void tud_msc_scsi_complete_cb(uint8_t lun, uint8_t const scsi_cmd[16]) {

    (void)lun;
    (void)scsi_cmd;
}

static bool _msc_valid_lun(uint8_t lun) {

    return lun == 0;
}

static void _disk_init(void) {

    if (s_disk_initialized) {
        return;
    }

    memset(s_msc_disk, 0, sizeof(s_msc_disk));
    memcpy(s_msc_disk[0], s_boot_sector_prefix, sizeof(s_boot_sector_prefix));
    s_msc_disk[0][510] = 0x55;
    s_msc_disk[0][511] = 0xAA;
    s_msc_disk[1][0] = 0xF8;
    s_msc_disk[1][1] = 0xFF;
    s_msc_disk[1][2] = 0xFF;
    s_msc_disk[1][3] = 0xFF;
    s_msc_disk[1][4] = 0x0F;
    memcpy(s_msc_disk[2], s_volume_label_entry, sizeof(s_volume_label_entry));
    memcpy(&s_msc_disk[2][32], s_readme_entry, sizeof(s_readme_entry));
    memcpy(s_msc_disk[3], BOOT_MSC_README_TEXT,
           sizeof(BOOT_MSC_README_TEXT) - 1u);

    s_disk_initialized = true;
    s_entry_signature[1] =
        _file_checksum(BOOT_MSC_FIRST_CLUSTER,
                       sizeof(BOOT_MSC_README_TEXT) - 1u) ^
        ((sizeof(BOOT_MSC_README_TEXT) - 1u) << 1) ^ BOOT_MSC_FIRST_CLUSTER;
}

static bool _msc_valid_span(uint32_t lba, uint32_t offset, uint32_t bufsize) {

    if ((lba >= BOOT_MSC_BLOCK_COUNT) || (offset >= BOOT_MSC_BLOCK_SIZE)) {
        return false;
    }

    uint32_t start = (lba * BOOT_MSC_BLOCK_SIZE) + offset;
    uint32_t end = start + bufsize;
    uint32_t disk_size = BOOT_MSC_BLOCK_COUNT * BOOT_MSC_BLOCK_SIZE;

    return (end >= start) && (end <= disk_size);
}

static uint16_t _fat12_next_cluster(uint16_t cluster) {

    uint32_t fat_offset = cluster + (cluster / 2u);

    if ((fat_offset + 1u) >= BOOT_MSC_BLOCK_SIZE) {
        return 0xFFFu;
    }

    uint16_t value = (uint16_t)s_msc_disk[1][fat_offset] |
                     ((uint16_t)s_msc_disk[1][fat_offset + 1u] << 8);

    if ((cluster & 1u) == 0u) {
        return value & 0x0FFFu;
    }

    return value >> 4;
}

static bool _cluster_to_lba(uint16_t cluster, uint32_t *lba) {

    if ((cluster < BOOT_MSC_FIRST_CLUSTER) ||
        (cluster >= (BOOT_MSC_FIRST_CLUSTER + BOOT_MSC_DATA_CLUSTERS))) {
        return false;
    }

    *lba = BOOT_MSC_DATA_LBA + (cluster - BOOT_MSC_FIRST_CLUSTER);

    return true;
}

static bool _file_chain_covers(uint16_t first_cluster, uint32_t file_size) {

    uint16_t cluster = first_cluster;
    uint32_t remaining = file_size;
    uint32_t guard = 0;

    while ((remaining > 0u) && (guard < BOOT_MSC_DATA_CLUSTERS)) {
        uint32_t lba = 0;

        if (!_cluster_to_lba(cluster, &lba)) {
            return false;
        }

        if (remaining <= BOOT_MSC_BLOCK_SIZE) {
            return true;
        }

        remaining -= BOOT_MSC_BLOCK_SIZE;
        cluster = _fat12_next_cluster(cluster);

        if (cluster >= 0xFF8u) {
            return remaining == 0u;
        }

        guard++;
    }

    return remaining == 0u;
}

static uint32_t _le32(const uint8_t *data) {

    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint16_t _le16(const uint8_t *data) {

    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t _file_checksum(uint16_t first_cluster, uint32_t file_size) {

    uint16_t cluster = first_cluster;
    uint32_t remaining = file_size;
    uint32_t checksum = 2166136261u;
    uint32_t guard = 0;

    while ((remaining > 0) && (guard < BOOT_MSC_DATA_CLUSTERS)) {
        uint32_t lba = 0;

        if (!_cluster_to_lba(cluster, &lba)) {
            break;
        }

        uint32_t count = remaining;

        if (count > BOOT_MSC_BLOCK_SIZE) {
            count = BOOT_MSC_BLOCK_SIZE;
        }

        for (uint32_t i = 0; i < count; i++) {
            checksum ^= s_msc_disk[lba][i];
            checksum *= 16777619u;
        }

        remaining -= count;
        cluster = _fat12_next_cluster(cluster);

        if (cluster >= 0xFF8u) {
            break;
        }

        guard++;
    }

    return checksum ^ file_size ^ first_cluster;
}

static bool _entry_is_readme(const uint8_t *entry) {

    static const uint8_t readme_name[11] = {
        'R', 'E', 'A', 'D', 'M', 'E', ' ', ' ', 'T', 'X', 'T',
    };

    return memcmp(entry, readme_name, sizeof(readme_name)) == 0;
}

static void _format_83_name(const uint8_t *entry, char name[13]) {

    uint32_t pos = 0;
    int32_t base_end = 7;
    int32_t ext_end = 10;

    while ((base_end >= 0) && (entry[base_end] == ' ')) {
        base_end--;
    }

    while ((ext_end >= 8) && (entry[ext_end] == ' ')) {
        ext_end--;
    }

    for (int32_t i = 0; i <= base_end; i++) {
        name[pos++] = (char)entry[i];
    }

    if (ext_end >= 8) {
        name[pos++] = '.';

        for (int32_t i = 8; i <= ext_end; i++) {
            name[pos++] = (char)entry[i];
        }
    }

    name[pos] = '\0';
}

static char _hex_digit(uint8_t value) {

    value &= 0x0Fu;

    if (value < 10u) {
        return (char)('0' + value);
    }

    return (char)('A' + (value - 10u));
}

static void _log_hex_line(uint32_t offset, const uint8_t *data,
                          uint32_t count) {

    char text[16u * 3u];
    uint32_t pos = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (i > 0u) {
            text[pos++] = ' ';
        }

        text[pos++] = _hex_digit(data[i] >> 4);
        text[pos++] = _hex_digit(data[i]);
    }

    text[pos] = '\0';

    DEBUG_LOG("boot MSC +%03u: %s", (unsigned)offset, text);
}

static void _log_file_bytes(const char *name, uint16_t first_cluster,
                            uint32_t file_size, uint32_t checksum) {

    uint16_t cluster = first_cluster;
    uint32_t remaining = file_size;
    uint32_t logged = 0;
    uint32_t guard = 0;

    DEBUG_LOG("boot MSC file %s size=%u checksum=0x%08X", name,
              (unsigned)file_size, (unsigned)checksum);

    while ((remaining > 0) && (logged < BOOT_MSC_LOG_BYTE_LIMIT) &&
           (guard < BOOT_MSC_DATA_CLUSTERS)) {
        uint32_t lba = 0;

        if (!_cluster_to_lba(cluster, &lba)) {
            break;
        }

        uint32_t count = remaining;

        if (count > BOOT_MSC_BLOCK_SIZE) {
            count = BOOT_MSC_BLOCK_SIZE;
        }

        for (uint32_t i = 0;
             (i < count) && (logged < BOOT_MSC_LOG_BYTE_LIMIT); i += 16u) {
            uint32_t line_count = count - i;

            if (line_count > 16u) {
                line_count = 16u;
            }

            if ((logged + line_count) > BOOT_MSC_LOG_BYTE_LIMIT) {
                line_count = BOOT_MSC_LOG_BYTE_LIMIT - logged;
            }

            _log_hex_line(logged, &s_msc_disk[lba][i], line_count);
            logged += line_count;
        }

        remaining -= count;
        cluster = _fat12_next_cluster(cluster);

        if (cluster >= 0xFF8u) {
            break;
        }

        guard++;
    }

    if (file_size > BOOT_MSC_LOG_BYTE_LIMIT) {
        DEBUG_LOG("boot MSC file log truncated at %u bytes",
                  (unsigned)BOOT_MSC_LOG_BYTE_LIMIT);
    }
}

static void _log_files_if_changed(void) {

    for (uint32_t entry_index = 0; entry_index < BOOT_MSC_ROOT_ENTRIES;
         entry_index++) {
        const uint8_t *entry = &s_msc_disk[BOOT_MSC_ROOT_LBA][entry_index * 32u];

        if (entry[0] == 0x00u) {
            break;
        }

        if ((entry[0] == 0xE5u) || ((entry[11] & 0x08u) != 0u) ||
            ((entry[11] & 0x10u) != 0u) || ((entry[11] & 0x0Fu) == 0x0Fu)) {
            s_entry_signature[entry_index] = 0;
            continue;
        }

        uint16_t first_cluster = _le16(&entry[26]);
        uint32_t file_size = _le32(&entry[28]);

        if ((file_size == 0u) ||
            !_file_chain_covers(first_cluster, file_size)) {
            s_entry_signature[entry_index] = 0;
            continue;
        }

        uint32_t checksum = _file_checksum(first_cluster, file_size);
        uint32_t signature = checksum ^ (file_size << 1) ^ first_cluster;

        if (_entry_is_readme(entry)) {
            s_entry_signature[entry_index] = signature;
            continue;
        }

        if (signature == s_entry_signature[entry_index]) {
            continue;
        }

        char name[13];

        s_entry_signature[entry_index] = signature;
        _format_83_name(entry, name);
        _log_file_bytes(name, first_cluster, file_size, checksum);
    }
}

#endif

/*----- End of file --------------------------------------------------*/
