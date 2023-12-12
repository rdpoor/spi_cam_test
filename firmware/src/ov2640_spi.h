/**
 * @file ov2640_spi.h
 *
 * MIT License
 *
 * Copyright (c) 2023 BrainChip, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @brief Interface to the OV2640 camera via the SPI bus (data channel)
 */

#ifndef _OV2640_SPI_H_
#define _OV2640_SPI_H_

// *****************************************************************************
// Includes

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// *****************************************************************************
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

// *****************************************************************************
// Public declarations

bool ov2640_spi_read_byte(uint8_t addr, uint8_t *data);

bool ov2640_spi_write_byte(uint8_t addr, uint8_t data);

/**
 * @brief Write one byte, read rx_buflen bytes
 *
 * Note: this is a specialized function for reading a block of bytes from
 * the OV2640 image buffer.
 */
bool ov2640_spi_read_bytes(uint8_t command, uint8_t *rx_buf, size_t rx_buflen);

bool ov2640_spi_set_bit(uint8_t addr, uint8_t bitmask);

bool ov2640_spi_clear_bit(uint8_t addr, uint8_t bitmask);

bool ov2640_spi_test_bit(uint8_t addr, uint8_t bitmask, bool *value);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _OV2640_SPI_H_ */
