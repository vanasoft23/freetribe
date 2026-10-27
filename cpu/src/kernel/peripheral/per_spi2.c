/**
 * @file    per_spi2.c
 *
 * @brief   SPI peripheral driver.
 *
 * @author  vanasoft23
 */

#include "ft.h"

#ifdef FREETRIBE_FREERTOS
#include <FreeRTOS.h>
#include <semphr.h>
#endif

#if 0
#include <am18x_aintc.h>
#endif
#include <am18x_spi.h>
#include <am18x_map.h>

#if 0
#include "per_aintc.h"
#endif
#include "per_spi2.h"

/*--- Board Facts ---*/

static const u32 BASE_ADDRS[] = { SPI0_BASE, SPI1_BASE };
static const u32 PIN_FUNC[] = {
	SPIPC0_CLKFUN_spi | SPIPC0_SIMOFUN_spi | SPIPC0_SOMIFUN_spi | SPIPC0_SCS0FUN_spi(0),
	SPIPC0_CLKFUN_spi | SPIPC0_SIMOFUN_spi | SPIPC0_SOMIFUN_spi | SPIPC0_SCS0FUN_spi(0) | SPIPC0_SCS0FUN_spi(1) //| SPIPC0_ENAFUN_spi
};

#if 0
/*--- Interrupt Mode ---*/

static void _isr_0(void);
static void _isr_1(void);
static void (*const ISR[2])(void) = { &_isr_0, &_isr_1 };
static const u32 SYS_INT[] = { AINTC_SPI0_INT, AINTC_SPI1_INT };
struct tx {
	u8 *buf;
	u32 len;
	spi_tx_cb_t tx_callback;
};
struct rx {
	u8 *buf;
	u32 len;
	spi_rx_cb_t rx_callback;
};
static struct tx s_tx[2];
static struct rx s_rx[2];
#endif

/*---*/

static bool _wait_txbuf_empty(SPI_con_t *con);
static bool _wait_rx(SPI_con_t *con, u8 *rx_byte);

#ifdef FREETRIBE_FREERTOS
SemaphoreHandle_t s_spi1_mutex;
#endif

/*---*/

void per_spi2_init(const spi_config_t *config)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[config->instance];

	con->SPIGCR0 = FIELD_SET(0, SPIGCR0_RESET_MASK, SPIGCR0_RESET_held);
	con->SPIGCR0 = FIELD_SET(0, SPIGCR0_RESET_MASK, SPIGCR0_RESET_released);

	u32 msk, val;
	msk = SPIGCR1_MASTER_MASK   | SPIGCR1_CLKMOD_MASK;
	val = SPIGCR1_MASTER_master | SPIGCR1_CLKMOD_master;
	con->SPIGCR1 = FIELD_SET(0, msk, val);

	con->SPIPC0 = PIN_FUNC[config->instance];

	// Configure hard-coded data rate, charlen, shiftdir, phase, polarity...
	if (config->instance == 0) {
		spi_set_spifmt(con, SPI_LCD_FORMAT, 30000000u);
	} else {
		spi_set_spifmt(con, SPI_FLASH_FORMAT, 37500000u);
		spi_set_spifmt(con, SPI_DSP_BOOT_FORMAT, 50000000u);
		spi_set_spifmt(con, SPI_DSP_COMMAND_FORMAT, 50000000u);
	}

#if 0
	if (config->register_int) {
		per_aintc_register_int(
			config->int_channel,
			SYS_INT[config->instance],
			ISR[config->instance]
		);
		con->SPILVL =
			SPILVL_DLENERRLVL_INT1 |
			SPILVL_PARERRLVL_INT1 |
			SPILVL_DESYNCLVL_INT1 |
			SPILVL_BITERRLVL_INT1 |
			SPILVL_RXINTLVL_INT1 |
			SPILVL_RXINTLVL_INT1 |
			SPILVL_TXINTLVL_INT1;
		// interrupts will be unmasked/masked as pleased
	}
