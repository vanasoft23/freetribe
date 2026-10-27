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
 * @file    tud_dfu.c
 *
 * @brief   TinyUSB DFU download handler for receiving firmware into reserved DDR2 memory.
 * 
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <class/dfu/dfu_device.h>
#include <tusb.h>

#include "flash_io.h"
#include "usb/boot_usb.h"
#include "gdb_monitor.h"

/*----- Macros -------------------------------------------------------*/

#define DFUSE_COMMAND_BLOCK             0u
#define DFUSE_FIRST_DATA_BLOCK          2u
#define DFUSE_COMMAND_SET_ADDRESS       0x21u

#define DFU_FLASH_RETRIES               3
#define DFU_DOWNLOAD_TIMEOUT_MS         1u
#define DFU_MANIFEST_BASE_TIMEOUT_MS    1000u
#define DFU_MANIFEST_SECTOR_TIMEOUT_MS  150u

#define FLASH_SECTOR_SIZE               (4u * 1024u)

/*----- Typedefs -----------------------------------------------------*/

typedef struct {
	u32  address;
	u32  target_address;
	u32  next_address;
	u32  received_size;
	u8   status;
	bool address_set;
	bool started;
	bool transfer_ended;
} dfu_download_t;

/*----- Static variable definitions ----------------------------------*/

static u8 s_dfu_staging[FLASH_TOTAL_SIZE]
	__attribute__((section(".dfu_staging"), aligned(8)));

static dfu_download_t s_download;

/*----- Static function prototypes -----------------------------------*/

static void _reset_download(void);
static u8 _handle_command(const u8 *data, u16 length);
static u8 _stage_data(u16 block_num, const u8 *data, u16 length);
static u32 _read_le32(const u8 *data);
static u32 _manifest_timeout(void);
static u8 _alt_reflash_cb(void);

/*----- Extern function implementations ------------------------------*/

void boot_dfu_discard_staging(void)
{
	_reset_download();
}

/**
 * @brief   Invoked right before tud_dfu_download_cb() (state=DFU_DNBUSY) or tud_dfu_manifest_cb() (state=DFU_MANIFEST)
 *
 * @param   alt_id   Alt ID passed via command line
 * @param   state    Current state of DFU TUD
 * 
 * @returns Application return timeout in milliseconds (bwPollTimeout) for the next download/manifest operation.
 *          During this period, USB host won't try to communicate with us.
 */
u32 tud_dfu_get_timeout_cb(u8 alt_id, u8 state)
{
	(void)alt_id;

	if (DFU_DNBUSY == state) {
		return DFU_DOWNLOAD_TIMEOUT_MS;
	}

	if (DFU_MANIFEST == state) {
		return _manifest_timeout();
	}

	return 0;
}

/**
 * @brief   Invoked when received DFU_DNLOAD (wLength>0) following by DFU_GETSTATUS (state=DFU_DNBUSY) requests
 * 
 * @param   alt_id     Alt ID passed via command line
 * @param   block_num
 * @param   data
 * @param   length     Max length is CFG_TUD_DFU_XFER_BUFSIZE
 * 
 * @details Once finished flashing, application must call tud_dfu_finish_flashing()
 *          This callback could be returned before flashing op is complete (async).
 */
void tud_dfu_download_cb(
	u8        alt_id,
	u16       block_num,
	u8 const *data,
	u16       length
) {
	u8 status;

	if (!gdb_monitor_target_running()) {
		s_download.status = DFU_STATUS_ERR_NOTDONE;
		goto end;
	}

	if (ALT_REFLASH != alt_id) {
		s_download.status = DFU_STATUS_ERR_TARGET;
		goto end;
	}

	if (DFU_STATUS_OK != s_download.status) {
		goto end;
	}

	if (DFUSE_COMMAND_BLOCK == block_num) {
		s_download.status = _handle_command(data, length);
	} else {
		s_download.status = _stage_data(block_num, data, length);
	}

end:
	status = s_download.status;
	tud_dfu_finish_flashing(status);
	if (DFU_STATUS_OK != status) {
		_reset_download();
	}
}


/**
 * @brief   Invoked when download process is complete, received DFU_DNLOAD (wLength=0)
 *          following by DFU_GETSTATUS (state=Manifest)
 *
 * @details Application can do checksum, or actual flashing if buffered entire image previously.
 *          Once finished flashing, application must call tud_dfu_finish_flashing()
 *
 * @param   alt_id   Alt ID passed via command line
 */
void tud_dfu_manifest_cb(u8 alt_id)
{
	u8 status = s_download.status;

	if (ALT_REFLASH != alt_id) {
		status = DFU_STATUS_ERR_TARGET;
	} else if ((DFU_STATUS_OK == status)
	        && (!s_download.started || (0 == s_download.received_size))) {
		status = DFU_STATUS_ERR_NOTDONE;
	} else if ((DFU_STATUS_OK == status)
	        && s_download.transfer_ended
	        && (!s_download.address_set
	         || (s_download.address != s_download.target_address))) {
		status = DFU_STATUS_ERR_NOTDONE;
	} else if (DFU_STATUS_OK == status) {
		status = _alt_reflash_cb();
	}

	tud_dfu_finish_flashing(status);
	_reset_download();
}

