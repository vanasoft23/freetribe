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
 *  @file   dev_flash.c
 *
 *  @brief  Flash memory device driver.
 */

/// TODO: Flash access is very slow.
//          Possible issue with SPI clock.
//              Check if fixed now cache enabled.

/*----- Includes -----------------------------------------------------*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ft_error.h"

#include "per_spi.h"

#include "dev_flash.h"

/// TODO: Use device layer delay, or SPI peripheral timing.
#include "svc_delay.h"

/*----- Macros -------------------------------------------------------*/

// SPI settings
#define FLASH_SPI             SPI_1
#define FLASH_SPI_PIN_FUNC                                                   \
    (SPI_PIN_SOMI | SPI_PIN_SIMO | SPI_PIN_CLK | SPI_PIN_CS0 | SPI_PIN_CS1)
#define FLASH_SPI_PIN_DIR     0
#define FLASH_SPI_DATA_FORMAT SPI_DATA_FORMAT0
#define FLASH_SPI_FREQ        SPI_FREQ_37_5_MHZ
#define FLASH_SPI_CHAR_LENGTH 8

#define SPI_FLASH_CS 0x1

/// TODO: typedef enum {} t_flash_command;
#define WRITE_IN_PROGRESS 0x01
#define WRITE_ENABLE_LATCH 0x02
#define PROGRAM_FAIL 0x20
#define ERASE_FAIL 0x40

#define FLASH_PAGE_PROGRAM 0x02
#define FLASH_READ 0x03
#define FLASH_WRITE_DISABLE 0x04
#define FLASH_WRITE_ENABLE 0x06
#define FLASH_SECTOR_ERASE 0x20
#define FLASH_BLOCK_32_ERASE 0x52
#define FLASH_BLOCK_64_ERASE 0xd8

#define FLASH_READ_STATUS 0x05
#define FLASH_READ_CONFIG 0x15
#define FLASH_READ_SECURITY 0x2b
#define FLASH_READ_LOCK 0x2d
#define FLASH_READ_SPB_LOCK 0xa7
#define FLASH_READ_SPB 0xe2
#define FLASH_ERASE_SPB 0xe4
#define FLASH_READ_DPB 0xe0
#define FLASH_GANG_BLOCK_UNLOCK 0x98

#define PAGE_LENGTH 0x100
#define SECTOR_LENGTH 0x1000
#define PAGES_PER_SECTOR 0x10

#define SECTOR_OFFSET_MASK 0xfff
#define SECTOR_INDEX_SHIFT 0xc

/*----- Typedefs -----------------------------------------------------*/

/*----- Static variable definitions ----------------------------------*/

static uint8_t g_sector_buffer[SECTOR_LENGTH] = {0};

/*----- Static function prototypes -----------------------------------*/

static void _flash_transaction_begin(void);
static void _flash_command(uint8_t cmd);
static void _flash_command_end(uint8_t cmd);
static void _flash_address(uint32_t address);
static void _flash_address_end(uint32_t address);
static void _flash_address_32bit(uint32_t address);
static void _flash_pack_address(uint32_t address, uint8_t addr[3]);
static void _flash_pack_address_32bit(uint32_t address, uint8_t addr[4]);
static void _flash_tx(uint8_t *p_tx, uint32_t len);
static void _flash_tx_end(uint8_t *p_tx, uint32_t len);
static void _flash_rx(uint8_t *p_rx, uint32_t len);
static void _flash_rx_end(uint8_t *p_rx, uint32_t len);
static void _sector_erase(uint32_t sector_addr);
static void _sector_write(uint32_t dest, uint8_t *p_src);
static void _page_program(uint32_t dest, uint8_t *p_src);
static bool _write_enable(void);
static uint8_t _read_status(void);
static uint8_t _read_security(void);
static uint16_t _read_lock(void);
static uint8_t _read_spb_lock(void);
static bool _read_spb(uint32_t addr);
static bool _read_dpb(uint32_t addr);
static void _erase_spb(void);
static void _write_disable(void);
static void _gang_block_unlock(void);
static bool _flash_busy(void);

/*----- Extern variable definitions ----------------------------------*/

/*----- Extern function implementations ------------------------------*/

