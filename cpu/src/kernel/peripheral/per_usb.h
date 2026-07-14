/*----------------------------------------------------------------------

                     This file is part of Freetribe

----------------------------------------------------------------------*/

/**
 * @file per_usb.h
 *
 * @brief AM1802 USB peripheral bridge.
 */

#ifndef PER_USB_H
#define PER_USB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool per_usb_init_device(uint8_t int_channel);
void per_usb_terminate(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
