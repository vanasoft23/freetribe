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
 * @file   boot_image.c
 * 
 * @author vanasoft23
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <csl_cp15.h>

#include "boot_image.h"
#include "bootloader.h"
#include "handoff.h"

/*----- Macros -------------------------------------------------------*/

#define FREETRIBE_TRAILER_MAGIC   "FTKERNL1"
#define FREETRIBE_TRAILER_VERSION "00000001"
#define BOOT_IMAGE_LOAD_ADDR      0xC0000000u
#define BOOT_IMAGE_VSB_MAGIC      "KORG SYSTEM FILEE2"

/*----- Static function prototypes -----------------------------------*/

static void _set_invalid(boot_image_info_t *info);
static bool _parse_hex_u32(const u8 *text, u32 *value);
static bool _is_vsb_factory(
	const u8 *prefix,
	u32       prefix_size,
	u32       image_size
);
static bool _is_freetribe_kernel(
	const u8 *trailer,
	u32       trailer_size,
	u32       image_size,
	u32      *payload_size
);

/*----- Extern function implementations ------------------------------*/

// Classify a boot image from separately supplied head and tail
// buffers, e.g. when the full image is not contiguous in memory.
bool boot_image_classify_parts(
	const u8          *prefix,
	u32                prefix_size,
	const u8          *trailer,
	u32                trailer_size,
	u32                image_size,
	boot_image_info_t *info
) {

	u32 payload_size;

	if (NULL == info) {
		return false;
	}

	_set_invalid(info);

	if (_is_vsb_factory(prefix, prefix_size, image_size)) {
		info->type = BOOT_IMAGE_FACTORY;
		info->payload_offset = BOOT_IMAGE_VSB_HEADER_SIZE;
		info->payload_size = BOOT_IMAGE_FACTORY_PAYLOAD_SIZE;
		return true;
	}

	if (_is_freetribe_kernel(trailer, trailer_size, image_size, &payload_size)) {
		info->type = BOOT_IMAGE_FREETRIBE;
		info->payload_offset = 0;
		info->payload_size = payload_size;
		return true;
	}

	if (BOOT_IMAGE_FACTORY_PAYLOAD_SIZE == image_size) {
		info->type = BOOT_IMAGE_FACTORY;
		info->payload_offset = 0;
		info->payload_size = BOOT_IMAGE_FACTORY_PAYLOAD_SIZE;
		return true;
	}

	return false;

}

// Classify a boot image that is fully resident in memory, deriving
// the head/tail buffers needed by boot_image_classify_parts().
bool boot_image_classify_memory(
	const u8          *image,
	u32                image_size,
	boot_image_info_t *info
) {

	const u8 *trailer = NULL;
	u32 trailer_size = 0;
	u32 prefix_size = image_size;

	if (NULL == image) {
		if (NULL != info) {
			_set_invalid(info);
		}
		return false;
	}

	if (prefix_size > BOOT_IMAGE_VSB_HEADER_SIZE) {
		prefix_size = BOOT_IMAGE_VSB_HEADER_SIZE;
	}

	if (image_size >= BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) {
		trailer = image + image_size - BOOT_IMAGE_FREETRIBE_TRAILER_SIZE;
		trailer_size = BOOT_IMAGE_FREETRIBE_TRAILER_SIZE;
	}

	return boot_image_classify_parts(
		image,
		prefix_size,
		trailer,
		trailer_size,
		image_size,
		info
	);

}

const char *boot_image_type_name(boot_image_type_t type) {

	switch (type) {
	case BOOT_IMAGE_FACTORY:
		return "factory";

	case BOOT_IMAGE_FREETRIBE:
		return "freetribe";

	case BOOT_IMAGE_INVALID:
	default:
		return "invalid";
	}

}

void boot_image_handoff(const boot_image_info_t *info) {

	if (NULL == info) {
		fatal_error("invalid boot image handoff");
	}

	switch (info->type) {
	case BOOT_IMAGE_FREETRIBE:
		 handoff_freetribe_kernel();
		 break;

	case BOOT_IMAGE_FACTORY:
		 handoff_factory_firmware();
		 break;

	case BOOT_IMAGE_INVALID:
	default:
		 fatal_error("invalid boot image handoff");
		 break;
	}

}

/*----- Static function implementations ------------------------------*/


static void _set_invalid(boot_image_info_t *info) {

	info->type = BOOT_IMAGE_INVALID;
	info->payload_offset = 0;
	info->payload_size = 0;

}

// Parse 8 ASCII hex digits at `text` into `value`. Returns false on
// any non-hex character, leaving `value` untouched.
static bool _parse_hex_u32(const u8 *text, u32 *value) {

	u32 result = 0;

	for (u8 i = 0; i < 8; i++) {
		u8 digit = text[i];
		u32 nibble;

		if ((digit >= '0') && (digit <= '9')) {
			nibble = digit - '0';
		} else if ((digit >= 'A') && (digit <= 'F')) {
			nibble = digit - 'A' + 10u;
		} else if ((digit >= 'a') && (digit <= 'f')) {
			nibble = digit - 'a' + 10u;
		} else {
			return false;
		}

		result = (result << 4) | nibble;
	}

	*value = result;
	return true;

}

static bool _is_vsb_factory(
	const u8 *prefix,
	u32       prefix_size,
	u32       image_size
) {

	if ((NULL == prefix) || (prefix_size < (sizeof(BOOT_IMAGE_VSB_MAGIC) - 1))) {
		return false;
	}

	if ((BOOT_IMAGE_FACTORY_PAYLOAD_SIZE + BOOT_IMAGE_VSB_HEADER_SIZE) != image_size) {
		return false;
	}

	return 0 == memcmp(
		prefix,
		BOOT_IMAGE_VSB_MAGIC,
		sizeof(BOOT_IMAGE_VSB_MAGIC) - 1
	);

}

// A Freetribe kernel image ends with a trailer of the form:
//
//   FREETRIBE_TRAILER_MAGIC (8 bytes) + FREETRIBE_TRAILER_VERSION
//   (8 bytes) + 8 ASCII hex digits giving the payload size.
static bool _is_freetribe_kernel(
	const u8 *trailer,
	u32       trailer_size,
	u32       image_size,
	u32      *payload_size
) {

	u32 parsed_payload_size;

	if ((NULL == trailer) || (trailer_size < BOOT_IMAGE_FREETRIBE_TRAILER_SIZE)) {
		return false;
	}

	if (0 != memcmp(trailer, FREETRIBE_TRAILER_MAGIC, 8)) {
		return false;
	}

	if (0 != memcmp(trailer + 8, FREETRIBE_TRAILER_VERSION, 8)) {
		return false;
	}

	if (!_parse_hex_u32(trailer + 16, &parsed_payload_size)) {
		return false;
	}

	if (parsed_payload_size > (0xFFFFFFFFu - BOOT_IMAGE_FREETRIBE_TRAILER_SIZE)) {
		return false;
	}

	if ((parsed_payload_size + BOOT_IMAGE_FREETRIBE_TRAILER_SIZE) != image_size) {
		return false;
	}

	*payload_size = parsed_payload_size;
	return true;

}

/*----- End of file --------------------------------------------------*/