t_status dev_flash_init(void) {

    // per_spi1_init(); // Flash
    
    // ////////////////
    // HWREG(SOC_SPI_1_REGS + SPI_SPIGCR0) = 0;
    // delay_block_us(5);
    // HWREG(SOC_SPI_1_REGS + SPI_SPIGCR0) |= 1;
    // HWREG(SOC_SPI_1_REGS + SPI_SPIGCR1) = 3; // confirmed
    // HWREG(SOC_SPI_1_REGS + SPI_SPIPC(0)) = 0xe00; // 0x00000F03 in custom SBL
    // HWREG(SOC_SPI_1_REGS + SPI_SPIPC(1)) = 3;
    // HWREG(SOC_SPI_1_REGS + SPI_SPIFMT(0)) = 0x00010308;
    // HWREG(SOC_SPI_1_REGS + SPI_SPIGCR1) |= SPI_SPIGCR1_ENABLE;
    // delay_block_us(5);
    // ////////////////
    t_spi_config config = {
        .instance = FLASH_SPI,
        .int_channel = 0,
        .int_level = 0,
        .pin_func = FLASH_SPI_PIN_FUNC,
        .pin_dir = FLASH_SPI_PIN_DIR,
        .int_enable = false
    };

    per_spi_init(&config);

    t_spi_format format = {
        .instance = FLASH_SPI,
        .index = FLASH_SPI_DATA_FORMAT,
        .freq = FLASH_SPI_FREQ,
        .char_length = FLASH_SPI_CHAR_LENGTH,
    };

    per_spi_set_data_format(&format);
    per_spi_chip_format(FLASH_SPI, FLASH_SPI_DATA_FORMAT, SPI_FLASH_CS, false);
    delay_block_us(5);

    return SUCCESS;
}


void dev_flash_read(uint32_t src, uint8_t *p_dest, uint32_t len) {

    // Wait for any write operations to complete.
    while (_flash_busy())
        ;

    _flash_transaction_begin();

    // Send read command.
    _flash_command(FLASH_READ);

    // Send source address.
    _flash_address(src);

    // Receive data.
    _flash_rx_end(p_dest, len);

}

void dev_flash_write(uint32_t dest, uint8_t *p_src, uint32_t len) {

    uint16_t sector_offset = dest & SECTOR_OFFSET_MASK;

    // If destination address not sector aligned.
    if (sector_offset) {

        // Start address of target sector.
        dest -= sector_offset;

        // Number of bytes to copy.
        uint16_t copy_length = SECTOR_LENGTH - sector_offset;

        // Read target sector.
        dev_flash_read(dest, g_sector_buffer, SECTOR_LENGTH);

        // Copy source data to sector buffer.
        memcpy(g_sector_buffer + sector_offset, p_src, copy_length);

        _sector_erase(dest);

        _sector_write(dest, g_sector_buffer);

        p_src += copy_length;
        dest += SECTOR_LENGTH;
        len -= copy_length;

        // Handle underflow.
        if ((int32_t)len < 0) {
            len = 0;
        }
    }

    // Write complete sectors.
    while (len >> SECTOR_INDEX_SHIFT) {

        _sector_erase(dest);

        _sector_write(dest, p_src);

        p_src += SECTOR_LENGTH;
        dest += SECTOR_LENGTH;
        len -= SECTOR_LENGTH;
    }

    // If partial final sector.
    if (len) {

        // Read target sector.
        dev_flash_read(dest, g_sector_buffer, SECTOR_LENGTH);

        // Copy source data to sector buffer.
        memcpy(g_sector_buffer, p_src, len);

        _sector_erase(dest);

        _sector_write(dest, g_sector_buffer);
    }
}

bool dev_flash_verify(uint32_t flash_addr, uint8_t *p_ram_data, uint32_t len) {

    bool verified = true;
    uint16_t i;

    while (len > SECTOR_LENGTH) {

        // Read data from flash (does not need to be sector aligned).
        dev_flash_read(flash_addr, g_sector_buffer, SECTOR_LENGTH);

        for (i = 0; i < SECTOR_LENGTH; i++) {

            if (g_sector_buffer[i] != p_ram_data[i]) {
                verified = false;
            }
        }

        p_ram_data += SECTOR_LENGTH;
        flash_addr += SECTOR_LENGTH;
        len -= SECTOR_LENGTH;
    }

    dev_flash_read(flash_addr, g_sector_buffer, len);

    for (i = 0; i < len; i++) {

        if (g_sector_buffer[i] != p_ram_data[i]) {
            verified = false;
        }
    }

    return verified;
}

