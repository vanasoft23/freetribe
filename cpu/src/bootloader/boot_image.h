/*----------------------------------------------------------------------

                     This file is part of Freetribe

                https://github.com/bangcorrupt/freetribe

                                License

                   GNU AFFERO GENERAL PUBLIC LICENSE
                      Version 3, 19 November 2007

                           AGPL-3.0-or-later

----------------------------------------------------------------------*/

#ifndef BOOT_IMAGE_H
#define BOOT_IMAGE_H

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>

/*----- Macros -------------------------------------------------------*/

#define BOOT_IMAGE_FACTORY_PAYLOAD_SIZE (2u * 1024u * 1024u)
#define BOOT_IMAGE_VSB_HEADER_SIZE 256u
#define BOOT_IMAGE_FREETRIBE_TRAILER_SIZE 64u

/*----- Typedefs -----------------------------------------------------*/

typedef enum {
    BOOT_IMAGE_INVALID,
    BOOT_IMAGE_FACTORY,
    BOOT_IMAGE_FREETRIBE
} boot_image_type_t;

typedef struct {
    boot_image_type_t type;
    uint32_t payload_offset;
    uint32_t payload_size;
} boot_image_info_t;

/*----- Extern function prototypes -----------------------------------*/

bool boot_image_classify_parts(
    const uint8_t     *prefix,
    uint32_t           prefix_size,
    const uint8_t     *trailer,
    uint32_t           trailer_size,
    uint32_t           image_size,
    boot_image_info_t *info
);

bool boot_image_classify_memory(
    const uint8_t     *image,
    uint32_t           image_size,
    boot_image_info_t *info
);

const char *boot_image_type_name(boot_image_type_t type);
void boot_image_handoff(const boot_image_info_t *info);

#endif

/*----- End of file --------------------------------------------------*/
