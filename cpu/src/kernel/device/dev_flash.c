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

/*----- Includes -----------------------------------------------------*/

#include "ft.h"

#include "ft_error.h"
#include "per_spi2.h"
#include "dev_flash.h"
// #include "dev_delay.h"

/*----- Macros -------------------------------------------------------*/

#define SPI1        1
#define CHIP_SELECT 0x1

/// TODO: typedef enum {} t_flash_command;
#define WRITE_IN_PROGRESS 0x01
#define WRITE_ENABLE_LATCH 0x02
#define PROGRAM_FAIL 0x20
#define ERASE_FAIL 0x40

#define FLASH_PAGE_PROGRAM   0x02
#define FLASH_READ           0x03
#define FLASH_WRITE_DISABLE  0x04
#define FLASH_WRITE_ENABLE   0x06
#define FLASH_SECTOR_ERASE   0x20
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

static u8 g_sector_buffer[SECTOR_LENGTH] = {0};

/*----- Static function prototypes -----------------------------------*/

static void _acquire_spi_bus(void);
static void _release_spi_bus(void);
static void _read_locked(u32 src, u8 *p_dest, u32 len);
static void _flash_chip_select(bool cs_hold);
static void _flash_command(u8 cmd);
static void _flash_address(u32 address);
static void _flash_address_32bit(u32 address);
static void _flash_tx(u8 *p_tx, u32 len);
static void _flash_rx(u8 *p_rx, u32 len);
static void _sector_erase(u32 sector_addr);
static void _sector_write(u32 dest, u8 *p_src);
static void _page_program(u32 dest, u8 *p_src);
static bool _write_enable(void);
static u8   _read_status(void);
static u8   _read_security(void);
static u8   _read_reg_byte(u8 cmd);
static u16  _read_reg_short(u8 cmd);
static u8   _read_config(void);
static u16  _read_lock(void);
static u8   _read_spb_lock(void);
static bool _read_spb(u32 addr);
static bool _read_dpb(u32 addr);
static void _erase_spb(void);
static void _write_disable(void);
static void _gang_block_unlock(void);
static bool _flash_busy(void);

/*----- Extern variable definitions ----------------------------------*/

/*----- Extern function implementations ------------------------------*/

/** @returns true on success, false on failure */
bool dev_flash_init(void)
{
	// ////////////////
	// HWREG(SOC_SPI_1_REGS + SPI_SPIGCR0) = 0;
	// dev_delay_block_us(5);
	// HWREG(SOC_SPI_1_REGS + SPI_SPIGCR0) |= 1;
	// HWREG(SOC_SPI_1_REGS + SPI_SPIGCR1) = 3; // confirmed
	// HWREG(SOC_SPI_1_REGS + SPI_SPIPC(0)) = 0xe00; // 0x00000F03 in custom SBL
	// HWREG(SOC_SPI_1_REGS + SPI_SPIPC(1)) = 3;
	// HWREG(SOC_SPI_1_REGS + SPI_SPIFMT(0)) = 0x00010308;
	// HWREG(SOC_SPI_1_REGS + SPI_SPIGCR1) |= SPI_SPIGCR1_ENABLE;
	// dev_delay_block_us(5);
	// ////////////////

	return true;
}

void dev_flash_read(u32 src, u8 *p_dest, u32 len)
{
	_acquire_spi_bus();
	_read_locked(src, p_dest, len);
	_release_spi_bus();
}