void dev_flash_erase(uint32_t address, uint32_t len) {

    uint16_t sector_offset = address & SECTOR_OFFSET_MASK;

    // If destination address not sector aligned.
    if (sector_offset) {

        // Start address of target sector.
        address -= sector_offset;

        // Number of bytes to set.
        uint16_t set_length = SECTOR_LENGTH - sector_offset;

        // Read target sector.
        dev_flash_read(address, g_sector_buffer, SECTOR_LENGTH);

        memset(g_sector_buffer + sector_offset, 0xff, set_length);

        _sector_erase(address);

        _sector_write(address, g_sector_buffer);

        address += SECTOR_LENGTH;
        len -= set_length;
    }

    while (address >> SECTOR_INDEX_SHIFT) {

        _sector_erase(address);

        address += SECTOR_LENGTH;
        len -= SECTOR_LENGTH;
    }

    if (len) {

        dev_flash_read(address, g_sector_buffer, SECTOR_LENGTH);

        memset(g_sector_buffer, 0xff, len);

        _sector_erase(address);

        _sector_write(address, g_sector_buffer);
    }
}

void dev_flash_unlock(void) { _gang_block_unlock(); }

/*----- Static function implementations ------------------------------*/

static void _sector_write(uint32_t dest, uint8_t *p_src) {

    uint8_t i;

    for (i = 0; i < PAGES_PER_SECTOR; i++) {

        _page_program(dest, p_src);

        dest += PAGE_LENGTH;
        p_src += PAGE_LENGTH;
    }
}

static void _sector_erase(uint32_t sector_addr) {

    while (!_write_enable())
        ;

    _flash_transaction_begin();

    _flash_command(FLASH_SECTOR_ERASE);

    _flash_address_end(sector_addr);

    while (_flash_busy())
        ;

    while (_read_status() & WRITE_ENABLE_LATCH)
        ;

    if (_read_security() & ERASE_FAIL) {
        /// TODO: Handle error.
    }
}

static void _page_program(uint32_t dest, uint8_t *p_src) {

    while (!_write_enable())
        ;

    _flash_transaction_begin();

    _flash_command(FLASH_PAGE_PROGRAM);

    _flash_address(dest);
 
    _flash_tx_end(p_src, PAGE_LENGTH);

    while (_flash_busy())
        ;

    while (_read_status() & WRITE_ENABLE_LATCH)
        ;

    if (_read_security() & PROGRAM_FAIL) {
        /// TODO: Handle error.
    }
}

static bool _flash_busy(void) { return _read_status() & WRITE_IN_PROGRESS; }

static bool _write_enable(void) {

    _flash_transaction_begin();
    _flash_command_end(FLASH_WRITE_ENABLE);

    return _read_status() & WRITE_ENABLE_LATCH;
}

static uint8_t _read_reg_byte(uint8_t cmd) {

    uint8_t flash_reg = 0;

    _flash_transaction_begin();
    _flash_command(cmd);

    _flash_rx_end(&flash_reg, 1);

    return flash_reg;
}

static uint16_t _read_reg_short(uint8_t cmd) {

    uint8_t flash_reg[2] = {0};

    _flash_transaction_begin();
    _flash_command(cmd);

    _flash_rx_end(&flash_reg[0], 2);

    return (flash_reg[0] << 8) | flash_reg[1];
}

static uint8_t _read_status(void) { return _read_reg_byte(FLASH_READ_STATUS); }

static uint8_t _read_config(void) { return _read_reg_byte(FLASH_READ_CONFIG); }

static uint8_t _read_security(void) { return _read_reg_byte(FLASH_READ_SECURITY); }

static uint16_t _read_lock(void) { return _read_reg_short(FLASH_READ_LOCK); }

static uint8_t _read_spb_lock(void) { return _read_reg_byte(FLASH_READ_SPB_LOCK); }

