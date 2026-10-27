#pragma once
#include "ft.h"

typedef struct {
	u8   instance;
	u8   int_channel;
#if 0
	bool register_int;
#endif
} spi_config_t;

/**
 * @brief   Sets data format
 */
enum spi_format {
	SPI_LCD_FORMAT = 0,
	SPI_FLASH_FORMAT = 0,
	SPI_DSP_BOOT_FORMAT = 1,
	SPI_DSP_COMMAND_FORMAT = 2,
};

enum irq_mode {
	BLOCKING = 0,
	INTERRUPT = 1
};

typedef void (*spi_tx_cb_t)(void);
typedef void (*spi_rx_cb_t)(void);

void per_spi2_init(const spi_config_t *config);

#if 0
void per_spi2_set_callbacks(u8 instance, spi_tx_cb_t tx_cb, spi_rx_cb_t rx_cb);
#endif

void per_spi2_terminate(u8 instance);

void per_spi2_select(u8 instance, enum spi_format fmt, u8 cs, bool cshold);

bool per_spi2_tx_blocking(u8 instance, const u8 *tx_buf, u32 tx_len);

bool per_spi2_rx_blocking(u8 instance, u8 *rx_buf, u32 rx_len);

#if 0
enum irq_mode per_spi2_irq_mode(u8 instance, enum irq_mode mode);
#endif

#ifdef FREETRIBE_FREERTOS
void per_spi2_lock(u8 instance);
void per_spi2_unlock(u8 instance);
#endif
