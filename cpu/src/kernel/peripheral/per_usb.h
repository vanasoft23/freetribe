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

#include "ft.h"

#ifdef __cplusplus
extern "C" {
#endif

bool per_usb_init_device(u8 int_channel);
void per_usb_terminate(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
