/*----------------------------------------------------------------------

                     This file is part of Freetribe

----------------------------------------------------------------------*/

#ifndef KERNEL_TUSB_CONFIG_H
#define KERNEL_TUSB_CONFIG_H

#ifndef OPT_MCU_AM1802
#define OPT_MCU_AM1802 520
#endif

#define CFG_TUSB_MCU OPT_MCU_AM1802
#define CFG_TUSB_OS OPT_OS_FREERTOS

#define TUP_USBIP_MUSB
#define TUP_USBIP_MUSB_AM1802
#define TUP_DCD_ENDPOINT_MAX 4

#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUD_ENDPOINT0_SIZE 64
#define CFG_TUD_ENDPPOINT_MAX 4

#define CFG_TUD_CDC 1
#define CFG_TUD_MSC 1
#define CFG_TUD_HID 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

#define CFG_TUD_TASK_QUEUE_SZ 16
#define CFG_TUD_TASK_EVENTS_PER_RUN 8
#define CFG_TUD_CDC_RX_BUFSIZE 256
#define CFG_TUD_CDC_TX_BUFSIZE 256
#define CFG_TUD_CDC_EP_BUFSIZE 64
#define CFG_TUD_MSC_EP_BUFSIZE 512

#define CFG_TUSB_DEBUG 0

#endif

/*----- End of file --------------------------------------------------*/