static bool _read_spb(uint32_t addr) {

    uint8_t spb = 0;

    _flash_transaction_begin();
    _flash_command(FLASH_READ_SPB);
    _flash_address_32bit(addr);

    _flash_rx_end(&spb, 1);

    return (bool)spb;
}

static bool _read_dpb(uint32_t addr) {

    uint8_t dpb = 0;

    _flash_transaction_begin();
    _flash_command(FLASH_READ_DPB);
    _flash_address_32bit(addr);

    _flash_rx_end(&dpb, 1);

    return (bool)dpb;
}

static void _erase_spb(void) {

    while (!_write_enable())
        ;

    _flash_transaction_begin();

    _flash_command_end(FLASH_ERASE_SPB);

    while (_flash_busy())
        ;

    while (_read_status() & WRITE_ENABLE_LATCH)
        ;

    if (_read_security() & ERASE_FAIL) {
        /// TODO: Handle error.
    }

    if (_read_security() & PROGRAM_FAIL) {
        /// TODO: Handle error.
    }
}

static void _gang_block_unlock(void) {

    while (!_write_enable())
        ;

    _flash_transaction_begin();

    _flash_command_end(FLASH_GANG_BLOCK_UNLOCK);

    while (_flash_busy())
        ;

    while (_read_status() & WRITE_ENABLE_LATCH)
        ;
}

static void _write_disable(void) {

    _flash_transaction_begin();

    _flash_command_end(FLASH_WRITE_DISABLE);

    while (_read_status() & WRITE_ENABLE_LATCH)
        ;
}

static void _flash_transaction_begin(void) {

    per_spi_chip_format(FLASH_SPI, FLASH_SPI_DATA_FORMAT, SPI_FLASH_CS, true);
}

static void _flash_command(uint8_t cmd) { _flash_tx(&cmd, 1); }

static void _flash_command_end(uint8_t cmd) {

    _flash_tx_end(&cmd, 1);
}

/// TODO: Maybe typedef flash_address.
static void _flash_address(uint32_t address) {

    uint8_t addr[3];

    _flash_pack_address(address, addr);
    _flash_tx(addr, sizeof(addr));
}

static void _flash_address_end(uint32_t address) {

    uint8_t addr[3];

    _flash_pack_address(address, addr);
    _flash_tx_end(addr, sizeof(addr));
}

static void _flash_address_32bit(uint32_t address) {

    uint8_t addr[4];

    _flash_pack_address_32bit(address, addr);
    _flash_tx(addr, sizeof(addr));
}

static void _flash_pack_address(uint32_t address, uint8_t addr[3]) {

    addr[0] = (uint8_t)(address >> 16);
    addr[1] = (uint8_t)(address >> 8);
    addr[2] = (uint8_t)address;
}

static void _flash_pack_address_32bit(uint32_t address, uint8_t addr[4]) {

    addr[0] = (uint8_t)(address >> 24);
    addr[1] = (uint8_t)(address >> 16);
    addr[2] = (uint8_t)(address >> 8);
    addr[3] = (uint8_t)address;
}

static void _flash_tx(uint8_t *p_tx, uint32_t len) {

    /// TODO: SPI_1 needs mutex to prevent DSP and flash access collision.
    if (!p_tx || len == 0) {
        return;
    }

    per_spi_transfer_blocking(FLASH_SPI, p_tx, NULL, len);

}

static void _flash_tx_end(uint8_t *p_tx, uint32_t len) {

    if (!p_tx || len == 0) {
        return;
    }

    per_spi_transfer_blocking_end(FLASH_SPI, p_tx, NULL, len,
                                  FLASH_SPI_DATA_FORMAT, SPI_FLASH_CS);
}

static void _flash_rx(uint8_t *p_rx, uint32_t len) {

    /// TODO: SPI_1 needs mutex to prevent DSP and flash access collision.
    if (!p_rx || len == 0) {
        return;
    }

    per_spi_transfer_blocking(FLASH_SPI, NULL, p_rx, len);
}

static void _flash_rx_end(uint8_t *p_rx, uint32_t len) {

    if (!p_rx || len == 0) {
        return;
    }

    per_spi_transfer_blocking_end(FLASH_SPI, NULL, p_rx, len,
                                  FLASH_SPI_DATA_FORMAT, SPI_FLASH_CS);
}
