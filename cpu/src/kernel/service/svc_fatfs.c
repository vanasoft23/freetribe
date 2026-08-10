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
 * @file    svc_fatfs.c.
 *
 * @brief   Filesystem service layer.
 */

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include <ff.h>
#include <diskio.h>

#include "dev_sdcard.h"
#include "svc_fatfs.h"

/*----- Macros -------------------------------------------------------*/

#define SVC_FATFS_SECTOR_SIZE 512u

/*----- Typedefs -----------------------------------------------------*/

/*----- Extern variable definitions ----------------------------------*/

FATFS g_fatfs;
PARTITION VolToPart[FF_VOLUMES] = { { 0, 0 } };

/*----- Extern variable definitions ----------------------------------*/

/*----- Static function prototypes -----------------------------------*/

static DSTATUS _disk_not_ready_status(void);
static bool    _is_word_aligned(const void *ptr);

/*----- Extern function implementations ------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static bool s_disk_initialized = false;
static u32 s_sector_bounce[SVC_FATFS_SECTOR_SIZE / sizeof(u32)];

/**
 * @brief   Initialize Disk Drive.
 * 
 * @param   pdrv   Physical drive number (0)
 */
DSTATUS disk_initialize(BYTE pdrv) {

    if (0 != pdrv) {
        return STA_NOINIT;
    }

    if (!dev_sdcard_present()) {
        s_disk_initialized = false;
        return _disk_not_ready_status();
    }

    if (SDCARD_OK != dev_sdcard_init()) {
        s_disk_initialized = false;
        DEBUG_LOG("disk_initialize() dev_sdcard_init() failed");
        return STA_NOINIT;
    }
    
    s_disk_initialized = true;
    return 0;
}

/**
 * @brief   Returns the current status of a drive.
 * 
 * @param   drv   Physical drive number (0)
 */
DSTATUS disk_status(BYTE drv) {

    if (0 != drv) {
        return STA_NOINIT;
    }

    if (!dev_sdcard_present()) {
        s_disk_initialized = false;
        return _disk_not_ready_status();
    }

    return s_disk_initialized ? 0 : STA_NOINIT;

}

/**
 * @brief   Read sector(s) from the disk drive.
 * 
 * @param   drv     Physical drive number (0)
 * @param   buff    Pointer to the data buffer to store read data
 * @param   sector  Start sector in LBA
 * @param   count   Number of sectors to read
 */
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
	
    DEBUG_LOG("disk_read(%i, %p, %i, %i)", (int)pdrv, (void*)buff, (int)sector, (int)count);

    if ((0 != pdrv) || (NULL == buff) || (0 == count)) {
        return RES_PARERR;
    }

    if (!s_disk_initialized || !dev_sdcard_present()) {
        s_disk_initialized = false;
        return RES_NOTRDY;
    }

    if (_is_word_aligned(buff)) {
        t_sdcard_status st = dev_sdcard_read(sector, count, (u32 *)buff);

        if (SDCARD_OK != st) {
            DEBUG_LOG("disk_read->dev_sdcard_read error: %i", (int)st);
            return RES_ERROR;
        }

        return RES_OK;
    }

    for (UINT i = 0; i < count; i++) {
        t_sdcard_status st = dev_sdcard_read(sector + i, 1,
                                             s_sector_bounce);

        if (SDCARD_OK != st) {
            DEBUG_LOG("disk_read->dev_sdcard_read error: %i", (int)st);
            return RES_ERROR;
        }

        memcpy(buff + (i * SVC_FATFS_SECTOR_SIZE), s_sector_bounce,
               SVC_FATFS_SECTOR_SIZE);
    }

	return RES_OK;
}

/**
 * @brief   Write sector(s) to the disk drive.
 * 
 * @param   pdrv    Physical drive number (0)
 * @param   buff    Data to be written
 * @param   sector  Start sector in LBA
 * @param   count   Number of sectors to write
 */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {

    DEBUG_LOG("disk_write(%i, %p, %i, %i)", (int)pdrv, (void*)buff, (int)sector, (int)count);

    if ((0 != pdrv) || (NULL == buff) || (0 == count)) {
        return RES_PARERR;
    }

    if (!s_disk_initialized || !dev_sdcard_present()) {
        s_disk_initialized = false;
        return RES_NOTRDY;
    }

    if (_is_word_aligned(buff)) {
        t_sdcard_status st = dev_sdcard_write(sector, count,
                                              (const u32 *)buff);

        if (SDCARD_OK != st) {
            DEBUG_LOG("disk_write->dev_sdcard_write error: %i", (int)st);
            return RES_ERROR;
        }

        return RES_OK;
    }

    for (UINT i = 0; i < count; i++) {
        memcpy(s_sector_bounce, buff + (i * SVC_FATFS_SECTOR_SIZE),
               SVC_FATFS_SECTOR_SIZE);

        t_sdcard_status st = dev_sdcard_write(sector + i, 1,
                                              s_sector_bounce);

        if (SDCARD_OK != st) {
            DEBUG_LOG("disk_write->dev_sdcard_write error: %i", (int)st);
            return RES_ERROR;
        }
    }

    return RES_OK;
}

/**
 * @brief   Control interface between SD card driver and FatFS. FatFS
 *          calls this function to inform itself about the SD card driver.
 * 
 * @param   pdrv    Physical drive number (0)
 * @param   cmd     Control code
 * @param   buff    Buffer to send/receive control data
 */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {

    if (0 != pdrv) {
        return RES_PARERR;
    }

    if (!s_disk_initialized || !dev_sdcard_present()) {
        s_disk_initialized = false;
        return RES_NOTRDY;
    }

    switch (cmd) {

    case CTRL_SYNC:
        return RES_OK;

    case GET_SECTOR_COUNT:
        if (NULL == buff) {
            return RES_PARERR;
        }
        *(LBA_t *)buff = (LBA_t)dev_sdcard_get_sector_count();
        return (*(LBA_t *)buff > 0) ? RES_OK : RES_NOTRDY;

    case GET_SECTOR_SIZE:
        if (NULL == buff) {
            return RES_PARERR;
        }
        *(WORD *)buff = SVC_FATFS_SECTOR_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        if (NULL == buff) {
            return RES_PARERR;
        }
        *(DWORD *)buff = 1;
        return RES_OK;

    default:
        return RES_PARERR;

    }

}

/**
 * @brief   Real time clock service to be called from FatFS module.
 *          Any valid time must be returned even if the system does
 *          not support a real time clock.
 */
DWORD get_fattime(void) {
    // @TODO: implement RTC
    return   ((2007UL-1980) << 25)  // Year 2007
            | (6UL << 21)           // Month June
            | (5UL << 16)           // Day 5
            | (11U << 11)           // Hour 11
            | (38U << 5)            // Min 38
            | (0U >> 1);            // Sec 0
}

/*----- Static function implementations ------------------------------*/

static DSTATUS _disk_not_ready_status(void) {
    return STA_NOINIT | STA_NODISK;
}

static bool _is_word_aligned(const void *ptr) {
    return 0 == (((uintptr_t)ptr) & (sizeof(u32) - 1u));
}

/*----- End of file --------------------------------------------------*/
