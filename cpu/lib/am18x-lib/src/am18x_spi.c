#include "am18x_spi.h"

void spi_set_spifmt(SPI_con_t *con, uint8_t reg_index, uint32_t freq_hz)
{
	const uint32_t SPI_MODULE_HZ = 300000000u / 2u;
	uint32_t prescale = (SPI_MODULE_HZ / freq_hz) - 1u;

	con->SPIFMT[reg_index] = SPIFMT_PRESCALE_VAL(prescale)
	                       | SPIFMT_CHARLEN_VAL(8)
	                       | SPIFMT_PHASE_delayed;
}

void spi_set_spidat1_control(SPI_con_t *con, uint8_t dfsel, uint8_t cs,
                             bool cshold, uint32_t extra_flags)
{
	vuint8_t *dat1 = (vuint8_t *)&con->SPIDAT1;
	uint32_t control = SPIDAT1_DFSEL_VAL(dfsel)
	                 | SPIDAT1_CSNR_VAL(cs ^ (con->SPIDEF & SPIDEF_CSDEF_MASK))
	                 | (cshold ? SPIDAT1_CSHOLD_hold : SPIDAT1_CSHOLD_deactivate)
	                 | (extra_flags & SPIDAT1_WDEL_MASK);

	// Little-endian bytes 3 and 2 are control; bytes 1 and 0 are TXDATA.
	dat1[3] = (uint8_t)(control >> 24);
	dat1[2] = (uint8_t)(control >> 16);
}