/**
 * @brief   Invoked when the Host has terminated a download or upload transfer
 * 
 * @param   alt_id   Alt ID passed via command line
 */
void tud_dfu_abort_cb(u8 alt_id)
{
	(void)alt_id;

	if ((DFU_STATUS_OK == s_download.status) && s_download.started) {
		// dfu-util ends a successful DfuSe download with ABORT, then sends
		// SET_ADDRESS and manifestation when :leave was requested. Retain
		// staging here, but never write flash from the abort callback.
		s_download.address_set = false;
		s_download.transfer_ended = true;
	} else {
		_reset_download();
	}
}

/**
 * @brief  Invoked when a DFU_DETACH request is received
 */
void tud_dfu_detach_cb(void)
{
}

/*----- Static function implementations ------------------------------*/

static void _reset_download(void)
{
	memset(&s_download, 0, sizeof(s_download));
}

static u8 _handle_command(const u8 *data, u16 length)
{
	if ((NULL == data) || (5u != length)) {
		return DFU_STATUS_ERR_STALLEDPKT;
	}

	if (DFUSE_COMMAND_SET_ADDRESS != data[0]) {
		return DFU_STATUS_ERR_STALLEDPKT;
	}

	u32 address = _read_le32(&data[1]);
	if (address > FLASH_TOTAL_SIZE) {
		return DFU_STATUS_ERR_ADDRESS;
	}

	s_download.address = address;
	s_download.address_set = true;

	if (!s_download.started) {
		s_download.target_address = address;
		s_download.next_address = address;
		s_download.received_size = 0;
	}

	return DFU_STATUS_OK;
}

static u8 _stage_data(u16 block_num, const u8 *data, u16 length)
{
	if ((block_num < DFUSE_FIRST_DATA_BLOCK)
	 || !s_download.address_set
	 || (NULL == data)
	 || (0 == length)) {
		return DFU_STATUS_ERR_STALLEDPKT;
	}

	if (s_download.transfer_ended) {
		u32 address = s_download.address;

		_reset_download();
		s_download.address = address;
		s_download.address_set = true;
	}

	u32 block_offset =
		(u32)(block_num - DFUSE_FIRST_DATA_BLOCK) * CFG_TUD_DFU_XFER_BUFSIZE;

	if ((s_download.address > FLASH_TOTAL_SIZE)
	 || (block_offset > (FLASH_TOTAL_SIZE - s_download.address))) {
		return DFU_STATUS_ERR_ADDRESS;
	}

	u32 target_address = s_download.address + block_offset;
	if ((target_address > FLASH_TOTAL_SIZE)
	 || ((u32)length > (FLASH_TOTAL_SIZE - target_address))) {
		return DFU_STATUS_ERR_ADDRESS;
	}

	if (!s_download.started) {
		s_download.target_address = target_address;
		s_download.next_address = target_address;
		s_download.received_size = 0;
		s_download.started = true;
	}

	if ((target_address != s_download.next_address)
	 || ((u32)length > (FLASH_TOTAL_SIZE - s_download.received_size))) {
		return DFU_STATUS_ERR_ADDRESS;
	}

	memcpy(&s_dfu_staging[s_download.received_size], data, length);
	s_download.received_size += length;
	s_download.next_address += length;

	return DFU_STATUS_OK;
}

static u32 _read_le32(const u8 *data)
{
	return (u32)data[0]
		| ((u32)data[1] << 8)
		| ((u32)data[2] << 16)
		| ((u32)data[3] << 24);
}

static u32 _manifest_timeout(void)
{
	if (!s_download.started || (0 == s_download.received_size)) {
		return DFU_DOWNLOAD_TIMEOUT_MS;
	}

	u32 first_sector = s_download.target_address / FLASH_SECTOR_SIZE;
	u32 last_byte = s_download.target_address + s_download.received_size - 1u;
	u32 last_sector = last_byte / FLASH_SECTOR_SIZE;
	u32 sector_count = last_sector - first_sector + 1u;

	return DFU_MANIFEST_BASE_TIMEOUT_MS
		+ sector_count * DFU_MANIFEST_SECTOR_TIMEOUT_MS;
}


/*----- DFU alts -----------------------------------------------------*/

static u8 _alt_reflash_cb(void)
{
	DLOG(
		"DFU writing %u bytes at flash address 0x%08X",
		(unsigned)s_download.received_size,
		(unsigned)s_download.target_address
	);

	flashio_status_t result = flashio_write_safe(
		s_download.target_address,
		s_dfu_staging,
		s_download.received_size,
		DFU_FLASH_RETRIES,
		NULL
	);

	switch (result) {
	case FLASHIO_SUCCESS:
		return DFU_STATUS_OK;
	case FLASHIO_INVALID_PARAM:
		return DFU_STATUS_ERR_ADDRESS;
	case FLASHIO_READ_FAILED:
		return DFU_STATUS_ERR_VERIFY;
	case FLASHIO_WRITE_REPAIRED:
		return DFU_STATUS_ERR_WRITE;
	case FLASHIO_WRITE_CORRUPT:
		return DFU_STATUS_ERR_FIRMWARE;
	case FLASHIO_SDCARD_FAIL:
	default:
		return DFU_STATUS_ERR_UNKNOWN;
	}
}

/*----- End of file --------------------------------------------------*/
