#ifndef AM18X_SPI_H
#define AM18X_SPI_H

#include <stdbool.h>

#include "am18x_map.h"

/* Configures an 8-bit, delayed-phase format using the 150 MHz SPI module clock. */
void spi_set_spifmt(SPI_con_t *con, uint8_t reg_index, uint32_t freq_hz);

/* cs is a bitmask of chip selects to invert relative to SPIDEF.
 * extra_flags accepts SPIDAT1_WDEL_MASK; zero clears WDEL.
 * TXDATA is never written here. */
void spi_set_spidat1_control(SPI_con_t *con, uint8_t dfsel, uint8_t cs,
                             bool cshold, uint32_t extra_flags);

#endif /* AM18X_SPI_H */
