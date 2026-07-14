/*----------------------------------------------------------------------

                     This file is part of Freetribe

----------------------------------------------------------------------*/

/**
 * @file svc_usb.h
 *
 * @brief USB service task.
 */

#ifndef SVC_USB_H
#define SVC_USB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SVC_USB_MSC_EXPORT_OK = 0,
    SVC_USB_MSC_EXPORT_DISABLED,
    SVC_USB_MSC_EXPORT_USB_NOT_READY,
    SVC_USB_MSC_EXPORT_SDCARD_BUSY,
    SVC_USB_MSC_EXPORT_NO_CARD,
    SVC_USB_MSC_EXPORT_SDCARD_INIT_FAILED,
    SVC_USB_MSC_EXPORT_NO_BLOCKS,
    SVC_USB_MSC_EXPORT_DEVICE_ERROR,
} t_svc_usb_msc_export_status;

void svc_usb_task(void *param);
bool svc_usb_ready(void);
bool svc_usb_init_failed(void);
bool svc_usb_mounted(void);
bool svc_usb_cdc_connected(void);
uint32_t svc_usb_cdc_available(void);
uint32_t svc_usb_cdc_read(void *buffer, uint32_t length);
uint32_t svc_usb_cdc_write(const void *buffer, uint32_t length);
uint32_t svc_usb_vendor_write(const void *buffer, uint32_t length);
bool svc_usb_msc_export_sdcard_readonly(void);
void svc_usb_msc_disable(void);
bool svc_usb_msc_exported(void);
t_svc_usb_msc_export_status svc_usb_msc_export_status(void);

#ifdef __cplusplus
}
#endif
#endif

/*----- End of file --------------------------------------------------*/
