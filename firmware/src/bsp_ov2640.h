/**
 * @file bsp_ov2640.h
 *
 * MIT License
 *
 * Copyright (c) 2023 R. Dunbar Poor
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
 * @brief platform-dependent interface to OV2640 video camera module
 */

#ifndef _BSP_OV2640_H_
#define _BSP_OV2640_H_

// *****************************************************************************
// Includes

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

// *****************************************************************************
// Public declarations

int spi_write_read_blocking(void *spi, const uint8_t *src, uint8_t *dst,
                            size_t len);

int spi_read_blocking(void *spi, uint8_t repeated_tx_data, uint8_t *dst,
                      size_t len);

int spi_write_blocking(void *spi, const uint8_t *src, size_t len);

unsigned int i2c_init (void *i2c, unsigned int baudrate);

int i2c_write_blocking(void *i2c, uint8_t addr, const uint8_t *src, size_t len,
                       bool nostop);

int i2c_read_blocking(void *i2c, uint8_t addr, uint8_t *dst, size_t len,
                      bool nostop);

unsigned int uart_init (void *uart, unsigned int baudrate);


void uart_write_blocking (void *uart, const uint8_t *src, size_t len);

void sleep_ms(int ms);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _BSP_OV2640_H_ */
