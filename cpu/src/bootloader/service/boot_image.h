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

#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define DDR_FIRMWARE_BASE      ((u32)(uintptr_t)&_ddr2_start)
#define FIRMWARE_RESERVED_SIZE ((u32)(uintptr_t)&__firmware_reserved_size)

#define BOOT_IMAGE_FACTORY_PAYLOAD_SIZE   (2u * 1024u * 1024u)
#define BOOT_IMAGE_VSB_HEADER_SIZE        256u
#define BOOT_IMAGE_FREETRIBE_TRAILER_SIZE 64u

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    BOOT_IMAGE_INVALID,
    BOOT_IMAGE_FACTORY,
    BOOT_IMAGE_FREETRIBE
} boot_image_type_t;

typedef struct {
    boot_image_type_t type;
    u32               payload_offset;
    u32               payload_size;
} boot_image_info_t;

/*----- Extern variable declarations ---------------------------------*/

extern u8 _ddr2_start;
extern u8 __firmware_reserved_size;

/*----- Extern function prototypes -----------------------------------*/

bool boot_image_classify_parts(
    const u8          *prefix,
    u32                prefix_size,
    const u8          *trailer,
    u32                trailer_size,
    u32                image_size,
    boot_image_info_t *info
);

bool boot_image_classify_memory(
    const u8          *image,
    u32                image_size,
    boot_image_info_t *info
);

const char *boot_image_type_name(boot_image_type_t type);
void boot_image_handoff(const boot_image_info_t *info);

#endif

/*----- End of file --------------------------------------------------*/
