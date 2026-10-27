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
 * @file    install.c
 *
 * @brief   Build and safely install the bootloader AIS image.
 * 
 * @details During startup, the original SBL image is copied from OC RAM to
 *          DDR before mutable static data is used. The saved image can then
 *          be wrapped in an AIS script and installed later.
 *
 * @author  vanasoft23 (mvandijk303@gmail.com)
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "flash_io.h"
#include "install.h"

/*----- Macros -------------------------------------------------------*/

#define SRAM_START                  0x80000000u
#define BOOTSECT_SIZE               (128u * 1024u)
#define BOOTSECT_CHECKSUM_SIZE      2u
#define BOOTSECT_CHECKSUM_TARGET    0x0000u

#ifdef DEBUG_BOOTLOADER_INSTALL
#  define BOOTSECT_START            0x00F00000u /* safe address in flash */
#else
#  define BOOTSECT_START            0x00000000u
#endif

#define SBL_SIZE ((u32)(uintptr_t)&__sbl_max_size)

/*----- Static variable definitions ----------------------------------*/

__attribute__((section(".ais_head"), used))
	static const u8 s_ais_head[] = {
		// "TIPA": AIS bootscript magic
		0x54, 0x49, 0x50, 0x41,
		// Sequential Read Enable
		0x63, 0x59, 0x53, 0x58,
		// Function Execute 6: PLL and Clock Configuration
		0x0D, 0x59, 0x53, 0x58, 0x06, 0x00, 0x03, 0x00, 
		0x01, 0x00, 0x18, 0x00, 0x05, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
		// Section Load: With this command we load our own SBL into SRAM at 0x80000000
		0x01, 0x59, 0x53, 0x58,
		0x00, 0x00, 0x00, 0x80,
		0x00, 0x00, 0x00, 0x00 // Section size, filled in at runtime.
	};

__attribute__((section(".ais_tail"), used))
	static const u8 s_ais_tail[] = {
		// Jump and Close command
		0x06, 0x59, 0x53, 0x58, 0x00, 0x00, 0x00, 0x80, 
	};

__attribute__((section(".bootsect_cache"), aligned(8)))
	static u8 s_bootsect_image[BOOTSECT_SIZE];

extern u8 __sbl_max_size;

/*----- Static function declarations ---------------------------------*/

static u16  _sum_le16(const u8 *buffer, u32 len);
static void _construct_new_bootsect(void);

/*----- Extern function implementations ------------------------------*/

/**
 * @brief   Creates a copy of the bootloader code to DDR, which can be
            installed later.
 * @details Q: Why do we need a DDR mirror?
 *          A: Because data used by the bootloader is subject to change.
 */
void create_sbl_ddr_snapshot(void)
{
	memcpy(
		&s_bootsect_image[sizeof(s_ais_head)],
		(const void *)SRAM_START,
		SBL_SIZE
	);
}

/**
 * @brief   Install bootloader into first 128 KiB of flash.
 * @details The boot section exists of 4 parts:
 *          1. AIS script beginning.
 *          2. our SBL payload, which is part of AIS script.
 *          3. AIS script ending, which is the instruction to jump to our SBL.
 *          4. 16-bit checksum
 */
bootsect_res_t install_sbl_to_flash(void)
{
	_construct_new_bootsect();

	if (BOOTSECT_CHECKSUM_TARGET !=
		_sum_le16(s_bootsect_image, BOOTSECT_SIZE)) {
		return BOOTSECT_INSTALL_FAIL_IMAGE;
	}

	flashio_status_t flash_status = flashio_write_safe(
		BOOTSECT_START,
		s_bootsect_image,
		BOOTSECT_SIZE,
		3,
		NULL
	);

	switch (flash_status) {
	case FLASHIO_SUCCESS:
		return BOOTSECT_INSTALL_SUCCESS;
	case FLASHIO_READ_FAILED:
		return BOOTSECT_INSTALL_FAIL_BACKUP_READ;
	case FLASHIO_WRITE_REPAIRED:
		return BOOTSECT_INSTALL_FAIL_REPAIRED;
	case FLASHIO_WRITE_CORRUPT:
		return BOOTSECT_INSTALL_FAIL_CORRUPTED;
	case FLASHIO_INVALID_PARAM:
	case FLASHIO_SDCARD_FAIL:
	default:
		return BOOTSECT_UNKNOWN_ERROR;
	}
}

/*----- Static function implementations ------------------------------*/

/**
 * @brief   Sum little-endian 16-bit words modulo 65536.
 */
static u16 _sum_le16(const u8 *buffer, u32 len)
{
	u16 sum = 0;

	for (u32 i = 0; i < len; i += sizeof(u16)) {
		u16 word = buffer[i] | ((u16)buffer[i + 1] << 8);
		sum += word;
	}

	return sum;
}

/**
 * @brief   Wrap the saved SBL image in AIS commands and add the KORG
 *          boot-section checksum.
 */
static void _construct_new_bootsect(void)
{
	const u32 size_offset = sizeof(s_ais_head) - sizeof(u32);
	const u32 tail_offset = sizeof(s_ais_head) + SBL_SIZE;
	const u32 checksum_offset = BOOTSECT_SIZE - BOOTSECT_CHECKSUM_SIZE;
	u32 section_size = SBL_SIZE;

	memcpy(s_bootsect_image, s_ais_head, sizeof(s_ais_head));
	memcpy(&s_bootsect_image[size_offset], &section_size, sizeof(section_size));
	memcpy(&s_bootsect_image[tail_offset], s_ais_tail, sizeof(s_ais_tail));

	// Factory boot sections have a total 16-bit word sum of zero.
	u16 checksum = BOOTSECT_CHECKSUM_TARGET -
		_sum_le16(s_bootsect_image, checksum_offset);
	memcpy(
		&s_bootsect_image[checksum_offset],
		&checksum,
		sizeof(checksum)
	);
}