#endif

	// Enable SPI communication by setting SPIGCR1.ENABLE to 1
	con->SPIGCR1 = FIELD_SET(con->SPIGCR1,
	               SPIGCR1_ENABLE_MASK,
	               SPIGCR1_ENABLE_enabled);

	// 10. Setup and enable the DMA for SPI data handling and then enable the DMA servicing for the SPI data
	// requests by setting the SPIINT0.DMAREQEN to 1......

#ifdef FREETRIBE_FREERTOS
	// @FIXME: Must also free this! (or use static memory?)
	// s_spi1_mutex = xSemaphoreCreateMutex();
	// if (!s_spi1_mutex)
	// 	PANIC(PANIC_FREERTOS);
#endif

}

void per_spi2_terminate(u8 instance)
{
#if 0
	per_aintc_unregister_int(SYS_INT[instance]);
#endif
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];
	con->SPIGCR0 = FIELD_SET(0, SPIGCR0_RESET_MASK, SPIGCR0_RESET_held);

#ifdef FREETRIBE_FREERTOS
	// @FIXME: deallocate
#endif
}

/*--- DMA ---*/

void per_spi2_enable_dma(u8 instance)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];
	con->SPIINT0 |= SPIINT0_DMAREQEN_enabled;
	////////////
	//////////////
	////////
}

void per_spi2_disable_dma(u8 instance)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];

	////////////
	//////////////
	////////
	con->SPIINT0 |= SPIINT0_DMAREQEN_disabled;
}

/*--- General Operations ---*/

/**
 * @brief   Configures SPIDAT1 register without initiating a transfer.
 */
void per_spi2_select(u8 instance, enum spi_format fmt, u8 cs, bool cshold)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];
	spi_set_spidat1_control(con, fmt, cs, cshold, 0);
}

/*--- Blocking Operations ---*/

bool per_spi2_tx_blocking(u8 instance, const u8 *tx_buf, u32 len)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];

	ASSERT(!(con->SPIINT0 & SPIINT0_TXINTENA_MASK));

	if (!len)
		return true;

	if (NULL == tx_buf) {
		DLOG("%s invalid params", __func__);
		return false;
	}

	// <maybe add the double !SPIBUF->RXEMPTY error check here>
	
	for (u32 i = 0; i < len; i++) {
		if (!_wait_txbuf_empty(con))
			return false;

		con->SPIDAT0 = (u16)(tx_buf[i]);

		u8 discard;
		if (!_wait_rx(con, &discard))
			return false;
	}

	return true;
}

bool per_spi2_rx_blocking(u8 instance, u8 *rx_buf, u32 len)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];

	ASSERT(!(con->SPIINT0 & SPIINT0_TXINTENA_MASK));

	u8 *ptr = rx_buf;
	for (u32 i = 0; i < len; i++) {

		// <maybe add the double !SPIBUF->RXEMPTY error check here>

		con->SPIDAT0 = 0x00000000UL; // request one byte

		if (!_wait_rx(con, ptr))
			return false;

		ptr++;
	}
	return true;
}

/*--- Blocking Helpers ---*/

static bool _wait_txbuf_empty(SPI_con_t *con)
{
	int retries = 1000;
	do {
		if (con->SPIFLG & SPIFLG_TXINTFLG_MASK)
			return true;
		retries--;
	} while (retries > 0);
	DLOG("%s _wait_tx_buf_empty timed out", __func__);
	return false;
}

static bool _wait_rx(SPI_con_t *con, u8 *rx_byte)
{
	int retries = 1000;
	do {
		u32 spibuf = con->SPIBUF;
		if (!(spibuf & SPIBUF_RXEMPTY_MASK)) {
			*rx_byte = (u8)spibuf;
			return true;
		}
		retries--;
	} while (retries > 0);
	DLOG("%s _wait_rx_consume timed out", __func__);
	return false;
}

#if 0
/*----- Interrupts ---------------------------------------------------*/

static enum irq_mode s_irq_mode = BLOCKING;

