/**
 * @file bsp_arducam.h
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
 * @brief platform-dependent interface to ARDUCAM OV2460-based video camera
 */

#ifndef _BSP_ARDUCAM_H_
#define _BSP_ARDUCAM_H_

// *****************************************************************************
// Includes

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// =============================================================================
// C++ compatibility

#ifdef __cplusplus
extern "C" {
#endif

// *****************************************************************************
// Public types and definitions

// *****************************************************************************
// Public declarations

void bsp_arducam_init(void);

// int bsp_arducam_spi_xfer(void *spi, const uint8_t *src, uint8_t *dst,
//                          size_t len);

void bsp_arducam_spi_cs_enable(void);

void bsp_arducam_spi_cs_disable(void);

int bsp_arducam_spi_read(void *spi, uint8_t repeated_tx_data, uint8_t *dst,
                         size_t len);

int bsp_arducam_spi_write(void *spi, const uint8_t *src, size_t len);

unsigned int bsp_arducam_i2c_init(void *i2c, unsigned int baudrate);

int bsp_arducam_i2c_write(void *i2c, uint8_t addr, const uint8_t *src,
                          size_t len, bool nostop);

int bsp_arducam_i2c_read(void *i2c, uint8_t addr, uint8_t *dst, size_t len,
                         bool nostop);

unsigned int bsp_arducam_uart_init(void *uart, unsigned int baudrate);

void bsp_arducam_uart_write(void *uart, const uint8_t *src, size_t len);

void bsp_arducam_sleep_ms(int ms);

// *****************************************************************************
// End of file

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _BSP_ARDUCAM_H_ */
