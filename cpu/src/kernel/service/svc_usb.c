/*----------------------------------------------------------------------

                     This file is part of Freetribe

----------------------------------------------------------------------*/

/**
 * @file svc_usb.c
 *
 * @brief FreeRTOS-owned TinyUSB service.
 */

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "dev_usb.h"
#include "svc_fatfs.h"

#include "svc_usb.h"

#define USB_INT_CHANNEL 11
#define USB_TASK_PERIOD_MS 1

static SemaphoreHandle_t g_usb_mutex = NULL;
static volatile bool g_usb_ready = false;
static volatile bool g_usb_init_failed = false;
static volatile t_svc_usb_msc_export_status g_msc_export_status =
    SVC_USB_MSC_EXPORT_DISABLED;

static bool _usb_lock(TickType_t timeout);
static void _usb_unlock(void);
static void _usb_idle_failed(void);
static t_svc_usb_msc_export_status
_usb_msc_status_from_device(t_dev_usb_msc_export_status status);

void svc_usb_task(void *param) {

    (void)param;

    g_usb_mutex = xSemaphoreCreateMutex();
    configASSERT(g_usb_mutex != NULL);

    if (!dev_usb_init_device(USB_INT_CHANNEL)) {
        g_usb_init_failed = true;
        _usb_idle_failed();
    }

    g_usb_ready = true;

    while (true) {
        if (_usb_lock(portMAX_DELAY)) {
            dev_usb_task();
            _usb_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(USB_TASK_PERIOD_MS));
    }
}

bool svc_usb_ready(void) { return g_usb_ready && dev_usb_ready(); }

bool svc_usb_init_failed(void) { return g_usb_init_failed; }

bool svc_usb_mounted(void) {

    bool mounted = false;

    if (!svc_usb_ready()) {
        return false;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        mounted = dev_usb_mounted();
        _usb_unlock();
    }

    return mounted;
}

bool svc_usb_cdc_connected(void) {

    bool connected = false;

    if (!svc_usb_ready()) {
        return false;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        connected = dev_usb_cdc_connected();
        _usb_unlock();
    }

    return connected;
}

uint32_t svc_usb_cdc_available(void) {

    uint32_t available = 0;

    if (!svc_usb_ready()) {
        return 0;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        available = dev_usb_cdc_available();
        _usb_unlock();
    }

    return available;
}

uint32_t svc_usb_cdc_read(void *buffer, uint32_t length) {

    uint32_t read = 0;

    if ((buffer == NULL) || (length == 0) || !svc_usb_ready()) {
        return 0;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        read = dev_usb_cdc_read(buffer, length);
        _usb_unlock();
    }

    return read;
}

uint32_t svc_usb_cdc_write(const void *buffer, uint32_t length) {

    uint32_t written = 0;

    if ((buffer == NULL) || (length == 0) || !svc_usb_ready()) {
        return 0;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        written = dev_usb_cdc_write(buffer, length);
        _usb_unlock();
    }

    return written;
}

uint32_t svc_usb_vendor_write(const void *buffer, uint32_t length) {

    uint32_t written = 0;

    if ((buffer == NULL) || (length == 0) || !svc_usb_ready()) {
        return 0;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        written = dev_usb_vendor_write(buffer, length);
        _usb_unlock();
    }

    return written;
}

bool svc_usb_msc_export_sdcard_readonly(void) {

    bool exported = false;

    if (!svc_usb_ready()) {
        g_msc_export_status = SVC_USB_MSC_EXPORT_USB_NOT_READY;
        return false;
    }

    if (!svc_fatfs_sdcard_begin_usb_msc_export()) {
        g_msc_export_status = SVC_USB_MSC_EXPORT_SDCARD_BUSY;
        return false;
    }

    if (_usb_lock(portMAX_DELAY)) {
        exported = dev_usb_msc_export_sdcard_readonly();
        g_msc_export_status =
            _usb_msc_status_from_device(dev_usb_msc_export_status());
        _usb_unlock();
    } else {
        g_msc_export_status = SVC_USB_MSC_EXPORT_DEVICE_ERROR;
    }

    if (!exported) {
        svc_fatfs_sdcard_end_usb_msc_export();
    }

    return exported;
}

void svc_usb_msc_disable(void) {

    if (!svc_usb_ready()) {
        g_msc_export_status = SVC_USB_MSC_EXPORT_DISABLED;
        svc_fatfs_sdcard_end_usb_msc_export();
        return;
    }

    if (_usb_lock(portMAX_DELAY)) {
        dev_usb_msc_disable();
        g_msc_export_status = SVC_USB_MSC_EXPORT_DISABLED;
        _usb_unlock();
    }

    svc_fatfs_sdcard_end_usb_msc_export();
}

bool svc_usb_msc_exported(void) {

    bool exported = false;

    if (!svc_usb_ready()) {
        return false;
    }

    if (_usb_lock(pdMS_TO_TICKS(USB_TASK_PERIOD_MS))) {
        exported = dev_usb_msc_exported();
        _usb_unlock();
    }

    return exported;
}

t_svc_usb_msc_export_status svc_usb_msc_export_status(void) {

    return g_msc_export_status;
}

static bool _usb_lock(TickType_t timeout) {

    if (g_usb_mutex == NULL) {
        return false;
    }

    return xSemaphoreTake(g_usb_mutex, timeout) == pdPASS;
}

static void _usb_unlock(void) { xSemaphoreGive(g_usb_mutex); }

static void _usb_idle_failed(void) {

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static t_svc_usb_msc_export_status
_usb_msc_status_from_device(t_dev_usb_msc_export_status status) {

    switch (status) {
    case DEV_USB_MSC_EXPORT_OK:
        return SVC_USB_MSC_EXPORT_OK;
    case DEV_USB_MSC_EXPORT_DISABLED:
        return SVC_USB_MSC_EXPORT_DISABLED;
    case DEV_USB_MSC_EXPORT_NO_CARD:
        return SVC_USB_MSC_EXPORT_NO_CARD;
    case DEV_USB_MSC_EXPORT_INIT_FAILED:
        return SVC_USB_MSC_EXPORT_SDCARD_INIT_FAILED;
    case DEV_USB_MSC_EXPORT_NO_BLOCKS:
        return SVC_USB_MSC_EXPORT_NO_BLOCKS;
    default:
        return SVC_USB_MSC_EXPORT_DEVICE_ERROR;
    }
}

/*----- End of file --------------------------------------------------*/