void dev_flash_write(u32 dest, u8 *p_src, u32 len)
{
	if (len == 0)
		return;

	_acquire_spi_bus();

	u16 sector_offset = dest & SECTOR_OFFSET_MASK;

	// If destination address not sector aligned.
	if (sector_offset) {

		// Start address of target sector.
		dest -= sector_offset;

		// Number of bytes to copy.
		u16 copy_length = SECTOR_LENGTH - sector_offset;
		if (copy_length > len) {
			copy_length = len;
		}

		// Read target sector.
		_read_locked(dest, g_sector_buffer, SECTOR_LENGTH);

		// Copy source data to sector buffer.
		memcpy(g_sector_buffer + sector_offset, p_src, copy_length);

		_sector_erase(dest);

		_sector_write(dest, g_sector_buffer);

		p_src += copy_length;
		dest += SECTOR_LENGTH;
		len -= copy_length;
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

	_release_spi_bus();
}

bool dev_flash_verify(u32 flash_addr, u8 *p_ram_data, u32 len)
{
	_acquire_spi_bus();

	bool verified = true;
	u16 i;

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

	_release_spi_bus();
	return verified;
}

void dev_flash_erase(u32 address, u32 len)
{
	if (len == 0)
		return;

	_acquire_spi_bus();

	u16 sector_offset = address & SECTOR_OFFSET_MASK;

	// If destination address not sector aligned.
	if (sector_offset) {

		// Start address of target sector.
		address -= sector_offset;

		// Number of bytes to set.
		u16 set_length = SECTOR_LENGTH - sector_offset;
		if (set_length > len) {
			set_length = len;
		}

		// Read target sector.
		dev_flash_read(address, g_sector_buffer, SECTOR_LENGTH);

		memset(g_sector_buffer + sector_offset, 0xff, set_length);

		_sector_erase(address);

		_sector_write(address, g_sector_buffer);

		address += SECTOR_LENGTH;
		len -= set_length;
	}

	while (len >> SECTOR_INDEX_SHIFT) {

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

	_release_spi_bus();
}

void dev_flash_unlock(void)
{
	_acquire_spi_bus();
	_gang_block_unlock();
	_release_spi_bus();
}

/*----- Static function implementations ------------------------------*/

static void _acquire_spi_bus(void)
{
#ifdef FREETRIBE_FREERTOS
	per_spi2_lock(SPI1);
#endif
}

static void _release_spi_bus(void)
{
#ifdef FREETRIBE_FREERTOS
	per_spi2_unlock(SPI1);
#endif
}


static void _read_locked(u32 src, u8 *p_dest, u32 len)
{
	while (_flash_busy())
		;

	_flash_chip_select(true);

	_flash_command(FLASH_READ);

	_flash_address(src);

	_flash_rx(p_dest, len);

	_flash_chip_select(false);
}

void _sector_write(u32 dest, u8 *p_src)
{
	for (u8 i = 0; i < PAGES_PER_SECTOR; i++) {

		_page_program(dest, p_src);

		dest += PAGE_LENGTH;
		p_src += PAGE_LENGTH;
	}
}

void _sector_erase(u32 sector_addr)
{
	while (!_write_enable())
		;

	_flash_chip_select(true);

	_flash_command(FLASH_SECTOR_ERASE);

	_flash_address(sector_addr);

	_flash_chip_select(false);

	while (_flash_busy())
		;

	while (_read_status() & WRITE_ENABLE_LATCH)
		;

	if (_read_security() & ERASE_FAIL) {
		/// TODO: Handle error.
	}
}

void _page_program(u32 dest, u8 *p_src)
{
	while (!_write_enable())
		;

	_flash_chip_select(true);

	_flash_command(FLASH_PAGE_PROGRAM);

	_flash_address(dest);

	_flash_tx(p_src, PAGE_LENGTH);

	_flash_chip_select(false);

	while (_flash_busy())
		;

	while (_read_status() & WRITE_ENABLE_LATCH)
		;

	if (_read_security() & PROGRAM_FAIL) {
		/// TODO: Handle error.
	}
}

bool _flash_busy(void) {
	return _read_status() & WRITE_IN_PROGRESS;
}

bool _write_enable(void)
{
	_flash_chip_select(true);
	_flash_command(FLASH_WRITE_ENABLE);
	_flash_chip_select(false);

	return _read_status() & WRITE_ENABLE_LATCH;
}

u8 _read_reg_byte(u8 cmd)
{
	u8 flash_reg = 0;

	_flash_chip_select(true);
	_flash_command(cmd);

	_flash_rx(&flash_reg, 1);
	_flash_chip_select(false);

	return flash_reg;
}

u16 _read_reg_short(u8 cmd)
{
	u8 flash_reg[2] = {0};

	_flash_chip_select(true);
	_flash_command(cmd);

	_flash_rx(&flash_reg[0], 2);
	_flash_chip_select(false);

	return (flash_reg[0] << 8) | flash_reg[1];
}

u8 _read_status(void) { return _read_reg_byte(FLASH_READ_STATUS); }

u8 _read_config(void) { return _read_reg_byte(FLASH_READ_CONFIG); }

u8 _read_security(void) { return _read_reg_byte(FLASH_READ_SECURITY); }

u16 _read_lock(void) { return _read_reg_short(FLASH_READ_LOCK); }

u8 _read_spb_lock(void) { return _read_reg_byte(FLASH_READ_SPB_LOCK); }

bool _read_spb(u32 addr)
{
	u8 spb = 0;

	_flash_chip_select(true);
	_flash_command(FLASH_READ_SPB);
	_flash_address_32bit(addr);

	_flash_rx(&spb, 1);
	_flash_chip_select(false);

	return (bool)spb;
}

bool _read_dpb(u32 addr)
{
	u8 dpb = 0;

	_flash_chip_select(true);
	_flash_command(FLASH_READ_DPB);
	_flash_address_32bit(addr);

	_flash_rx(&dpb, 1);
	_flash_chip_select(false);

	return (bool)dpb;
}

void _erase_spb(void)
{
	while (!_write_enable())
		;

	_flash_chip_select(true);

	_flash_command(FLASH_ERASE_SPB);

	_flash_chip_select(false);

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

void _gang_block_unlock(void)
{
	while (!_write_enable())
		;

	_flash_chip_select(true);

	_flash_command(FLASH_GANG_BLOCK_UNLOCK);

	_flash_chip_select(false);

	while (_flash_busy())
		;

	while (_read_status() & WRITE_ENABLE_LATCH)
		;
}

void _write_disable(void)
{
	_flash_chip_select(true);

	_flash_command(FLASH_WRITE_DISABLE);

	_flash_chip_select(false);

	while (_read_status() & WRITE_ENABLE_LATCH)
		;
}

void _flash_chip_select(bool cs_hold)
{
	// if (!cs_hold)
	// 	dev_delay_cycles(4);
	per_spi2_select(SPI1, SPI_FLASH_FORMAT, CHIP_SELECT, cs_hold);
	// if (cs_hold)
	// 	dev_delay_cycles(4);
}

void _flash_command(u8 cmd)
{
	_flash_tx(&cmd, 1);
}

void _flash_address(u32 address)
{
	// Source address array.
	u8 addr[3];

	// 24 bit flash address as 3 bytes.
	addr[0] = (u8)(address >> 16);
	addr[1] = (u8)(address >> 8);
	addr[2] = (u8) address;

	_flash_tx(addr, 3);
}

void _flash_address_32bit(u32 address)
{
	// Source address array.
	u8 addr[4];

	// 32 bit flash address as 4 bytes.
	addr[0] = (u8)(address >> 24);
	addr[1] = (u8)(address >> 16);
	addr[2] = (u8)(address >> 8);
	addr[3] = (u8) address;

	_flash_tx(addr, 4);
}

void _flash_tx(u8 *p_tx, u32 len)
{
	bool success = per_spi2_tx_blocking(SPI1, p_tx, len);
	if (!success)
		PANIC(PANIC_GENERIC);
}

void _flash_rx(u8 *p_rx, u32 len)
{
	bool success = per_spi2_rx_blocking(SPI1, p_rx, len);
	if (!success)
		PANIC(PANIC_GENERIC);
}