void per_spi2_set_callbacks(u8 instance, spi_tx_cb_t tx_cb, spi_rx_cb_t rx_cb)
{
	s_tx[instance].tx_callback = tx_cb;
	s_rx[instance].rx_callback = rx_cb;
}

enum irq_mode per_spi2_irq_mode(u8 instance, enum irq_mode mode)
{
	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];
	enum irq_mode old_mode = s_irq_mode;
	if (mode == BLOCKING) {
		con->SPIINT0 = // mask interrupts
			SPIINT0_DLENERRENA_disabled |
			SPIINT0_PARERRENA_disabled |
			SPIINT0_DESYNCENA_disabled |
			SPIINT0_BITERRENA_disabled |
			SPIINT0_RXINTENA_disabled |
			SPIINT0_TXINTENA_disabled;
	} else {     // INTERRUPT
		con->SPIINT0 = // unmask interrupts
			SPIINT0_DLENERRENA_enabled |
			SPIINT0_PARERRENA_enabled |
			SPIINT0_DESYNCENA_enabled |
			SPIINT0_BITERRENA_enabled |
			SPIINT0_RXINTENA_enabled |
			SPIINT0_TXINTENA_enabled;
	}
	return old_mode;
}

/*--- Interrupt handling ---*/

#define DIR_RX   0
#define DIR_TX   1
static enum irq_mode s_old_irq_mode;

static inline void _handle_irq_tx_empty(u8 instance, SPI_con_t *con)
{
	struct tx *tx = &s_tx[instance];
	if (tx->len == 0)
		return;
	
	con->SPIDAT0 = (u16)(*tx->buf++);
	tx->len--;

	if (tx->len == 0) {
		con->SPIINT0 |= SPIINT0_TXINTENA_disabled;
		ASSERT(tx->tx_callback);
		tx->tx_callback();
	}

}

static inline void _handle_irq_rx_full(u8 instance, SPI_con_t *con)
{
	struct s_trans
	if (s_transfer.dir == DIR_RX) {
		if (rx->len == 0) {
			con->SPIINT0 |= SPIINT0_RXINTENA_disabled;
			ASSERT(rx->rx_callback);
			rx->rx_callback();
		}
	} else { // DIR_TX */
	}

	struct rx *rx = &s_rx[instance];
			// @TODO: handle
	if (rx->len == 0)
		return;

	con->SPIDAT0 = 0x00000000UL; // request one byte
	rx->len--;

	if (rx->len == 0) {
		con->SPIINT0 |= SPIINT0_RXINTENA_disabled;
		ASSERT(rx->rx_callback);
		rx->rx_callback();
	}
}

static inline void _handle_irq_error(u8 instance, SPI_con_t *con)
{
			// @TODO: handle

}

static inline void _isr(u8 instance)
{
	aintc_clear(SYS_INT[instance]);

	SPI_con_t *con = (SPI_con_t*)BASE_ADDRS[instance];

	u8 vector = 0;
	while ((vector = con->INTVEC1)) {
		switch (vector) {

		case INTVEC1_INTVECT1_tx_empty:
			_handle_irq_tx_empty(instance, con);
			break;

		case INTVEC1_INTVECT1_rx_full:
			_handle_irq_rx_full(instance, con);
			break;

		case INTVEC1_INTVECT1_error:
			_handle_irq_error(instance, con);
			break;

		}
	}

}

static void _isr_0(void)
{ _isr(0); }

static void _isr_1(void)
{ _isr(1); }
#endif

/*----- Misc ---------------------------------------------------------*/

/*--- Mutex ---*/

#ifdef FREETRIBE_FREERTOS
void per_spi2_lock(u8 instance)
{
	ASSERT(instance == 1); // you dumb programmer
	xSemaphoreTake(s_spi1_mutex, portMAX_DELAY);
}

void per_spi2_unlock(u8 instance)
{
	ASSERT(instance == 1); // only mutex SPI1
	xSemaphoreGive(s_spi1_mutex);
}
#endif
